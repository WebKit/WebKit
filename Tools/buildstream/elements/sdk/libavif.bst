kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/dav1d.bst

sources:
- kind: git_repo
  url: github_com:AOMediaCodec/libavif
  track: v0.10.1
  ref: v0.10.1-0-gf938f6b7dd412ffcbdab1808a265b6375dc7b037
variables:
  cmake-local: >-
    -DAVIF_CODEC_DAV1D=ON
