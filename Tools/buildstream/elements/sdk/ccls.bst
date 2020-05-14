kind: cmake

sources:
- kind: git_tag
  url: github_com:MaskRay/ccls.git
  track: 0.20190823.6
  ref: 0.20190823.6-0-gb3d694b8bd4105ffe4e57f9ae290245aea6ef0fc

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- sdk-build-depends/rapidjson.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/llvm.bst
