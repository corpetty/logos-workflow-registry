#include "workflow_registry_plugin.h"
#include "module_introspector.h"
#include "node_type_builder.h"
#include "logos_api.h"
#include "logos_api_client.h"

#include <QDebug>
#include <QJsonDocument>

WorkflowRegistryPlugin::WorkflowRegistryPlugin(QObject* parent)
    : QObject(parent)
    , m_introspector(new ModuleIntrospector(this))
    , m_nodeTypeBuilder(new NodeTypeBuilder(this))
{
    qDebug() << "[workflow_registry] Constructor";
}

WorkflowRegistryPlugin::~WorkflowRegistryPlugin()
{
    qDebug() << "[workflow_registry] Destructor";
}

void WorkflowRegistryPlugin::initLogos(LogosAPI* logosAPIInstance)
{
    if (logos) {
        delete logos;
        logos = nullptr;
    }
    if (logosAPI) {
        delete logosAPI;
        logosAPI = nullptr;
    }
    logosAPI = logosAPIInstance;
    if (logosAPI) {
        logos = new LogosModules(logosAPI);
    }

    qDebug() << "[workflow_registry] initLogos — rebuilding registry";
    buildRegistry();
}

void WorkflowRegistryPlugin::buildRegistry()
{
    m_modules = m_introspector->discoverModules(logosAPI);
    m_nodeTypes = m_nodeTypeBuilder->buildNodeTypes(m_modules);
    m_dirty = false;

    qDebug() << "[workflow_registry] Registry built:"
             << m_modules.size() << "modules,"
             << m_nodeTypes.size() << "node types";

    emit eventResponse("registryNodeTypesUpdated",
                       QVariantList() << m_nodeTypes.size());
}

QString WorkflowRegistryPlugin::getAvailableModules()
{
    if (m_dirty) buildRegistry();
    return QString::fromUtf8(QJsonDocument(m_modules).toJson(QJsonDocument::Compact));
}

QString WorkflowRegistryPlugin::getModuleDetail(const QString& moduleName)
{
    if (m_dirty) buildRegistry();

    for (const auto& val : m_modules) {
        QJsonObject mod = val.toObject();
        if (mod["name"].toString() == moduleName) {
            return QString::fromUtf8(QJsonDocument(mod).toJson(QJsonDocument::Compact));
        }
    }
    QJsonObject err;
    err["error"] = QString("Module not found: %1").arg(moduleName);
    return QString::fromUtf8(QJsonDocument(err).toJson(QJsonDocument::Compact));
}

QString WorkflowRegistryPlugin::getNodeTypeDefinitions()
{
    if (m_dirty) buildRegistry();
    return QString::fromUtf8(QJsonDocument(m_nodeTypes).toJson(QJsonDocument::Compact));
}

QString WorkflowRegistryPlugin::refreshModules()
{
    m_dirty = true;
    buildRegistry();

    QJsonObject result;
    result["status"] = "ok";
    result["modules"] = m_modules.size();
    result["nodeTypes"] = m_nodeTypes.size();
    return QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
}
