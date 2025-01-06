kind: meson

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- freedesktop-sdk.bst:components/glib.bst

variables:
  meson-local: >-
    -Ddocs=false
    -Dexamples=false
    -Dsysprof=false
    -Dtests=false
    -Dvapi=false
    -Deventfd=enabled
    -Dliburing=disabled
    -Dintrospection=disabled

sources:
- kind: tar
  url: gnome_downloads:libdex/0.6/libdex-0.6.1.tar.xz
  ref: d176de6578571e32a8c0b603b6a5a13fa5f87fb6b5442575b38ec5af16b17a92

public:
  bst:
    integration-commands:
    - |
      test $(pkg-config --modversion libdex-1) = 0.6.1
