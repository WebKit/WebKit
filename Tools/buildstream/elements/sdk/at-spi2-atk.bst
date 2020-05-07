kind: meson
sources:
- kind: tar
  url: gnome_downloads:at-spi2-atk/2.33/at-spi2-atk-2.33.2.tar.xz
  ref: 64d4351b245d717ec6f4f81fd40fe67481ff8e62d3893d465f01e71d9a3db3d3
build-depends:
- freedesktop-sdk.bst:components/libxml2.bst
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- sdk/at-spi2-core.bst
- sdk/atk.bst
- sdk/glib.bst
- freedesktop-sdk.bst:bootstrap-import.bst
public:
  bst:
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libatk-bridge-2.0.so'
