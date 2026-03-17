#include "node_type_builder.h"
#include "module_introspector.h"

#include <QDebug>

NodeTypeBuilder::NodeTypeBuilder(QObject* parent)
    : QObject(parent)
{
}

QJsonArray NodeTypeBuilder::buildNodeTypes(const QJsonArray& modules)
{
    QJsonArray allTypes;

    // 1. Module method nodes
    for (const auto& modVal : modules) {
        QJsonArray modTypes = buildModuleNodeTypes(modVal.toObject());
        for (const auto& t : modTypes) {
            allTypes.append(t);
        }
    }

    // 2. Built-in nodes
    for (const auto& t : buildUtilityNodeTypes()) allTypes.append(t);
    for (const auto& t : buildControlFlowNodeTypes()) allTypes.append(t);
    for (const auto& t : buildTransformNodeTypes()) allTypes.append(t);
    for (const auto& t : buildTriggerNodeTypes()) allTypes.append(t);

    qDebug() << "[node_type_builder] Built" << allTypes.size() << "node types";
    return allTypes;
}

QJsonArray NodeTypeBuilder::buildModuleNodeTypes(const QJsonObject& module)
{
    QJsonArray types;
    QString moduleName = module["name"].toString();
    QString displayName = displayNameForModule(moduleName);
    QString color = colorForModule(moduleName);
    bool isLive = module["isLive"].toBool(false);

    QJsonArray methods = module["methods"].toArray();
    for (const auto& methodVal : methods) {
        QJsonObject method = methodVal.toObject();
        QString methodName = method["name"].toString();

        QJsonObject nodeDef;
        nodeDef["nodeTypeId"] = displayName + "/" + methodName;
        nodeDef["category"] = "module_method";
        nodeDef["module"] = moduleName;
        nodeDef["method"] = methodName;
        nodeDef["displayName"] = displayName;
        nodeDef["methodDisplayName"] = methodName;
        nodeDef["color"] = color;
        nodeDef["isLive"] = isLive;

        // Build input ports from method parameters
        QJsonArray inputs;
        QJsonArray params = method["parameters"].toArray();
        for (const auto& paramVal : params) {
            QJsonObject param = paramVal.toObject();
            inputs.append(makePort(
                param["name"].toString(),
                param["type"].toString(),
                param["name"].toString(),
                "input"
            ));
        }

        // Build output port from return type
        QJsonArray outputs;
        outputs.append(makePort("result", method["returnType"].toString("object"), "Result", "output"));

        QJsonObject ports;
        ports["inputs"] = inputs;
        ports["outputs"] = outputs;
        nodeDef["ports"] = ports;

        types.append(nodeDef);
    }

    return types;
}

