kind: cmake

sources:
- kind: git_tag
  url: github_com:MaskRay/ccls.git
  track: '0.20220729'
  ref: 0.20220729-0-g74458915b3472b0df26264d7d1599505385cf453

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- sdk-build-depends/rapidjson.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/llvm.bst
