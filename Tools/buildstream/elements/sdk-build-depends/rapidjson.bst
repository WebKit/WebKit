kind: cmake
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

# https://github.com/Tencent/rapidjson/issues/1924
variables:
  cmake-local: >-
    -DCMAKE_CXX_FLAGS=-Wno-error=stringop-overflow

sources:
- kind: git_tag
  url: github_com:Tencent/rapidjson
  track: master
  ref: v1.1.0-665-g00dbcf2c6e03c47d6c399338b6de060c71356464
- kind: patch
  path: patches/rapidjson-0001-Fix-build-warnings-emitted-by-GCC-10-on-Aarch64.patch
