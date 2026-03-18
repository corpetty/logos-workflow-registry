# logos-workflow-registry

A native [Logos](https://logos.co) module that discovers loaded Logos modules, introspects their method signatures, and produces node type definitions for use by the workflow canvas and engine.

This is **Module 1** of the [Logos Legos](https://github.com/corpetty/logos-legos) v2 native architecture — the discovery layer that feeds everything else.

---

## What It Does

The registry queries the Logos ecosystem for loaded plugins, inspects each plugin's `Q_INVOKABLE` methods, and translates them into a unified registry of node types. Both the [workflow canvas](https://github.com/corpetty/logos-workflow-canvas) (for palette population) and the [workflow engine](https://github.com/corpetty/logos-workflow-engine) (for dispatch validation) depend on this registry.

It also exposes the full catalog of built-in node types — control flow, transforms, triggers, and utilities — so consumers have a single source of truth for what nodes exist and how they are wired.

---

## API

The module exposes four `Q_INVOKABLE` methods through the `WorkflowRegistryInterface`:

| Method | Returns | Description |
|---|---|---|
| `getAvailableModules()` | `QString` (JSON array) | All loaded modules with name, version, and method count |
| `getModuleDetail(moduleName)` | `QString` (JSON object) | Full detail for one module, including all invokable methods and their parameter types |
| `getNodeTypeDefinitions()` | `QString` (JSON array) | Complete node type catalog — module methods + all built-in node types |
| `refreshModules()` | `QString` (status) | Force a re-scan of loaded modules and rebuild the registry |

All return values are JSON-encoded strings (Qt types marshalled across process boundaries).

### Node Type Categories Returned by `getNodeTypeDefinitions()`

- **Module method nodes** — one node per `Q_INVOKABLE` method on each loaded plugin
- **Utility nodes** — String, Number, Boolean, JSON Parse/Stringify, Display, Template
- **Control flow nodes** — IfElse, Switch, ForEach, Merge, TryCatch, Retry, Fallback
- **Transform nodes** — ArrayMap, ArrayFilter, ObjectPick, ObjectMerge, CodeExpression, HttpRequest
- **Trigger nodes** — Webhook, Timer, ManualTrigger

Port types are derived from Qt parameter types: `QString` → string, `int`/`double` → number, `bool` → boolean, `QVariant*` → object.

---

## Architecture

This module sits at the base of the Logos Legos dependency graph:

```
logos-workflow-registry   ← you are here
        │
        ├──▶ logos-workflow-canvas  (populates node palette)
        │
        └──▶ logos-workflow-engine  (validates dispatch targets)
                    │
                    └──▶ logos-workflow-scheduler  (triggers engine)
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
│   ├── node_type_builder.h             # Translates module methods → node type defs
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
