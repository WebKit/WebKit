kind: autotools
sources:
- kind: tar
  url: gnome_downloads:adwaita-icon-theme/40/adwaita-icon-theme-40.1.1.tar.xz
  ref: 0b6c436ed6ad9887a88ada1f72a0197b1eb73b020d8d344abab4c7fa7250f8f6
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
depends:
- sdk/gtk+-3.bst
- freedesktop-sdk.bst:components/librsvg.bst
- freedesktop-sdk.bst:bootstrap-import.bst
