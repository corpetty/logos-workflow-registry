#include "registry_bridge_lp.h"

#include "logos_call_error.h"
#include "logos_lp_client.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

// How long to wait for one module to answer an introspection call.
//
// This is NOT a tuning knob, it is the difference between a usable palette and
// a frozen UI. The registry introspects every module it finds ON DISK, and a
// module that is installed but not LOADED has no listener: the protocol
// default blocks the caller for 20 SECONDS before failing, once per such
// module, while the canvas waits on getNodeTypeDefinitions(). Two unloaded
// modules were enough to make the canvas look hung.
//
// A loaded module answers this in single-digit milliseconds, so a short
// deadline costs a live module nothing and turns a dead one into a fast,
// correctly-reported "installed, not live".
constexpr int kIntrospectTimeoutMs = 1500;

// Qt/LIDL type names -> the port types the canvas draws and the engine
// type-checks. Anything unrecognised becomes "object", the permissive case:
// the canvas will still wire it, it just won't pretty-print it.
std::string mapType(const std::string& raw)
{
    if (raw == "QString" || raw == "std::string" || raw == "tstr" ||
        raw == "string" || raw == "char*" || raw == "const char*")
        return "string";

    if (raw == "int" || raw == "qint32" || raw == "qint64" || raw == "int32_t" ||
        raw == "int64_t" || raw == "uint" || raw == "double" || raw == "float" ||
        raw == "float64" || raw == "number")
        return "number";

    if (raw == "bool" || raw == "boolean")
        return "boolean";

    if (raw == "QByteArray" || raw == "bytes" || raw == "bstr")
        return "bytes";

    if ((!raw.empty() && raw.front() == '[') ||
        raw.rfind("QList", 0) == 0 || raw.rfind("QVector", 0) == 0 ||
        raw == "QVariantList" || raw == "QJsonArray" || raw == "QStringList" ||
        raw == "array")
        return "array";

    return "object";
}

// Lifecycle and plumbing members that are never workflow steps.
bool isInternalMethod(const std::string& name)
{
    static const std::vector<std::string> kInternal = {
        "initLogos", "eventResponse", "name", "version",
        "deleteLater", "destroyed", "objectNameChanged",
        "getPluginMethods", "getPluginEvents",
    };
    if (name.empty() || name.front() == '_')
        return true;
    return std::find(kInternal.begin(), kInternal.end(), name) != kInternal.end();
}

// The registry must not offer the workflow stack as workflow steps: executing
// them from a workflow re-enters the very machinery running it.
bool isExcludedModule(const std::string& name)
{
    return name.empty()
        || name.rfind("workflow_", 0) == 0
        || name == "capability_module"
        || name == "core_manager";
}

std::string stringField(const json& obj, const char* key, const std::string& fallback = {})
{
    if (obj.is_object()) {
        auto it = obj.find(key);
        if (it != obj.end() && it->is_string())
            return it->get<std::string>();
    }
    return fallback;
}

std::vector<MethodParam> parseParams(const json& raw)
{
    std::vector<MethodParam> params;
    if (!raw.is_array())
        return params;
    for (const auto& entry : raw) {
        MethodParam p;
        p.name = stringField(entry, "name");
        p.type = mapType(stringField(entry, "type"));
        params.push_back(std::move(p));
    }
    return params;
}

// An lp invoke may hand back a JSON array directly or a STRING holding JSON,
// depending on how the target module was generated. Normalise both rather
// than betting on one.
json asArray(const json& value)
{
    if (value.is_array())
        return value;
    if (value.is_string()) {
        auto parsed = json::parse(value.get<std::string>(), nullptr, /*allow_exceptions=*/false);
        if (!parsed.is_discarded() && parsed.is_array())
            return parsed;
    }
    return json::array();
}

// One installed module directory -> its manifest facts. The .lgx installer
// writes manifest.json; a module staged straight from a Nix build has
// metadata.json instead. Read whichever is present, and fall back to the
// directory name, which is what the host uses as the module name when it
// stages a plugin — better a live-introspected module with a guessed version
// than a missing palette entry.
struct ManifestFacts {
    std::string name;
    std::string version = "0.0.0";
    std::string type    = "core";
};

ManifestFacts readManifest(const fs::path& dir)
{
    ManifestFacts facts;

    for (const char* candidate : { "manifest.json", "metadata.json" }) {
        const fs::path file = dir / candidate;
        std::error_code ec;
        if (!fs::exists(file, ec))
            continue;

        std::ifstream in(file);
        if (!in)
            continue;

        auto parsed = json::parse(in, nullptr, /*allow_exceptions=*/false);
        if (parsed.is_discarded())
            continue;

        const std::string name = stringField(parsed, "name");
        if (name.empty())
            continue;

        facts.name    = name;
        facts.version = stringField(parsed, "version", "0.0.0");
        facts.type    = stringField(parsed, "type", "core");
        return facts;
    }

    facts.name = dir.filename().string();
    return facts;
}

