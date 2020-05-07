kind: cmake
sources:
- kind: tar
  url: github_com:WebPlatformForEmbedded/libwpe/releases/download/1.6.0/libwpe-1.6.0.tar.xz
  ref: 3587c6b8a807f4bb76b268ba74ca82c6b395b90235db41ad8252224456193c90
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
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
