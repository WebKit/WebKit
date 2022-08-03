kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

variables:
  cmake-local: >-
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_CXX_STANDARD=17

sources:
- kind: git_tag
  url: github_com:abseil/abseil-cpp.git
  track: lts_2021_03_24
  ref: 20210324.2-0-g278e0a071885a22dcd2fd1b5576cc44757299343
