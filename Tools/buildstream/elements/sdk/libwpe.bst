kind: meson
sources:
- kind: tar
  url: wpe:libwpe-1.13.3.tar.xz
  ref: 05f871922f6ca750c5689a38a346c3fba130417d3490dd52362b4fe22f334e96
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
