kind: meson
sources:
- kind: tar
  url: wpe:libwpe-1.13.90.tar.xz
  ref: 97af9fbea3a70093e5f3979fa89594343e255942645c3b781d4f3517df5388a1
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
