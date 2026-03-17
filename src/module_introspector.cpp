#include "module_introspector.h"
#include "logos_api.h"
#include "logos_api_client.h"

#include <QDebug>
#include <QProcess>
#include <QJsonDocument>
#include <QDir>
#include <QStandardPaths>

ModuleIntrospector::ModuleIntrospector(QObject* parent)
    : QObject(parent)
{
    // Try to locate the `lm` binary
    m_lmPath = QStandardPaths::findExecutable("lm");
    if (m_lmPath.isEmpty()) {
        qDebug() << "[introspector] `lm` binary not found on PATH";
    }
}

QJsonArray ModuleIntrospector::discoverModules(LogosAPI* api)
{
    QJsonArray modules;

    // Primary: introspect via LogosAPI if available
    if (api) {
        modules = introspectViaAPI(api);
    }

    // Fallback: use `lm` CLI tool on plugin binaries
    if (modules.isEmpty() && !m_lmPath.isEmpty()) {
        modules = introspectViaCLI();
    }

    qDebug() << "[introspector] Discovered" << modules.size() << "modules";
    return modules;
}

QJsonArray ModuleIntrospector::introspectViaAPI(LogosAPI* api)
{
    QJsonArray modules;

    // Step 1: Get the list of known plugins via core_manager.
    // The generated CoreManager wrapper (from logos-cpp-sdk) provides
    // getKnownPlugins() which returns a QJsonArray of plugin descriptors,
    // and getPluginMethods(pluginName) which returns method metadata.
    //
    // We access these through the raw LogosAPIClient interface since
    // we don't link the generated wrappers directly.

    auto* coreClient = api->getClient("core_manager");
    if (!coreClient) {
        qDebug() << "[introspector] No core_manager client — trying capability_module";

        // Fallback: try capability_module for older logos-core versions
        auto* capClient = api->getClient("capability_module");
        if (!capClient) {
            qDebug() << "[introspector] No capability_module client either";
            return modules;
        }
    }

    // Query the list of known plugins
    QVariant knownResult;
    if (coreClient) {
        knownResult = coreClient->invokeRemoteMethod(
            QString("core_manager"), QString("getKnownPlugins"), QVariantList());
    }

    QJsonArray knownPlugins;
    if (knownResult.canConvert<QJsonArray>()) {
        knownPlugins = knownResult.toJsonArray();
    } else {
        // Try parsing as a JSON string
        QJsonDocument doc = QJsonDocument::fromJson(knownResult.toString().toUtf8());
        if (doc.isArray()) {
            knownPlugins = doc.array();
        }
    }

    if (knownPlugins.isEmpty()) {
        qDebug() << "[introspector] No known plugins returned from core_manager";
        return modules;
    }

    qDebug() << "[introspector] core_manager reports" << knownPlugins.size() << "known plugins";

    // Step 2: For each known plugin, introspect its methods
    for (const auto& pluginVal : knownPlugins) {
        QString pluginName;

        // The plugin entry might be a string (just the name) or an object
        if (pluginVal.isString()) {
            pluginName = pluginVal.toString();
        } else if (pluginVal.isObject()) {
            pluginName = pluginVal.toObject()["name"].toString();
        }

        if (pluginName.isEmpty()) continue;

        // Skip internal/infrastructure modules
        if (pluginName == "core_manager" || pluginName == "capability_module") {
            continue;
        }

        // Skip our own workflow modules to avoid circular introspection
        if (pluginName.startsWith("workflow_")) {
            continue;
        }

        // Get methods for this plugin via core_manager.getPluginMethods()
        QVariant methodsResult;
        if (coreClient) {
            methodsResult = coreClient->invokeRemoteMethod(
                QString("core_manager"), QString("getPluginMethods"), QVariant(pluginName));
        }

        QJsonArray rawMethods;
        if (methodsResult.canConvert<QJsonArray>()) {
            rawMethods = methodsResult.toJsonArray();
        } else {
            QJsonDocument doc = QJsonDocument::fromJson(methodsResult.toString().toUtf8());
            if (doc.isArray()) rawMethods = doc.array();
        }

        // Build module descriptor
        QJsonObject module;
        module["name"] = pluginName;
        module["version"] = "1.0.0";
        module["isLive"] = true;

        // Process methods — filter and normalize
        QJsonArray methods;
        for (const auto& methodVal : rawMethods) {
            QJsonObject rawMethod = methodVal.toObject();
            QString methodName = rawMethod["name"].toString();

            // Skip lifecycle and internal methods
            if (methodName == "initLogos" || methodName == "eventResponse" ||
                methodName == "name" || methodName == "version" ||
                methodName == "deleteLater" || methodName == "destroyed" ||
                methodName.startsWith("_")) {
                continue;
            }

            QJsonObject method;
            method["name"] = methodName;
            method["returnType"] = mapQtType(rawMethod["returnType"].toString());

            // Process parameters
            QJsonArray params;
            QJsonArray rawParams = rawMethod["parameters"].toArray();
            for (const auto& paramVal : rawParams) {
                QJsonObject rawParam = paramVal.toObject();
                QJsonObject param;
                param["name"] = rawParam["name"].toString();
                param["type"] = mapQtType(rawParam["type"].toString());
                params.append(param);
            }
            method["parameters"] = params;
            methods.append(method);
        }

        if (methods.isEmpty()) {
            qDebug() << "[introspector] Skipping" << pluginName << "(no invokable methods)";
            continue;
        }

        module["methods"] = methods;
        modules.append(module);

        qDebug() << "[introspector] Introspected" << pluginName
                 << ":" << methods.size() << "methods";
    }

    return modules;
}

