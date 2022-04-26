kind: autotools
sources:
- kind: tar
  url: gnome_downloads:adwaita-icon-theme/42/adwaita-icon-theme-42.0.tar.xz
  ref: 5e85b5adc8dee666900fcaf271ba717f7dcb9d0a03d96dae08f9cbd27e18b1e0
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
depends:
- sdk/gtk+-3.bst
- freedesktop-sdk.bst:components/librsvg.bst
- freedesktop-sdk.bst:bootstrap-import.bst
