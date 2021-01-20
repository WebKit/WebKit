kind: autotools
sources:
- kind: tar
  url: gnome_downloads:adwaita-icon-theme/3.24/adwaita-icon-theme-3.24.0.tar.xz
  ref: ccf79ff3bd340254737ce4d28b87f0ccee4b3358cd3cd5cd11dc7b42f41b272a
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
depends:
- sdk/gtk+-3.bst
- freedesktop-sdk.bst:components/librsvg.bst
- freedesktop-sdk.bst:bootstrap-import.bst
