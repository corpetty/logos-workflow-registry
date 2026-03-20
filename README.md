# logos-workflow-registry

A native [Logos](https://logos.co) module that discovers loaded Logos modules, introspects their method signatures, and produces node type definitions for use by the workflow canvas and engine.

This is **Module 1** of the [Logos Legos](https://github.com/corpetty/logos-legos) v2 native architecture — the discovery layer that feeds everything else.

---

## What It Does

The registry queries the Logos ecosystem for loaded plugins, inspects each plugin's `Q_INVOKABLE` methods, and translates them into a unified registry of node types. Both the [workflow canvas](https://github.com/corpetty/logos-workflow-canvas) (for palette population) and the [workflow engine](https://github.com/corpetty/logos-workflow-engine) (for dispatch validation) depend on this registry.

It also exposes the full catalog of built-in node types — control flow, transforms, triggers, and utilities — so consumers have a single source of truth for what nodes exist and how they are wired.

Module introspection runs once at startup (during `initLogos()`) and caches the results. Subsequent calls to `getAvailableModules()` or `getNodeTypeDefinitions()` return the cached data. Call `refreshModules()` to force a re-scan — for example, after loading or unloading plugins at runtime.

---

## API

The module exposes four `Q_INVOKABLE` methods through the `WorkflowRegistryInterface`:

| Method | Returns | Description |
|---|---|---|
| `getAvailableModules()` | `QString` (JSON array) | All loaded modules with name, version, and method count |
| `getModuleDetail(moduleName)` | `QString` (JSON object) | Full detail for one module, including all invokable methods and their parameter types |
| `getNodeTypeDefinitions()` | `QString` (JSON array) | Complete node type catalog — module methods + all built-in node types |
| `refreshModules()` | `QString` (JSON object) | Force a re-scan of loaded modules and rebuild the registry. Returns `{"status":"ok","modules":N,"nodeTypes":N}` |

All return values are JSON-encoded strings (Qt types marshalled across process boundaries).

### Events

The registry emits a `registryNodeTypesUpdated` event (via `eventResponse`) after every rebuild, carrying the new node type count.

### Node Type Categories Returned by `getNodeTypeDefinitions()`

- **Module method nodes** — one node per `Q_INVOKABLE` method on each loaded plugin
- **Utility nodes** (8) — String, Number, Boolean, JSON Parse, JSON Stringify, Display, Template
- **Control flow nodes** (7) — IfElse, Switch, ForEach, Merge, TryCatch, Retry, Fallback
- **Transform nodes** (6) — ArrayMap, ArrayFilter, ObjectPick, ObjectMerge, CodeExpression, HttpRequest
- **Trigger nodes** (3) — Webhook, Timer, ManualTrigger

Total built-in nodes: **24**, plus one dynamically generated node per introspected module method.

### Node Type Definition Schema

Each entry in the `getNodeTypeDefinitions()` array has this shape:

```json
{
  "nodeTypeId": "Chat/sendMessage",
  "category": "module_method",
  "module": "logos_chat_module",
  "method": "sendMessage",
  "displayName": "Chat",
  "methodDisplayName": "sendMessage",
  "color": "#4a9eff",
  "isLive": true,
  "ports": {
    "inputs": [
      {
        "id": "roomId",
        "type": "string",
        "label": "roomId",
        "direction": "input",
        "color": "#4caf50"
      },
      {
        "id": "message",
        "type": "string",
        "label": "message",
        "direction": "input",
        "color": "#4caf50"
      }
    ],
    "outputs": [
      {
        "id": "result",
        "type": "object",
        "label": "Result",
        "direction": "output",
        "color": "#26c6da"
      }
    ]
  }
}
```

Built-in nodes follow the same schema but omit `module`, `method`, and `isLive`, and may include `defaultProperties` for configurable values and boolean flags like `isTrigger` or `isErrorCatch`:

```json
{
  "nodeTypeId": "Flow/IfElse",
  "category": "control_flow",
  "displayName": "Flow",
  "methodDisplayName": "If / Else",
  "color": "#7e57c2",
  "ports": {
    "inputs": [
      { "id": "condition", "type": "boolean", "label": "Condition", "direction": "input", "color": "#e91e63" },
      { "id": "value", "type": "object", "label": "Value", "direction": "input", "color": "#26c6da" }
    ],
    "outputs": [
      { "id": "true", "type": "object", "label": "True", "direction": "output", "color": "#26c6da" },
      { "id": "false", "type": "object", "label": "False", "direction": "output", "color": "#26c6da" }
    ]
  }
}
```

### Port Type Mapping

Qt types from method signatures are normalized to workflow port types:

| Qt Type | Port Type | Port Color |
|---|---|---|
| `QString`, `QUrl` | `string` | `#4caf50` (green) |
| `int`, `double`, `float`, `qint64`, `quint64` | `number` | `#ff9800` (orange) |
| `bool` | `boolean` | `#e91e63` (pink) |
| `QByteArray` | `bytes` | `#9e9e9e` (grey) |
| `QStringList` | `array` | `#00bcd4` (cyan) |
| `QVariant`, `QVariantMap`, `QVariantList`, `QJsonObject`, `QJsonArray`, `LogosResult`, everything else | `object` | `#26c6da` (teal) |

The `const` and `&` qualifiers are stripped before mapping, so `const QString&` maps to `string`.

### Module Discovery Details

The introspector discovers modules in two ways, tried in order:

1. **LogosAPI** (primary) — Queries `core_manager.getKnownPlugins()` and `core_manager.getPluginMethods(name)` for each plugin. Falls back to `capability_module` if `core_manager` is unavailable.
2. **`lm` CLI** (fallback) — Scans `~/.local/share/logos/modules` and `$LOGOS_PLUGINS_DIR` for `*_plugin.so`/`*_plugin.dylib` files, running `lm methods <path> --json` on each.

The following modules are excluded from introspection:
- `core_manager` and `capability_module` (infrastructure)
- Any module whose name starts with `workflow_` (avoids circular introspection)
- Lifecycle methods (`initLogos`, `name`, `version`, `eventResponse`, `deleteLater`, `destroyed`, and methods starting with `_`) are filtered out

---

## Architecture

This module sits at the base of the Logos Legos dependency graph:

```
logos-workflow-registry   <- you are here
        |
        |-->  logos-workflow-canvas  (populates node palette)
        |
        \-->  logos-workflow-engine  (validates dispatch targets)
                    |
                    \-->  logos-workflow-scheduler  (triggers engine)
```

The registry has no upstream module dependencies — it only depends on `LogosAPI` to query `core_manager` for plugin metadata.

---

## Project Structure

```
logos-workflow-registry/
├── src/
│   ├── workflow_registry_interface.h   # Public interface (Q_INVOKABLE declarations)
│   ├── workflow_registry_plugin.h      # Plugin implementation header
│   ├── workflow_registry_plugin.cpp    # Plugin implementation
│   ├── module_introspector.h           # Queries LogosAPI for loaded modules
│   ├── module_introspector.cpp
│   ├── node_type_builder.h             # Translates module methods -> node type defs
│   └── node_type_builder.cpp
├── generated_code/                     # Auto-generated scaffolding
├── CMakeLists.txt                      # Build configuration
├── flake.nix                           # Nix build definition
├── module.yaml                         # Logos module descriptor
└── metadata.json                       # Qt plugin metadata (name, version, capabilities)
```

---

## Building

### With Nix (recommended)

The registry has no upstream module dependencies, so it builds standalone. It uses `logos-module-builder` to provide the CMake helpers and SDK headers.

```bash
nix build
```

Output: `result/lib/workflow_registry_plugin.so`

This is the first module in the build chain — **build and push the registry before building the engine, scheduler, or canvas**, since they reference it as a flake input.

### With CMake

Requires `logos-module-builder` CMake helpers and `LogosModule.cmake` on your include path. Set `LOGOS_CPP_SDK_ROOT` and `LOGOS_LIBLOGOS_ROOT` to point to your SDK installations.

```bash
mkdir build && cd build
cmake .. -GNinja
ninja
```

Output: `build/modules/workflow_registry_plugin.so`

---

## Usage

Once loaded into `logoscore`, call the registry methods through `LogosAPI`:

```bash
# Get all available modules
logoscore -c "workflow_registry.getAvailableModules()"

# Get detail for a single module
logoscore -c "workflow_registry.getModuleDetail(logos_chat_module)"

# Get node type definitions for the canvas/engine
logoscore -c "workflow_registry.getNodeTypeDefinitions()"

# Refresh after loading new plugins
logoscore -c "workflow_registry.refreshModules()"
```

---

## Related Modules

| Module | Role |
|---|---|
| [logos-workflow-canvas](https://github.com/corpetty/logos-workflow-canvas) | Qt/QML visual editor — queries this registry to populate the node palette |
| [logos-workflow-engine](https://github.com/corpetty/logos-workflow-engine) | Headless DAG executor — uses this registry to resolve dispatch targets |
| [logos-workflow-scheduler](https://github.com/corpetty/logos-workflow-scheduler) | Cron/webhook trigger manager — triggers the engine |
| [logos-legos](https://github.com/corpetty/logos-legos) | Parent repo with v1 prototype and full architecture docs |

See [logos-legos/docs/NATIVE-ARCHITECTURE.md](https://github.com/corpetty/logos-legos/blob/main/docs/NATIVE-ARCHITECTURE.md) for the full v2 design.

---

## License

MIT
