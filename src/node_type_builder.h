#ifndef NODE_TYPE_BUILDER_H
#define NODE_TYPE_BUILDER_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

/**
 * @brief Builds workflow node type definitions from introspected module data
 *
 * Takes raw module metadata (from ModuleIntrospector) and produces
 * the full set of node type definitions used by the canvas and engine.
 * Also registers built-in nodes (utility, control flow, transform, trigger).
 */
class NodeTypeBuilder : public QObject
{
    Q_OBJECT

public:
    explicit NodeTypeBuilder(QObject* parent = nullptr);

    /**
     * @brief Build the complete node type registry
     * @param modules JSON array of module descriptors from ModuleIntrospector
     * @return JSON array of node type definitions
     */
    QJsonArray buildNodeTypes(const QJsonArray& modules);

private:
    /**
     * @brief Create node types from a single module's methods
     */
    QJsonArray buildModuleNodeTypes(const QJsonObject& module);

    /**
     * @brief Create built-in utility node types
     */
    QJsonArray buildUtilityNodeTypes();

    /**
     * @brief Create built-in control flow node types
     */
    QJsonArray buildControlFlowNodeTypes();

    /**
     * @brief Create built-in data transform node types
     */
    QJsonArray buildTransformNodeTypes();

    /**
     * @brief Create built-in trigger node types
     */
    QJsonArray buildTriggerNodeTypes();

    /**
     * @brief Generate a human-friendly display name from a module name
     * e.g. "logos_chat_module" -> "Chat"
     */
    static QString displayNameForModule(const QString& moduleName);

    /**
     * @brief Assign a color to a module based on its category or name
     */
    static QString colorForModule(const QString& moduleName);

    /**
     * @brief Helper to build a port definition
     */
    static QJsonObject makePort(const QString& id, const QString& type,
                                const QString& label, const QString& direction = "input");
};

#endif // NODE_TYPE_BUILDER_H
