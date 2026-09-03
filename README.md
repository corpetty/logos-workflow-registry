# logos-workflow-registry

A native [Logos](https://logos.co) module that discovers installed Logos modules, introspects their methods and events, and produces node type definitions for use by the workflow canvas and engine.

This is **Module 1** of the [Logos Legos](https://github.com/corpetty/logos-legos) v2 native architecture — the discovery layer that feeds everything else.

---

## What It Does

The registry enumerates the modules installed alongside it, asks each one what it exposes, and translates the answer into a unified registry of node types. Both the [workflow canvas](https://github.com/corpetty/logos-workflow-canvas) (for palette population) and the [workflow engine](https://github.com/corpetty/logos-workflow-scheduler) depend on this registry, the latter via a declared dependency rather than dynamic discovery.

It also exposes the full catalog of built-in node types — control flow, transforms, triggers, and utilities — so consumers have a single source of truth for what nodes exist and how they are wired.

The palette is built lazily and cached: the first call to `getAvailableModules()` or `getNodeTypeDefinitions()` triggers a full scan; later calls return the cached result. Call `refreshModules()` to force a re-scan — for example, after installing a new module at runtime.

---

## Module Discovery

This module is a **universal** Logos module (`interface: "universal"`): the impl class in `src/workflow_registry_impl.h` is the entire public API, and the Qt plugin glue plus a published `.lidl` contract are generated from it by `logos-module-builder`. Being universal means the runtime confines it to its *declared* dependencies (`metadata.json#dependencies`, empty here) — there is no `core_manager` to ask "what modules exist," because a universal module is never handed one. The registry's whole job is knowing about modules it cannot declare a dependency on, so it works around that in two halves:

1. **Which modules exist — off disk.** `LogosModuleContext::modulePath()` is this module's own install directory; its parent is the host's modules root, and every sibling directory is another installed module carrying a `manifest.json` (an `.lgx` install) or `metadata.json` (staged straight from a Nix build). A host with more than one modules root (an app-bundled one plus a user-writable one) is covered by setting `LOGOS_WORKFLOW_MODULE_DIRS` (colon-separated, `;` on Windows) to the extra roots.
2. **What each one exposes — asked of the module itself**, over [`logos::LpClient`](https://github.com/logos-co/logos-cpp-sdk) (the Qt-free lp C ABI client), calling `getPluginMethods` and `getPluginEvents` — the surface every generated module publishes. This is the supported way to call a module chosen at runtime by name; it carries the capability/token flow itself.

A module that is installed but not currently loaded has no listener to answer that call. Rather than block the caller for the protocol's 20-second default, introspection carries a 1.5-second deadline — long enough for any loaded module to answer, short enough that an unloaded one doesn't stall the whole palette build. Such a module is still listed, marked `isLive: false`, so the canvas can show it greyed out instead of pretending it doesn't exist.

---

## API

The impl class exposes four public methods, from which `logos-module-builder` derives the `.lidl` contract and the Qt plugin glue:

| Method | Returns | Description |
|---|---|---|
| `getAvailableModules()` | JSON array | All installed modules, with name, version, type, live/methods/events |
| `getModuleDetail(moduleName)` | JSON object | Full detail for one module, or `{"error": ...}` if unknown |
| `getNodeTypeDefinitions()` | JSON array | Complete node type catalog — module methods + all built-in node types |
| `refreshModules()` | JSON object | Force a re-scan. Returns `{"status":"ok","modules":N,"nodeTypes":N}` |

All return values are JSON-encoded strings.

### Events

The registry declares one typed event, `registryNodeTypesUpdated(nodeTypeCount)`, fired after every rebuild. A dependent module gets a typed subscription for this through the generated `modules().workflow_registry.onRegistryNodeTypesUpdated(...)`, rather than the old untyped `eventResponse` signal.

### Node Type Categories Returned by `getNodeTypeDefinitions()`

- **Module method nodes** — one node per introspected method on each installed module
- **Utility nodes** (7) — String, Number, Boolean, JSON Parse, JSON Stringify, Display, Template
- **Control flow nodes** (7) — IfElse, Switch, ForEach, Merge, TryCatch, Retry, Fallback
- **Transform nodes** (6) — ArrayMap, ArrayFilter, ObjectPick, ObjectMerge, CodeExpression, HttpRequest
- **Trigger nodes** (3) — Webhook, Timer, ManualTrigger

Total built-in nodes: **23**, plus one dynamically generated node per introspected module method and one per module event.

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

Method signatures and LIDL/Qt type names are normalized to workflow port types:

| Type names | Port Type | Port Color |
|---|---|---|
| `QString`, `std::string`, `tstr`, `char*` | `string` | `#4caf50` (green) |
| `int`, `int32_t`, `int64_t`, `double`, `float`, `float64` | `number` | `#ff9800` (orange) |
| `bool`, `boolean` | `boolean` | `#e91e63` (pink) |
| `QByteArray`, `bytes`, `bstr` | `bytes` | `#9e9e9e` (grey) |
| `QStringList`, `QVariantList`, `[...]` (LIDL array), `QJsonArray` | `array` | `#00bcd4` (cyan) |
| everything else (`QVariantMap`, `QJsonObject`, ...) | `object` | `#26c6da` (teal) |

### Excluded from Introspection

- `core_manager` and `capability_module` — infrastructure, not workflow steps
- Any module whose name starts with `workflow_` — avoids the registry/engine/scheduler re-entering their own machinery from a running workflow
- Lifecycle and plumbing methods (`initLogos`, `name`, `version`, `eventResponse`, `deleteLater`, `destroyed`, `getPluginMethods`, `getPluginEvents`, and anything starting with `_`)

---

## Architecture

This module sits at the base of the Logos Legos dependency graph:

```
logos-workflow-registry   <- you are here
        |
        |-->  logos-workflow-canvas  (populates node palette; declared dependency)
        |
        \-->  logos-workflow-engine  (declared dependency, not yet called at runtime)
                    |
                    \-->  logos-workflow-scheduler  (triggers the engine)
```

The registry has no declared module dependencies — discovery is disk scan plus `LpClient`, not a typed call to any other module.

---

## Project Structure

```
logos-workflow-registry/
├── src/
│   ├── workflow_registry_impl.h    # The public API — this IS the module
│   ├── workflow_registry_impl.cpp
│   ├── registry_bridge.h           # Qt-free seam: "enumerate and introspect installed modules"
│   ├── registry_bridge_lp.h        # LpClient-backed implementation of the seam
│   ├── registry_bridge_lp.cpp
│   ├── node_type_builder.h         # Translates module methods -> node type defs, built-ins
│   └── node_type_builder.cpp
├── generated_code/                 # Generated glue + derived .lidl contract (build output)
├── CMakeLists.txt                  # Build configuration
├── flake.nix                       # Nix build definition
└── metadata.json                   # Module descriptor: name, type, interface, dependencies
```

`generated_code/` no longer ships committed scaffolding — `interface: "universal"` means the `*Plugin`/`*Interface` glue and the `.lidl` contract are generated at build time from `workflow_registry_impl.h`, not hand-written.

---

## Building

### With Nix (recommended)

The registry has no module dependencies, so it builds standalone. It uses `logos-module-builder` to generate the Qt plugin glue and provide the CMake helpers and SDK headers.

```bash
nix build
```

Output: `result/lib/workflow_registry_plugin.so`, plus a derived `result/lib/workflow_registry.lidl` (installed to `share/logos/`) that dependents (the engine, the canvas) consume for their typed `modules().workflow_registry` accessor.

This is the first module in the build chain — **build and push the registry before building the engine, scheduler, or canvas**, since they reference it as a flake input.

### With CMake

Requires `logos-module-builder`'s CMake helpers (`LogosModule.cmake`) and the generator tools (`logos-cpp-generator`, `logos-qt-generator`) on your `PATH`, plus `LOGOS_MODULE_BUILDER_ROOT` set. The Nix build drives codegen through `logos-module-builder`'s `preConfigure`; a bare CMake build needs the equivalent generator invocation run first (see `logos-module-builder`'s docs for the `universal` interface).

```bash
mkdir build && cd build
cmake .. -GNinja
ninja
```

Output: `build/modules/workflow_registry_plugin.so`

---

## Usage

Once loaded into a Logos host, call the registry methods through `LogosAPI` (or, from a module's own code, through the generated `modules().workflow_registry` accessor once it declares the dependency):

```bash
# Get all installed modules
logoscore -c "workflow_registry.getAvailableModules()"

# Get detail for a single module
logoscore -c "workflow_registry.getModuleDetail(logos_chat_module)"

# Get node type definitions for the canvas/engine
logoscore -c "workflow_registry.getNodeTypeDefinitions()"

# Refresh after installing new modules
logoscore -c "workflow_registry.refreshModules()"
```

---

## Related Modules

| Module | Role |
|---|---|
| [logos-workflow-canvas](https://github.com/corpetty/logos-workflow-canvas) | Qt/QML visual editor — queries this registry to populate the node palette |
| [logos-workflow-engine](https://github.com/corpetty/logos-workflow-engine) | DAG executor — declares this registry as a dependency |
| [logos-workflow-scheduler](https://github.com/corpetty/logos-workflow-scheduler) | Cron/webhook trigger manager — deploys workflows to the engine |
| [logos-legos](https://github.com/corpetty/logos-legos) | Parent repo with v1 prototype and full architecture docs |

See [logos-legos/docs/NATIVE-ARCHITECTURE.md](https://github.com/corpetty/logos-legos/blob/main/docs/NATIVE-ARCHITECTURE.md) for the full v2 design.

---

## License

MIT
