# logos-workflow-registry Specification

FURPS analysis of the Workflow Registry module (v1.0.0).

---

## Functionality

### Module Discovery

The registry discovers loaded Logos modules through two mechanisms, tried in priority order:

1. **LogosAPI introspection** (primary path) — Calls `core_manager.getKnownPlugins()` to enumerate all loaded plugins, then `core_manager.getPluginMethods(pluginName)` for each. If `core_manager` is unavailable, falls back to `capability_module`. This path works when the registry is loaded inside `logoscore` alongside other modules.

2. **`lm` CLI introspection** (fallback path) — Locates the `lm` binary on `$PATH` and scans plugin directories (`~/.local/share/logos/modules` and `$LOGOS_PLUGINS_DIR`) for `*_plugin.so` / `*_plugin.dylib` files. Runs `lm methods <path> --json` on each binary. This path works without a running `logoscore` instance.

Both paths filter out infrastructure modules (`core_manager`, `capability_module`) and workflow modules (any name starting with `workflow_`) to prevent circular introspection.

### Method Introspection

For each discovered module, the introspector extracts all `Q_INVOKABLE` methods from the Qt meta-object system. The following lifecycle/internal methods are excluded:

- `initLogos`, `eventResponse`, `name`, `version`, `deleteLater`, `destroyed`
- Any method whose name starts with `_`

For each remaining method, the introspector records:
- Method name
- Return type (normalized from Qt type to workflow port type)
- Parameter list with name and normalized type for each

### Node Type Translation

The `NodeTypeBuilder` converts introspected module data into node type definitions:

- **Module method nodes**: One node type per `Q_INVOKABLE` method. The `nodeTypeId` is formatted as `DisplayName/methodName` (e.g., `Chat/sendMessage`). Input ports are derived from method parameters; a single output port named `result` is derived from the return type. A deterministic color is assigned based on the module name hash against a 12-color palette.

- **Display name generation**: `logos_chat_module` becomes `Chat` (strips `logos_` prefix and `_module` suffix, title-cases remaining parts).

### Built-in Node Catalog

The registry provides 24 built-in node types across four categories:

**Utility (8 nodes)**

| Node | Inputs | Outputs | Properties |
|------|--------|---------|------------|
| String | (none) | `output: string` | `value: ""` |
| Number | (none) | `output: number` | `value: 0` |
| Boolean | (none) | `output: boolean` | (none) |
| JSON Parse | `input: string` | `output: object` | (none) |
| JSON Stringify | `input: object` | `output: string` | (none) |
| Display | `value: object` | (none) | (none) |
| Template | `data: object` | `output: string` | `template: "{key}"` |

**Control Flow (7 nodes)**

| Node | Inputs | Outputs | Properties | Flags |
|------|--------|---------|------------|-------|
| If / Else | `condition: boolean`, `value: object` | `true: object`, `false: object` | (none) | |
| Switch | `value: object`, `key: string` | `case_1: object`, `case_2: object`, `case_3: object`, `default: object` | `cases: "case1,case2,case3"` | |
| ForEach | `array: array` | `item: object`, `done: object` | (none) | |
| Merge | `input_1..4: object` | `merged: object` | (none) | |
| Try / Catch | `input: object` | `success: object`, `error: object` | (none) | `isErrorCatch` |
| Retry | `input: object` | `result: object`, `failed: object` | `maxRetries: 3`, `delayMs: 1000` | `isErrorCatch` |
| Fallback | `primary: object`, `fallback: object` | `result: object` | (none) | `isErrorCatch` |

**Transform (6 nodes)**

| Node | Inputs | Outputs | Properties |
|------|--------|---------|------------|
| Array Map | `array: array`, `expression: string` | `result: array` | `expression: "item"` |
| Array Filter | `array: array`, `expression: string` | `result: array` | `expression: "true"` |
| Object Pick | `object: object`, `keys: string` | `result: object` | (none) |
| Object Merge | `object_1: object`, `object_2: object` | `result: object` | (none) |
| Code Expression | `input: object` | `result: object` | `expression: "input"` |
| HTTP Request | `url: string`, `body: object` | `response: object`, `status: number` | `httpMethod: "GET"`, `headers: "{}"` |

**Trigger (3 nodes)**

| Node | Inputs | Outputs | Properties | Flags |
|------|--------|---------|------------|-------|
| Webhook | (none) | `payload: object`, `headers: object` | (none) | `isTrigger` |
| Timer | (none) | `data: object` | `intervalMs: 60000`, `cron: ""` | `isTrigger` |
| Manual | (none) | `data: object` | `data: "{}"` | `isTrigger` |

### Port Type System

Six port types are derived from Qt C++ types:

| Port Type | Source Qt Types | Color |
|-----------|----------------|-------|
| `string` | `QString`, `QUrl` | `#4caf50` (green) |
| `number` | `int`, `double`, `float`, `qint64`, `quint64` | `#ff9800` (orange) |
| `boolean` | `bool` | `#e91e63` (pink) |
| `bytes` | `QByteArray` | `#9e9e9e` (grey) |
| `array` | `QStringList` | `#00bcd4` (cyan) |
| `object` | `QVariant`, `QVariantMap`, `QVariantList`, `QJsonObject`, `QJsonArray`, `LogosResult`, and all unrecognized types | `#26c6da` (teal) |

Qualifiers (`const`, `&`) are stripped before mapping.

---

## Usability

### API Surface

The module exposes exactly four `Q_INVOKABLE` methods. All return JSON-encoded `QString` values, making them consumable by any language or component that can parse JSON strings:

1. **`getAvailableModules()`** — Returns a JSON array of module descriptors, each containing `name`, `version`, `isLive`, and a `methods` array.

