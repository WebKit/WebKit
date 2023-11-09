kind: meson
sources:
- kind: git_repo
  url: github_com:WebPlatformForEmbedded/libwpe
  track: master
  ref: 1.15.1-1-gd7c669ca6f5ec0d544c264016d270669b336c931
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
