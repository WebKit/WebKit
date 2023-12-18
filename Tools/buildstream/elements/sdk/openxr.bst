kind: cmake
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- freedesktop-sdk.bst:components/vulkan-headers.bst
- freedesktop-sdk.bst:components/mesa-headers.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/xorg-lib-x11.bst
- freedesktop-sdk.bst:components/xorg-lib-xxf86vm.bst

sources:
- kind: git_repo
  url: github_com:KhronosGroup/OpenXR-SDK.git
  track: main
  ref: release-1.0.30-0-g55224479ab13db8ebc8ab1e3d49197bce6201b0b
variables:
  cmake-local: >-
    -DBUILD_TESTS=OFF
