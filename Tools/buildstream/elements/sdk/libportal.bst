kind: meson
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/gtk3.bst
- sdk/gtk.bst

sources:
- kind: tar
  url: github_com:flatpak/libportal/releases/download/0.7.1/libportal-0.7.1.tar.xz
  ref: 297b90b263fad22190a26b8c7e8ea938fe6b18fb936265e588927179920d3805
variables:
  meson-local: >-
    -Dbackend-gtk3=enabled
    -Dbackend-gtk4=enabled
    -Dbackend-qt5=disabled
    -Dvapi=false
    -Dportal-tests=false
    -Ddocs=false
    -Dtests=false
