kind: meson
sources:
- kind: tar
  url: github_com:WebPlatformForEmbedded/libwpe/releases/download/1.10.0/libwpe-1.10.0.tar.xz
  ref: 2415e270d45e3595ed4052bc105f733744dc2d3677e12ff4a831e5029841084d
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
