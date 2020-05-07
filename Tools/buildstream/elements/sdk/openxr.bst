kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/xorg-lib-x11.bst

sources:
- kind: git_tag
  url: github_com:KhronosGroup/OpenXR-SDK-Source.git
  track: master
  ref: release-1.0.8-23-g38dca88abef5cfe1eb7957cbaea769221626f3c5
- kind: patch
  path: patches/openxr-0001-cmake-Check-for-C-17-and-conditionally-enable-it.patch

variables:
  cmake-local: >-
    -DBUILD_TESTS=OFF
