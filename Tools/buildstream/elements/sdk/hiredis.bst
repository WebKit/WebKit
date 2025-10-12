kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

variables:
  cmake-local: >-
    -DENABLE_NUGET=OFF
    -DENABLE_SSL=ON
    -DDISABLE_TESTS=ON

sources:
- kind: git_repo
  url: github_com:redis/hiredis.git
  track: master
  ref: v1.3.0-4-g294782041ef5841293dea3acc8ca9a28597bedc8