QJsonArray ModuleIntrospector::introspectViaCLI()
{
    QJsonArray modules;

    if (m_lmPath.isEmpty()) return modules;

    // Scan known plugin directories
    QStringList searchPaths;
    searchPaths << QDir::homePath() + "/.local/share/logos/modules";

    // Also check environment variable
    QString envPath = qEnvironmentVariable("LOGOS_PLUGINS_DIR");
    if (!envPath.isEmpty()) {
        searchPaths.prepend(envPath);
    }

    for (const auto& searchPath : searchPaths) {
        QDir dir(searchPath);
        if (!dir.exists()) continue;

        // Find all *_plugin.so / *_plugin.dylib files
        QStringList filters;
        filters << "*_plugin.so" << "*_plugin.dylib" << "*.so" << "*.dylib";

        auto entries = dir.entryInfoList(filters, QDir::Files, QDir::Name);
        for (const auto& entry : entries) {
            QJsonObject mod = introspectPlugin(entry.absoluteFilePath());
            if (!mod.isEmpty()) {
                modules.append(mod);
            }
        }

        // Also check subdirectories (one level deep)
        auto subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto& subdir : subdirs) {
            QDir sub(subdir.absoluteFilePath());
            auto subEntries = sub.entryInfoList(filters, QDir::Files, QDir::Name);
            for (const auto& entry : subEntries) {
                QJsonObject mod = introspectPlugin(entry.absoluteFilePath());
                if (!mod.isEmpty()) {
                    modules.append(mod);
                }
            }
        }
    }

    return modules;
}

QJsonObject ModuleIntrospector::introspectPlugin(const QString& pluginPath)
{
    QProcess proc;
    proc.setProgram(m_lmPath);
    proc.setArguments({"methods", pluginPath, "--json"});
    proc.start();

    if (!proc.waitForFinished(10000)) {
        qWarning() << "[introspector] lm timed out for" << pluginPath;
        return {};
    }

    if (proc.exitCode() != 0) {
        qWarning() << "[introspector] lm failed for" << pluginPath
                    << ":" << proc.readAllStandardError();
        return {};
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(proc.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "[introspector] Failed to parse lm output for"
                    << pluginPath << ":" << parseError.errorString();
        return {};
    }

    QJsonObject raw = doc.object();
    QJsonObject module;

    // Extract module name from the binary name or metadata
    QString moduleName = raw["name"].toString();
    if (moduleName.isEmpty()) {
        // Derive from filename: my_module_plugin.so -> my_module
        QString filename = QFileInfo(pluginPath).baseName();
        moduleName = filename.replace("_plugin", "");
    }

    module["name"] = moduleName;
    module["version"] = raw["version"].toString("1.0.0");
    module["description"] = raw["description"].toString();
    module["pluginPath"] = pluginPath;
    module["isLive"] = true;

    // Process methods
    QJsonArray methods;
    QJsonArray rawMethods = raw["methods"].toArray();
    for (const auto& methodVal : rawMethods) {
        QJsonObject rawMethod = methodVal.toObject();

        // Skip lifecycle methods
        QString methodName = rawMethod["name"].toString();
        if (methodName == "initLogos" || methodName == "eventResponse" ||
            methodName == "name" || methodName == "version") {
            continue;
        }

        if (!rawMethod["isInvokable"].toBool(false)) continue;

        QJsonObject method;
        method["name"] = methodName;
        method["returnType"] = mapQtType(rawMethod["returnType"].toString());

        QJsonArray params;
        QJsonArray rawParams = rawMethod["parameters"].toArray();
        for (const auto& paramVal : rawParams) {
            QJsonObject rawParam = paramVal.toObject();
            QJsonObject param;
            param["name"] = rawParam["name"].toString();
            param["type"] = mapQtType(rawParam["type"].toString());
            params.append(param);
        }
        method["parameters"] = params;
        methods.append(method);
    }

    module["methods"] = methods;
    return module;
}

QString ModuleIntrospector::mapQtType(const QString& qtType)
{
    QString t = qtType.trimmed();

    // Remove const& qualifiers
    t.remove("const ");
    t.remove("&");
    t = t.trimmed();

    if (t == "QString" || t == "QUrl" || t == "QByteArray") {
        return (t == "QByteArray") ? "bytes" : "string";
    }
    if (t == "int" || t == "double" || t == "float" || t == "qint64" || t == "quint64") {
        return "number";
    }
    if (t == "bool") {
        return "boolean";
    }
    if (t == "QStringList") {
        return "array";
    }
    // QVariant, QVariantMap, QVariantList, QJsonObject, QJsonArray, LogosResult
    return "object";
}

QString ModuleIntrospector::colorForType(const QString& portType)
{
    if (portType == "string")  return "#4caf50";  // green
    if (portType == "number")  return "#ff9800";  // orange
    if (portType == "boolean") return "#e91e63";  // pink
    if (portType == "bytes")   return "#9e9e9e";  // grey
    if (portType == "array")   return "#00bcd4";  // cyan
    return "#26c6da";                              // teal (object)
}
