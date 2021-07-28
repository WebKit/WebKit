kind: cmake

sources:
- kind: git_tag
  url: github_com:MaskRay/ccls.git
  track: '0.20210330'
  ref: 0.20210330-0-g0ada56ef3bb785b9b56eeebb70dff2aad60b3f39

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- sdk-build-depends/rapidjson.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/llvm.bst
