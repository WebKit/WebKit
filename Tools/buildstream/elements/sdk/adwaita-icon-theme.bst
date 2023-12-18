kind: meson
sources:
- kind: tar
  url: gnome_downloads:adwaita-icon-theme/45/adwaita-icon-theme-45.0.tar.xz
  ref: 2442bfb06f4e6cc95bf6e2682fdff98fa5eddc688751b9d6215c623cb4e42ff1
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- freedesktop-sdk.bst:components/gtk3.bst
- freedesktop-sdk.bst:components/librsvg.bst
- freedesktop-sdk.bst:bootstrap-import.bst
