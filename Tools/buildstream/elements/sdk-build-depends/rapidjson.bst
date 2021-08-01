kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_tag
  url: github_com:Tencent/rapidjson
  track: master
  ref: v1.1.0-654-g48fbd8cd202ca54031fe799db2ad44ffa8e77c13
- kind: patch
  path: patches/rapidjson-0001-Fix-build-warnings-emitted-by-GCC-10-on-Aarch64.patch
