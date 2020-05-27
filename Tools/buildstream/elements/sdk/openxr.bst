kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- freedesktop-sdk.bst:components/vulkan-headers.bst
- freedesktop-sdk.bst:components/mesa-headers.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/xorg-lib-x11.bst
- freedesktop-sdk.bst:components/xorg-lib-xxf86vm.bst
- freedesktop-sdk.bst:components/vulkan.bst

sources:
- kind: git_tag
  url: github_com:KhronosGroup/OpenXR-SDK-Source.git
  track: master
  ref: release-1.0.8-26-g3e40561410c72a56c1b55a5f421a6bd5f9c1b490
- kind: patch
  path: patches/openxr-0001-cmake-Check-for-C-17-and-conditionally-enable-it.patch

variables:
  cmake-local: >-
    -DBUILD_TESTS=OFF
