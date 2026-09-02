#include "workflow_registry_impl.h"

#include "node_type_builder.h"
#include "registry_bridge.h"
#include "registry_bridge_lp.h"

#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <memory>

namespace {

QJsonArray paramsToJson(const std::vector<MethodParam>& params)
{
    QJsonArray out;
    for (const MethodParam& p : params) {
        QJsonObject obj;
        obj[QStringLiteral("name")] = QString::fromStdString(p.name);
        obj[QStringLiteral("type")] = QString::fromStdString(p.type);
        out.append(obj);
    }
    return out;
}

// The bridge's structs -> the JSON shape NodeTypeBuilder already consumes.
// Keeping that shape unchanged is what lets the whole node-catalog half of
// this module carry over untouched.
QJsonArray modulesToJson(const std::vector<DiscoveredModule>& modules)
{
    QJsonArray out;
    for (const DiscoveredModule& mod : modules) {
        QJsonObject obj;
        obj[QStringLiteral("name")]    = QString::fromStdString(mod.name);
        obj[QStringLiteral("version")] = QString::fromStdString(mod.version);
        obj[QStringLiteral("type")]    = QString::fromStdString(mod.type);
        obj[QStringLiteral("isLive")]  = mod.isLive;

        QJsonArray methods;
        for (const MethodInfo& m : mod.methods) {
            QJsonObject method;
            method[QStringLiteral("name")]       = QString::fromStdString(m.name);
            method[QStringLiteral("returnType")] = QString::fromStdString(m.returnType);
            method[QStringLiteral("parameters")] = paramsToJson(m.parameters);
            methods.append(method);
        }
        obj[QStringLiteral("methods")] = methods;

        QJsonArray events;
        for (const EventInfo& e : mod.events) {
            QJsonObject event;
            event[QStringLiteral("name")]       = QString::fromStdString(e.name);
            event[QStringLiteral("parameters")] = paramsToJson(e.parameters);
            events.append(event);
        }
        obj[QStringLiteral("events")] = events;

        out.append(obj);
    }
    return out;
}

std::string compact(const QJsonDocument& doc)
{
    return doc.toJson(QJsonDocument::Compact).toStdString();
}

} // namespace

struct WorkflowRegistryImpl::State {
    std::shared_ptr<RegistryBridge> bridge;
    NodeTypeBuilder                 nodeTypeBuilder;
    QJsonArray                      modules;
    QJsonArray                      nodeTypes;
    // Starts dirty so the first read builds, even if onContextReady never
    // fires (impl used directly in a test).
    bool dirty = true;
};

WorkflowRegistryImpl::WorkflowRegistryImpl()
    : m_state(std::make_unique<State>())
{}

WorkflowRegistryImpl::~WorkflowRegistryImpl() = default;

void WorkflowRegistryImpl::setBridge(std::shared_ptr<RegistryBridge> bridge)
{
    m_state->bridge = std::move(bridge);
    m_state->dirty  = true;
}

void WorkflowRegistryImpl::onContextReady()
{
    // Nothing installs a bridge from outside in a real host, so make the one
    // that talks to the runtime here — this is the first point at which the
    // module knows it is running under one. A test that called setBridge()
    // first keeps its own.
    if (!m_state->bridge)
        m_state->bridge = std::make_shared<RegistryBridgeLp>("workflow_registry");

    // modulePath() is readable from here on; build the palette eagerly so the
    // canvas's first getNodeTypeDefinitions() is a cache read rather than a
    // full round of cross-module introspection.
    buildRegistry();
}

void WorkflowRegistryImpl::buildRegistry()
{
    std::vector<DiscoveredModule> discovered;

    if (m_state->bridge) {
        // modulePath() is THIS module's install directory; its parent is the
        // host's modules directory, where every sibling is another installed
        // module. That is the only module-side handle on "what is installed"
        // now that core_manager is gone.
        std::string modulesDir;
        if (!modulePath().empty()) {
            const QDir self(QString::fromStdString(modulePath()));
            QDir parent(self);
            if (parent.cdUp())
                modulesDir = parent.absolutePath().toStdString();
        }
        discovered = m_state->bridge->discoverModules(modulesDir);
    }

    m_state->modules   = modulesToJson(discovered);
    m_state->nodeTypes = m_state->nodeTypeBuilder.buildNodeTypes(m_state->modules);
    m_state->dirty     = false;

    qDebug() << "[workflow_registry] registry built:"
             << m_state->modules.size() << "modules,"
             << m_state->nodeTypes.size() << "node types";

    registryNodeTypesUpdated(static_cast<int64_t>(m_state->nodeTypes.size()));
}

std::string WorkflowRegistryImpl::getAvailableModules()
{
    if (m_state->dirty) buildRegistry();
    return compact(QJsonDocument(m_state->modules));
}

std::string WorkflowRegistryImpl::getModuleDetail(const std::string& moduleName)
{
    if (m_state->dirty) buildRegistry();

    const QString wanted = QString::fromStdString(moduleName);
    for (const auto& value : m_state->modules) {
        const QJsonObject mod = value.toObject();
        if (mod.value(QStringLiteral("name")).toString() == wanted)
            return compact(QJsonDocument(mod));
    }

    QJsonObject err;
    err[QStringLiteral("error")] = QStringLiteral("Module not found: %1").arg(wanted);
    return compact(QJsonDocument(err));
}

std::string WorkflowRegistryImpl::getNodeTypeDefinitions()
{
    if (m_state->dirty) buildRegistry();
    return compact(QJsonDocument(m_state->nodeTypes));
}

std::string WorkflowRegistryImpl::refreshModules()
{
    buildRegistry();

    QJsonObject result;
    result[QStringLiteral("status")]    = QStringLiteral("ok");
    result[QStringLiteral("modules")]   = m_state->modules.size();
    result[QStringLiteral("nodeTypes")] = m_state->nodeTypes.size();
    return compact(QJsonDocument(result));
}