2. **`getModuleDetail(moduleName)`** — Returns the full descriptor for a single module, or `{"error": "Module not found: <name>"}` if the module does not exist.

3. **`getNodeTypeDefinitions()`** — Returns the complete node type catalog as a JSON array. Every entry is self-describing with `nodeTypeId`, `category`, `displayName`, `ports` (with typed and colored input/output definitions), and optional `defaultProperties`.

4. **`refreshModules()`** — Returns `{"status": "ok", "modules": N, "nodeTypes": N}` after re-scanning.

### Self-Describing Node Types

Each node type definition includes all information a consumer needs to render or validate it:
- Unique `nodeTypeId` in `Category/Name` format
- Human-readable `displayName` and `methodDisplayName`
- Typed and colored ports with direction labels
- `defaultProperties` for configurable nodes (e.g., Retry's `maxRetries`, Timer's `intervalMs`)
- Boolean flags (`isTrigger`, `isErrorCatch`, `isLive`) for category-specific rendering

### Event Notification

After every registry rebuild, the module emits a `registryNodeTypesUpdated` event with the new node type count, allowing consumers to reactively update their state.

---

## Reliability

### Graceful Degradation

- **No LogosAPI available**: If `logosAPI` is null, module discovery returns an empty array. Built-in node types are still returned by `getNodeTypeDefinitions()`.
- **No `core_manager`**: Falls back to `capability_module`. If neither is available, returns empty module list.
- **No `lm` binary**: CLI fallback is silently skipped. Logged at debug level.
- **`lm` times out**: Per-plugin timeout of 10 seconds. Failed plugins are skipped with a warning; remaining plugins continue.
- **`lm` returns invalid JSON**: Parse error is logged; the plugin is skipped.
- **Unknown module in `getModuleDetail()`**: Returns `{"error": "Module not found: <name>"}` instead of crashing.
- **No modules loaded**: `getAvailableModules()` returns `[]`. `getNodeTypeDefinitions()` still returns the 24 built-in types.

### No Crash Paths

All introspection paths use defensive checks:
- Null pointer checks on `LogosAPI*` and `LogosAPIClient*` before use
- `QVariant` type checks before conversion (`canConvert<QJsonArray>()`)
- JSON string fallback parsing when direct conversion fails
- Process exit code validation before reading `lm` output

### Dirty Flag

The plugin tracks a `m_dirty` boolean. If set (e.g., after construction before `initLogos`), any read method triggers a rebuild automatically. After `initLogos()` and `refreshModules()`, the cache is populated and subsequent reads are fast.

---

## Performance

### Caching Strategy

- Module introspection and node type building happen once during `initLogos()`.
- Results are cached in `m_modules` (`QJsonArray`) and `m_nodeTypes` (`QJsonArray`).
- `getAvailableModules()`, `getModuleDetail()`, and `getNodeTypeDefinitions()` return cached data without recomputation.
- `refreshModules()` is the only method that triggers a full re-scan.

### Complexity

- **Discovery**: O(n) where n = number of loaded plugins. One IPC call per plugin to `core_manager.getPluginMethods()`.
- **Node type building**: O(n * m) where n = modules and m = average methods per module. Single pass, no sorting.
- **Built-in node construction**: O(1) — fixed set of 24 nodes built once.
- **`getModuleDetail()`**: O(n) linear scan over cached module array.
- **`getNodeTypeDefinitions()`**: O(1) — returns pre-built cached array, serialized to JSON string.

### CLI Fallback Cost

When the `lm` CLI path is used, each plugin requires a subprocess spawn (`QProcess`). This is bounded by a 10-second per-plugin timeout and only runs when the LogosAPI path produces no results.

---

## Supportability

### Dependencies

**Build-time**:
- `logos-module-builder` — provides `LogosModule.cmake` macro and SDK headers
- Qt 6 (pinned by `logos-cpp-sdk`) — `QObject`, `QJsonDocument`, `QProcess`, meta-object system

**Runtime**:
- `LogosAPI` — for module discovery via `core_manager` IPC
- `lm` binary (optional) — for CLI-based introspection fallback

**Module dependencies**: None. The registry has an empty `dependencies` list in `module.yaml`.

### Statelessness

The registry holds no persistent state. It does not write to disk, maintain databases, or preserve data across restarts. All data is derived from the current set of loaded modules at the time of the last `initLogos()` or `refreshModules()` call.

### Configuration

No configuration files. Behavior is determined by:
- Which modules are loaded in `logoscore` at runtime
- Whether `lm` is on `$PATH` (for fallback introspection)
- `$LOGOS_PLUGINS_DIR` environment variable (for CLI scan path)

### Build Chain Position

The registry is the first module in the workflow build chain. It must be built and available before `logos-workflow-engine`, `logos-workflow-canvas`, or `logos-workflow-scheduler`, all of which reference it as a flake input.

### Extensibility

- **Adding built-in node types**: Add a new method to `NodeTypeBuilder` (e.g., `buildMyNodeTypes()`) and call it from `buildNodeTypes()`.
- **Adding port types**: Extend `ModuleIntrospector::mapQtType()` with new Qt type mappings and `colorForType()` with a color.
- **Changing module filtering**: Edit the exclusion lists in `introspectViaAPI()` (line-level `if` checks on plugin names).

### Diagnostics

All significant events are logged via `qDebug()` / `qWarning()` with a `[workflow_registry]`, `[introspector]`, or `[node_type_builder]` prefix:
- Module count after discovery
- Per-module method count
- Skipped modules (no invokable methods, filtered names)
- `lm` failures (timeout, exit code, parse error)
- Registry rebuild summary (module count, node type count)
