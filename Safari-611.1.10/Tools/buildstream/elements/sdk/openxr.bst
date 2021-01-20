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
  ref: release-1.0.11-0-ge3a4e41d61544d8e2eba73f00da99b6818ec472b

variables:
  cmake-local: >-
    -DBUILD_TESTS=OFF
