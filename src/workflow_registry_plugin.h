#ifndef WORKFLOW_REGISTRY_PLUGIN_H
#define WORKFLOW_REGISTRY_PLUGIN_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include "workflow_registry_interface.h"
#include "logos_api.h"
#include "logos_sdk.h"

class ModuleIntrospector;
class NodeTypeBuilder;

/**
 * @brief Workflow Registry plugin implementation
 *
 * Discovers all loaded Logos modules, introspects their methods,
 * and builds a registry of node type definitions for the workflow
 * canvas and engine.
 */
class WorkflowRegistryPlugin : public QObject, public WorkflowRegistryInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID WorkflowRegistryInterface_iid FILE "metadata.json")
    Q_INTERFACES(WorkflowRegistryInterface PluginInterface)

public:
    explicit WorkflowRegistryPlugin(QObject* parent = nullptr);
    ~WorkflowRegistryPlugin() override;

    // PluginInterface
    QString name() const override { return "workflow_registry"; }
    QString version() const override { return "1.0.0"; }

    // WorkflowRegistryInterface
    Q_INVOKABLE QString getAvailableModules() override;
    Q_INVOKABLE QString getModuleDetail(const QString& moduleName) override;
    Q_INVOKABLE QString getNodeTypeDefinitions() override;
    Q_INVOKABLE QString refreshModules() override;

    // LogosAPI initialization
    Q_INVOKABLE void initLogos(LogosAPI* logosAPIInstance);

signals:
    void eventResponse(const QString& eventName, const QVariantList& args);

private:
    void buildRegistry();

    LogosModules* logos = nullptr;
    ModuleIntrospector* m_introspector = nullptr;
    NodeTypeBuilder* m_nodeTypeBuilder = nullptr;

    // Cached module data
    QJsonArray m_modules;
    QJsonArray m_nodeTypes;
    bool m_dirty = true;
};

#endif // WORKFLOW_REGISTRY_PLUGIN_H
