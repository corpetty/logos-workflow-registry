#pragma once

#include <string>
#include <vector>

/**
 * Pure-C++ seam over the Logos runtime, so WorkflowRegistryImpl carries no
 * Qt. RegistryBridgeQt is the real implementation; a test can substitute a
 * fake without a running host.
 */

struct MethodParam {
    std::string name;
    std::string type;   ///< normalized port type: string|number|boolean|object|array|bytes
};

struct MethodInfo {
    std::string name;
    std::string returnType;   ///< normalized port type
    std::vector<MethodParam> parameters;
};

struct EventInfo {
    std::string name;
    std::vector<MethodParam> parameters;
};

struct DiscoveredModule {
    std::string name;
    std::string version;
    std::string type;      ///< "core", "ui_qml", ...
    bool        isLive = false;   ///< true when the module answered introspection
    std::vector<MethodInfo> methods;
    std::vector<EventInfo>  events;
};

class RegistryBridge {
public:
    virtual ~RegistryBridge() = default;

    /**
     * Enumerate installed modules and introspect each one.
     *
     * @param modulesDir the host's modules directory — the PARENT of this
     *        module's own install directory (LogosModuleContext::modulePath()).
     *        Empty when running outside a host, in which case the disk half is
     *        skipped and the result is empty rather than an error.
     *
     * A host may have more than one modules root — basecamp reads both a
     * user-writable directory (where the package manager installs) and one
     * bundled inside the app. A module can only derive the root it was itself
     * installed into, so modules in the OTHER root are invisible here. Set
     * LOGOS_WORKFLOW_MODULE_DIRS (colon-separated, ';' on Windows) to name
     * extra roots to scan.
     */
    virtual std::vector<DiscoveredModule> discoverModules(const std::string& modulesDir) = 0;
};
