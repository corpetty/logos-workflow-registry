#pragma once

#include <memory>
#include <string>

#include "logos_module_context.h"

class RegistryBridge;
class WorkflowRegistryProviderObject;

/**
 * @brief The Workflow Registry module.
 *
 * Produces the node palette the canvas renders and the engine executes:
 * one node type per introspected module method, plus the built-in
 * utility / control-flow / transform / trigger nodes.
 *
 * Discovery in two halves, because a universal module is confined to its
 * declared dependencies and `core_manager` no longer exists:
 *
 *   1. WHICH modules exist — read off disk. `modulePath()` (from
 *      LogosModuleContext) is this module's own install directory, so its
 *      PARENT is the host's modules directory and each sibling is an
 *      installed module carrying a manifest.
 *   2. WHAT each one exposes — asked of the module itself, over the raw
 *      LogosAPI the generated glue hands us in onInit, via the
 *      `getPluginMethods` / `getPluginEvents` surface every generated
 *      module publishes.
 *
 * Both halves live behind RegistryBridge so this class stays Qt-free.
 */
class WorkflowRegistryImpl : public LogosModuleContext
{
    friend class WorkflowRegistryProviderObject;

public:
    WorkflowRegistryImpl();
    ~WorkflowRegistryImpl();

    /// JSON array of module descriptors (name, version, methods, events).
    std::string getAvailableModules();

    /// JSON object describing one module, or {"error": ...} if unknown.
    std::string getModuleDetail(const std::string& moduleName);

    /// JSON array of every node type — module methods plus the built-ins.
    std::string getNodeTypeDefinitions();

    /// Force a re-scan. Returns {"status","modules","nodeTypes"}.
    std::string refreshModules();

logos_events:
    /// Emitted whenever the palette is rebuilt, with the new node-type count.
    /// The canvas subscribes to this to refresh without polling.
    void registryNodeTypesUpdated(int64_t nodeTypeCount);

protected:
    /// modulePath() becomes readable here — the disk half of discovery
    /// needs it, so the first build waits for this rather than running
    /// in the constructor.
    void onContextReady() override;

private:
    struct State;
    std::unique_ptr<State> m_state;

    /// Installed by the generated glue's onInit. Not a module API method.
    void setBridge(std::shared_ptr<RegistryBridge> bridge);

    void buildRegistry();
};