// Every modules root to scan: the one this module was installed into, plus
// anything named by LOGOS_WORKFLOW_MODULE_DIRS. A host can have more than one
// root (basecamp bundles modules in the app AND installs them under the user
// directory) and a module can only derive its own, so the extra roots are the
// escape hatch for the ones it cannot see.
std::vector<fs::path> moduleRoots(const std::string& ownRoot)
{
#ifdef _WIN32
    constexpr char kSep = ';';
#else
    constexpr char kSep = ':';
#endif

    std::vector<fs::path> roots;
    if (!ownRoot.empty())
        roots.emplace_back(ownRoot);

    if (const char* extra = std::getenv("LOGOS_WORKFLOW_MODULE_DIRS")) {
        std::string spec(extra);
        size_t start = 0;
        while (start <= spec.size()) {
            const size_t end = spec.find(kSep, start);
            const std::string one =
                spec.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (!one.empty())
                roots.emplace_back(one);
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
    }

    return roots;
}

} // namespace

RegistryBridgeLp::RegistryBridgeLp(std::string origin)
    : m_origin(std::move(origin))
{}

std::vector<DiscoveredModule> RegistryBridgeLp::discoverModules(const std::string& modulesDir)
{
    std::vector<DiscoveredModule> modules;

    const std::vector<fs::path> roots = moduleRoots(modulesDir);
    if (roots.empty()) {
        std::fprintf(stderr,
            "[workflow_registry] no modules directory (running outside a host?) "
            "— palette will hold built-ins only\n");
        return modules;
    }

    std::error_code ec;

    // Sorted, so the palette order is stable between runs rather than
    // following directory-iteration order.
    std::vector<fs::path> entries;
    for (const fs::path& root : roots) {
        if (!fs::is_directory(root, ec)) {
            std::fprintf(stderr, "[workflow_registry] modules directory does not exist: %s\n",
                         root.string().c_str());
            continue;
        }
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (entry.is_directory())
                entries.push_back(entry.path());
        }
    }
    std::sort(entries.begin(), entries.end());

    std::set<std::string> seen;

    for (const fs::path& dir : entries) {
        const ManifestFacts facts = readManifest(dir);
        if (isExcludedModule(facts.name))
            continue;
        // The same module can sit in two roots (bundled and user-installed);
        // the palette must list it once.
        if (!seen.insert(facts.name).second)
            continue;

        DiscoveredModule mod;
        mod.name    = facts.name;
        mod.version = facts.version;
        mod.type    = facts.type;

        // Ask the module itself what it exposes. A module that is installed
        // but not loaded fails the call — it stays in the list as a non-live
        // entry so the canvas can grey it out rather than pretend it isn't
        // there.
        logos::LpClient client(facts.name, m_origin);

        logos::CallError err;
        const json methodsRaw =
            asArray(client.invoke("getPluginMethods", json::array(), &err, kIntrospectTimeoutMs));

        // Nothing answered — installed but not loaded. Skip the second call
        // rather than spend the deadline again on a module already known to be
        // silent.
        if (!err.code.empty()) {
            std::fprintf(stderr, "[workflow_registry] %s installed, not live (%s)\n",
                         facts.name.c_str(), err.message.c_str());
            modules.push_back(std::move(mod));
            continue;
        }

        for (const auto& entry : methodsRaw) {
            const std::string methodName = stringField(entry, "name");
            if (isInternalMethod(methodName))
                continue;

            MethodInfo method;
            method.name       = methodName;
            method.returnType = mapType(stringField(entry, "returnType"));
            method.parameters = parseParams(entry.is_object() && entry.contains("parameters")
                                                ? entry.at("parameters")
                                                : json::array());
            mod.methods.push_back(std::move(method));
        }

        // Events became a first-class part of the module surface after this
        // registry was written; they are what a trigger node subscribes to.
        const json eventsRaw =
            asArray(client.invoke("getPluginEvents", json::array(), &err, kIntrospectTimeoutMs));

        for (const auto& entry : eventsRaw) {
            const std::string eventName = stringField(entry, "name");
            if (eventName.empty())
                continue;

            EventInfo event;
            event.name       = eventName;
            event.parameters = parseParams(entry.is_object() && entry.contains("parameters")
                                               ? entry.at("parameters")
                                               : json::array());
            mod.events.push_back(std::move(event));
        }

        mod.isLive = !mod.methods.empty() || !mod.events.empty();

        std::fprintf(stderr, "[workflow_registry] %s %s %zu methods, %zu events\n",
                     facts.name.c_str(),
                     mod.isLive ? "live:" : "installed, not live:",
                     mod.methods.size(), mod.events.size());

        modules.push_back(std::move(mod));
    }

    return modules;
}
