#ifndef MODULE_INTROSPECTOR_H
#define MODULE_INTROSPECTOR_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

class LogosAPI;

/**
 * @brief Discovers and introspects loaded Logos modules
 *
 * Uses LogosAPI to enumerate available modules and extract
 * their Q_INVOKABLE method signatures, parameter types, and
 * return types. Also supports introspection via the `lm` CLI
 * tool for plugin .so files.
 */
class ModuleIntrospector : public QObject
{
    Q_OBJECT

public:
    explicit ModuleIntrospector(QObject* parent = nullptr);

    /**
     * @brief Discover all available modules via LogosAPI
     * @param api The LogosAPI instance
     * @return JSON array of module descriptors with methods
     */
    QJsonArray discoverModules(LogosAPI* api);

    /**
     * @brief Introspect a single plugin binary using `lm`
     * @param pluginPath Path to the .so/.dylib file
     * @return JSON object with module metadata and methods
     */
    QJsonObject introspectPlugin(const QString& pluginPath);

    /**
     * @brief Map a Qt type string to a workflow port type
     * @param qtType e.g. "QString", "int", "QVariantMap"
     * @return Normalized port type: "string", "number", "boolean", "object", "bytes", "array"
     */
    static QString mapQtType(const QString& qtType);

    /**
     * @brief Map a port type to a display color
     */
    static QString colorForType(const QString& portType);

private:
    QJsonArray introspectViaAPI(LogosAPI* api);
    QJsonArray introspectViaCLI();

    QString m_lmPath;
};

#endif // MODULE_INTROSPECTOR_H
