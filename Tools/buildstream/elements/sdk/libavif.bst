kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/dav1d.bst

sources:
- kind: git_tag
  url: github_com:AOMediaCodec/libavif
  track: v0.9.0
  ref: v0.9.0-0-g4fedf0bb08229767d75ebe7d00b272e131aaa578
variables:
  cmake-local: >-
    -DAVIF_CODEC_DAV1D=ON
