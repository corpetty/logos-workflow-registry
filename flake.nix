{
  description = "Logos Workflow Registry - Module introspection and node type definitions";

  inputs = {
    # Floated onto master for the Phase 0 modernization probe: the legacy
    # gate this pin was avoiding is not on current master.
    logos-module-builder.url = "github:logos-co/logos-module-builder";
    nixpkgs.follows = "logos-module-builder/nixpkgs";
  };

  outputs = { self, logos-module-builder, nixpkgs }:
    logos-module-builder.lib.mkLogosModule {
      src = ./.;
      configFile = ./metadata.json;
    };
}
