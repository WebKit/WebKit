kind: cmake

sources:
- kind: git_tag
  url: github_com:MaskRay/ccls.git
  track: '0.20201025'
  ref: 0.20201025-0-g1fec76c804a386e57af543baf5a337779eb78e81

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- sdk-build-depends/rapidjson.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/llvm.bst