QJsonArray NodeTypeBuilder::buildUtilityNodeTypes()
{
    QJsonArray types;

    // String constant
    {
        QJsonObject def;
        def["nodeTypeId"] = "Utility/String";
        def["category"] = "utility";
        def["displayName"] = "Utility";
        def["methodDisplayName"] = "String";
        def["color"] = "#607d8b";

        QJsonObject ports;
        ports["inputs"] = QJsonArray();
        QJsonArray outputs;
        outputs.append(makePort("output", "string", "Value", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        QJsonObject properties;
        properties["value"] = "";
        def["defaultProperties"] = properties;

        types.append(def);
    }

    // Number constant
    {
        QJsonObject def;
        def["nodeTypeId"] = "Utility/Number";
        def["category"] = "utility";
        def["displayName"] = "Utility";
        def["methodDisplayName"] = "Number";
        def["color"] = "#607d8b";

        QJsonObject ports;
        ports["inputs"] = QJsonArray();
        QJsonArray outputs;
        outputs.append(makePort("output", "number", "Value", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        QJsonObject properties;
        properties["value"] = 0;
        def["defaultProperties"] = properties;

        types.append(def);
    }

    // Boolean constant
    {
        QJsonObject def;
        def["nodeTypeId"] = "Utility/Boolean";
        def["category"] = "utility";
        def["displayName"] = "Utility";
        def["methodDisplayName"] = "Boolean";
        def["color"] = "#607d8b";

        QJsonObject ports;
        ports["inputs"] = QJsonArray();
        QJsonArray outputs;
        outputs.append(makePort("output", "boolean", "Value", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        types.append(def);
    }

    // JSON Parse
    {
        QJsonObject def;
        def["nodeTypeId"] = "Utility/JSONParse";
        def["category"] = "utility";
        def["displayName"] = "Utility";
        def["methodDisplayName"] = "JSON Parse";
        def["color"] = "#607d8b";

        QJsonObject ports;
        QJsonArray inputs;
        inputs.append(makePort("input", "string", "JSON String", "input"));
        ports["inputs"] = inputs;
        QJsonArray outputs;
        outputs.append(makePort("output", "object", "Object", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        types.append(def);
    }

    // JSON Stringify
    {
        QJsonObject def;
        def["nodeTypeId"] = "Utility/JSONStringify";
        def["category"] = "utility";
        def["displayName"] = "Utility";
        def["methodDisplayName"] = "JSON Stringify";
        def["color"] = "#607d8b";

        QJsonObject ports;
        QJsonArray inputs;
        inputs.append(makePort("input", "object", "Object", "input"));
        ports["inputs"] = inputs;
        QJsonArray outputs;
        outputs.append(makePort("output", "string", "JSON String", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        types.append(def);
    }

    // Display
    {
        QJsonObject def;
        def["nodeTypeId"] = "Utility/Display";
        def["category"] = "utility";
        def["displayName"] = "Utility";
        def["methodDisplayName"] = "Display";
        def["color"] = "#607d8b";

        QJsonObject ports;
        QJsonArray inputs;
        inputs.append(makePort("value", "object", "Value", "input"));
        ports["inputs"] = inputs;
        ports["outputs"] = QJsonArray();
        def["ports"] = ports;

        types.append(def);
    }

    // Template
    {
        QJsonObject def;
        def["nodeTypeId"] = "Utility/Template";
        def["category"] = "utility";
        def["displayName"] = "Utility";
        def["methodDisplayName"] = "Template";
        def["color"] = "#607d8b";

        QJsonObject ports;
        QJsonArray inputs;
        inputs.append(makePort("data", "object", "Data", "input"));
        ports["inputs"] = inputs;
        QJsonArray outputs;
        outputs.append(makePort("output", "string", "Result", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        QJsonObject properties;
        properties["template"] = "{key}";
        def["defaultProperties"] = properties;

        types.append(def);
    }

    return types;
}

QJsonArray NodeTypeBuilder::buildControlFlowNodeTypes()
{
    QJsonArray types;

    // If / Else
    {
        QJsonObject def;
        def["nodeTypeId"] = "Flow/IfElse";
        def["category"] = "control_flow";
        def["displayName"] = "Flow";
        def["methodDisplayName"] = "If / Else";
        def["color"] = "#7e57c2";

        QJsonObject ports;
        QJsonArray inputs;
        inputs.append(makePort("condition", "boolean", "Condition", "input"));
        inputs.append(makePort("value", "object", "Value", "input"));
        ports["inputs"] = inputs;
        QJsonArray outputs;
        outputs.append(makePort("true", "object", "True", "output"));
        outputs.append(makePort("false", "object", "False", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        types.append(def);
    }

    // Switch
    {
        QJsonObject def;
        def["nodeTypeId"] = "Flow/Switch";
        def["category"] = "control_flow";
        def["displayName"] = "Flow";
        def["methodDisplayName"] = "Switch";
        def["color"] = "#7e57c2";

        QJsonObject ports;
        QJsonArray inputs;
        inputs.append(makePort("value", "object", "Value", "input"));
        inputs.append(makePort("key", "string", "Key", "input"));
        ports["inputs"] = inputs;
        QJsonArray outputs;
        outputs.append(makePort("case_1", "object", "Case 1", "output"));
        outputs.append(makePort("case_2", "object", "Case 2", "output"));
        outputs.append(makePort("case_3", "object", "Case 3", "output"));
        outputs.append(makePort("default", "object", "Default", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        QJsonObject properties;
        properties["cases"] = "case1,case2,case3";
        def["defaultProperties"] = properties;

        types.append(def);
    }

    // ForEach
    {
        QJsonObject def;
        def["nodeTypeId"] = "Flow/ForEach";
        def["category"] = "control_flow";
        def["displayName"] = "Flow";
        def["methodDisplayName"] = "ForEach";
        def["color"] = "#7e57c2";

        QJsonObject ports;
        QJsonArray inputs;
        inputs.append(makePort("array", "array", "Array", "input"));
        ports["inputs"] = inputs;
        QJsonArray outputs;
        outputs.append(makePort("item", "object", "Item", "output"));
        outputs.append(makePort("done", "object", "Done", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        types.append(def);
    }

    // Merge
    {
        QJsonObject def;
        def["nodeTypeId"] = "Flow/Merge";
        def["category"] = "control_flow";
        def["displayName"] = "Flow";
        def["methodDisplayName"] = "Merge";
        def["color"] = "#7e57c2";

        QJsonObject ports;
        QJsonArray inputs;
        inputs.append(makePort("input_1", "object", "Input 1", "input"));
        inputs.append(makePort("input_2", "object", "Input 2", "input"));
        inputs.append(makePort("input_3", "object", "Input 3", "input"));
        inputs.append(makePort("input_4", "object", "Input 4", "input"));
        ports["inputs"] = inputs;
        QJsonArray outputs;
        outputs.append(makePort("merged", "object", "Merged", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        types.append(def);
    }

    // Try / Catch
    {
        QJsonObject def;
        def["nodeTypeId"] = "Flow/TryCatch";
        def["category"] = "control_flow";
        def["displayName"] = "Flow";
        def["methodDisplayName"] = "Try / Catch";
        def["color"] = "#ef5350";
        def["isErrorCatch"] = true;

        QJsonObject ports;
        QJsonArray inputs;
        inputs.append(makePort("input", "object", "Input", "input"));
        ports["inputs"] = inputs;
        QJsonArray outputs;
        outputs.append(makePort("success", "object", "Success", "output"));
        outputs.append(makePort("error", "object", "Error", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        types.append(def);
    }

    // Retry
    {
        QJsonObject def;
        def["nodeTypeId"] = "Flow/Retry";
        def["category"] = "control_flow";
        def["displayName"] = "Flow";
        def["methodDisplayName"] = "Retry";
        def["color"] = "#ef5350";
        def["isErrorCatch"] = true;

        QJsonObject ports;
        QJsonArray inputs;
        inputs.append(makePort("input", "object", "Input", "input"));
        ports["inputs"] = inputs;
        QJsonArray outputs;
        outputs.append(makePort("result", "object", "Result", "output"));
        outputs.append(makePort("failed", "object", "Failed", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        QJsonObject properties;
        properties["maxRetries"] = 3;
        properties["delayMs"] = 1000;
        def["defaultProperties"] = properties;

        types.append(def);
    }

    // Fallback
    {
        QJsonObject def;
        def["nodeTypeId"] = "Flow/Fallback";
        def["category"] = "control_flow";
        def["displayName"] = "Flow";
        def["methodDisplayName"] = "Fallback";
        def["color"] = "#ef5350";
        def["isErrorCatch"] = true;

        QJsonObject ports;
        QJsonArray inputs;
        inputs.append(makePort("primary", "object", "Primary", "input"));
        inputs.append(makePort("fallback", "object", "Fallback", "input"));
        ports["inputs"] = inputs;
        QJsonArray outputs;
        outputs.append(makePort("result", "object", "Result", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        types.append(def);
    }

    return types;
}

QJsonArray NodeTypeBuilder::buildTransformNodeTypes()
{
    QJsonArray types;

    auto addTransform = [&](const QString& id, const QString& name,
                            const QJsonArray& inputs, const QJsonArray& outputs,
                            const QJsonObject& props = {}) {
        QJsonObject def;
        def["nodeTypeId"] = "Transform/" + id;
        def["category"] = "transform";
        def["displayName"] = "Transform";
        def["methodDisplayName"] = name;
        def["color"] = "#26a69a";

        QJsonObject ports;
        ports["inputs"] = inputs;
        ports["outputs"] = outputs;
        def["ports"] = ports;

        if (!props.isEmpty()) def["defaultProperties"] = props;
        types.append(def);
    };

    // ArrayMap
    {
        QJsonArray in, out;
        in.append(makePort("array", "array", "Array", "input"));
        in.append(makePort("expression", "string", "Expression", "input"));
        out.append(makePort("result", "array", "Result", "output"));
        QJsonObject props; props["expression"] = "item";
        addTransform("ArrayMap", "Array Map", in, out, props);
    }

    // ArrayFilter
    {
        QJsonArray in, out;
        in.append(makePort("array", "array", "Array", "input"));
        in.append(makePort("expression", "string", "Predicate", "input"));
        out.append(makePort("result", "array", "Result", "output"));
        QJsonObject props; props["expression"] = "true";
        addTransform("ArrayFilter", "Array Filter", in, out, props);
    }

    // ObjectPick
    {
        QJsonArray in, out;
        in.append(makePort("object", "object", "Object", "input"));
        in.append(makePort("keys", "string", "Keys (csv)", "input"));
        out.append(makePort("result", "object", "Result", "output"));
        addTransform("ObjectPick", "Object Pick", in, out);
    }

    // ObjectMerge
    {
        QJsonArray in, out;
        in.append(makePort("object_1", "object", "Object 1", "input"));
        in.append(makePort("object_2", "object", "Object 2", "input"));
        out.append(makePort("result", "object", "Result", "output"));
        addTransform("ObjectMerge", "Object Merge", in, out);
    }

    // CodeExpression
    {
        QJsonArray in, out;
        in.append(makePort("input", "object", "Input", "input"));
        out.append(makePort("result", "object", "Result", "output"));
        QJsonObject props; props["expression"] = "input";
        addTransform("CodeExpression", "Code Expression", in, out, props);
    }

    // HttpRequest
    {
        QJsonArray in, out;
        in.append(makePort("url", "string", "URL", "input"));
        in.append(makePort("body", "object", "Body", "input"));
        out.append(makePort("response", "object", "Response", "output"));
        out.append(makePort("status", "number", "Status", "output"));
        QJsonObject props;
        props["httpMethod"] = "GET";
        props["headers"] = "{}";
        addTransform("HttpRequest", "HTTP Request", in, out, props);
    }

    return types;
}

QJsonArray NodeTypeBuilder::buildTriggerNodeTypes()
{
    QJsonArray types;

    // Webhook
    {
        QJsonObject def;
        def["nodeTypeId"] = "Trigger/Webhook";
        def["category"] = "trigger";
        def["displayName"] = "Trigger";
        def["methodDisplayName"] = "Webhook";
        def["color"] = "#ffa726";
        def["isTrigger"] = true;

        QJsonObject ports;
        ports["inputs"] = QJsonArray();
        QJsonArray outputs;
        outputs.append(makePort("payload", "object", "Payload", "output"));
        outputs.append(makePort("headers", "object", "Headers", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        types.append(def);
    }

    // Timer
    {
        QJsonObject def;
        def["nodeTypeId"] = "Trigger/Timer";
        def["category"] = "trigger";
        def["displayName"] = "Trigger";
        def["methodDisplayName"] = "Timer";
        def["color"] = "#ffa726";
        def["isTrigger"] = true;

        QJsonObject ports;
        ports["inputs"] = QJsonArray();
        QJsonArray outputs;
        outputs.append(makePort("data", "object", "Data", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        QJsonObject properties;
        properties["intervalMs"] = 60000;
        properties["cron"] = "";
        def["defaultProperties"] = properties;

        types.append(def);
    }

    // Manual Trigger
    {
        QJsonObject def;
        def["nodeTypeId"] = "Trigger/Manual";
        def["category"] = "trigger";
        def["displayName"] = "Trigger";
        def["methodDisplayName"] = "Manual";
        def["color"] = "#ffa726";
        def["isTrigger"] = true;

        QJsonObject ports;
        ports["inputs"] = QJsonArray();
        QJsonArray outputs;
        outputs.append(makePort("data", "object", "Data", "output"));
        ports["outputs"] = outputs;
        def["ports"] = ports;

        QJsonObject properties;
        properties["data"] = "{}";
        def["defaultProperties"] = properties;

        types.append(def);
    }

    return types;
}

QString NodeTypeBuilder::displayNameForModule(const QString& moduleName)
{
    // "logos_chat_module" -> "Chat"
    // "test_basic_module" -> "Test Basic"
    QString name = moduleName;
    name.remove("logos_").remove("_module");

    QStringList parts = name.split("_", Qt::SkipEmptyParts);
    for (auto& part : parts) {
        part[0] = part[0].toUpper();
    }
    return parts.join(" ");
}

QString NodeTypeBuilder::colorForModule(const QString& moduleName)
{
    // Deterministic color assignment based on module name hash
    static const QStringList palette = {
        "#4a9eff", "#ff6b6b", "#51cf66", "#ffd43b",
        "#cc5de8", "#20c997", "#ff922b", "#845ef7",
        "#339af0", "#f06595", "#94d82d", "#fcc419",
    };

    uint hash = qHash(moduleName);
    return palette[hash % palette.size()];
}

QJsonObject NodeTypeBuilder::makePort(const QString& id, const QString& type,
                                      const QString& label, const QString& direction)
{
    QJsonObject port;
    port["id"] = id;
    port["type"] = type;
    port["label"] = label;
    port["direction"] = direction;
    port["color"] = ModuleIntrospector::colorForType(type);
    return port;
}
