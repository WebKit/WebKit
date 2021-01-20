kind: meson
sources:
- kind: tar
  url: github_com:WebPlatformForEmbedded/libwpe/releases/download/1.8.0/libwpe-1.8.0.tar.xz
  ref: a6f00a7d091cbd4db57fe7ee3b4c12c6350921d654ed79812800a26c888481d2
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
