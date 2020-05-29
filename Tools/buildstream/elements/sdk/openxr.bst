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
  ref: release-1.0.8-47-g38c25761c6ecdcbf16fbcae7953b3421a6b268ac

variables:
  cmake-local: >-
    -DBUILD_TESTS=OFF
