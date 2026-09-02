#pragma once

#include <string>

#include "registry_bridge.h"

/**
 * RegistryBridge over the logos-protocol lp_* C ABI (logos::LpClient).
 *
 * Qt-free by construction: a universal module is compiled as a cdylib whose
 * own translation units must not touch Qt, and — unlike the pre-split world
 * this registry was written for — it is never handed a `LogosAPI`. LpClient
 * is the supported way to call a module chosen at RUNTIME: it takes the
 * target's name as a string and carries the capability/token flow itself,
 * which is exactly what a workflow registry needs and what the generated
 * `modules().<dep>` accessors (declared dependencies only) cannot give.
 */
class RegistryBridgeLp : public RegistryBridge {
public:
    /// @param origin this module's own name — every outbound lp call is made
    ///        on behalf of it, and the capability layer gates on it.
    explicit RegistryBridgeLp(std::string origin);
    ~RegistryBridgeLp() override = default;

    std::vector<DiscoveredModule> discoverModules(const std::string& modulesDir) override;

private:
    std::string m_origin;
};
