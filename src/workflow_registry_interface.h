#ifndef WORKFLOW_REGISTRY_INTERFACE_H
#define WORKFLOW_REGISTRY_INTERFACE_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include "interface.h"

/**
 * @brief Interface for the Workflow Registry module
 *
 * Discovers loaded Logos modules, introspects their Q_INVOKABLE methods,
 * and produces node type definitions consumable by the workflow canvas
 * and engine.
 */
class WorkflowRegistryInterface : public PluginInterface
{
public:
    virtual ~WorkflowRegistryInterface() = default;

    /**
     * @brief Get all available modules with their metadata
     * @return JSON array of module descriptors
     */
    Q_INVOKABLE virtual QString getAvailableModules() = 0;

    /**
     * @brief Get detailed info for a single module
     * @param moduleName The module to inspect
     * @return JSON object with module detail including methods
     */
    Q_INVOKABLE virtual QString getModuleDetail(const QString& moduleName) = 0;

    /**
     * @brief Get all node type definitions (module methods + built-in nodes)
     *
     * Returns the complete set of node types that the canvas can render
     * and the engine can execute. Includes:
     * - Module method nodes (one per Q_INVOKABLE method on each loaded module)
     * - Built-in utility nodes (String, Number, Boolean, JSON, Display, Template)
     * - Built-in control flow nodes (IfElse, Switch, ForEach, Merge, TryCatch, Retry, Fallback)
     * - Built-in transform nodes (ArrayMap, ArrayFilter, ObjectPick, ObjectMerge, CodeExpression, HttpRequest)
     * - Built-in trigger nodes (Webhook, Timer, ManualTrigger)
     *
     * @return JSON array of node type definitions
     */
    Q_INVOKABLE virtual QString getNodeTypeDefinitions() = 0;

    /**
     * @brief Force re-scan of available modules
     * @return Status message
     */
    Q_INVOKABLE virtual QString refreshModules() = 0;

signals:
    void eventResponse(const QString& eventName, const QVariantList& data);
};

#define WorkflowRegistryInterface_iid "org.logos.WorkflowRegistryInterface"
Q_DECLARE_INTERFACE(WorkflowRegistryInterface, WorkflowRegistryInterface_iid)

#endif // WORKFLOW_REGISTRY_INTERFACE_H
