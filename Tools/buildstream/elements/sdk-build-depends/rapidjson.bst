kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_tag
  url: github_com:Tencent/rapidjson
  track: master
  ref: v1.1.0-549-g8f4c021fa2f1e001d2376095928fc0532adf2ae6
