kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/dav1d.bst

sources:
- kind: git_tag
  url: github_com:AOMediaCodec/libavif
  track: v0.7.3
  ref: v0.7.3-0-g70f4122a7e26411e9527574a0dae75c4bac68e89
variables:
  cmake-local: >-
    -DAVIF_CODEC_DAV1D=ON
