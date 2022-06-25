kind: meson
sources:
- kind: tar
  url: https://wpewebkit.org/releases//libwpe-1.13.2.tar.xz
  ref: efcd97dc0f95ff7a506483ba3df944669cdc602b3fc45c9fd676dee0f8f92cac
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- freedesktop-sdk.bst:components/libxkbcommon.bst
- freedesktop-sdk.bst:components/mesa-headers.bst
- freedesktop-sdk.bst:bootstrap-import.bst
public:
  bst:
    split-rules:
      devel:
        (>):
        - '%{libdir}/libwpe-1.0.so'
