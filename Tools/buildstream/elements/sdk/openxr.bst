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
  url: github_com:KhronosGroup/OpenXR-SDK.git
  track: master
  ref: release-1.0.9-0-g97cfe495bb7a3853266b646d1c79e169387f9c7a

variables:
  cmake-local: >-
    -DBUILD_TESTS=OFF
