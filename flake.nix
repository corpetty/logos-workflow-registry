{
  description = "Logos Workflow Registry - Module introspection and node type definitions";

  inputs = {
    # Pinned, not floating. The builder throws on `interface: "legacy"` for a
    # core module that ships a plugin (lib/modulePreConfigure.nix) as of
    # 2026-08-20; this module is a handcrafted Qt plugin, so it needs the last
    # commit before that. Unpin once it is ported to `interface: "universal"`
    # (a plain src/<name>_impl.h the generator derives the contract from).
    logos-module-builder.url = "github:logos-co/logos-module-builder/f007edf1d7dc";
    nixpkgs.follows = "logos-module-builder/nixpkgs";
  };

  outputs = { self, logos-module-builder, nixpkgs }:
    logos-module-builder.lib.mkLogosModule {
      src = ./.;
      configFile = ./metadata.json;
    };
}
