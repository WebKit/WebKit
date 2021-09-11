kind: cmake
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- freedesktop-sdk.bst:components/vulkan-headers.bst
- freedesktop-sdk.bst:components/mesa-headers.bst
- freedesktop-sdk.bst:components/llvm.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/xorg-lib-x11.bst
- freedesktop-sdk.bst:components/xorg-lib-xxf86vm.bst

sources:
- kind: git_tag
  url: github_com:KhronosGroup/OpenXR-SDK.git
  track: master
  ref: release-1.0.19-0-g6e0ca79dad30d160285c62a1e06aaff55343392a
variables:
  cmake-local: >-
    -DBUILD_TESTS=OFF
    -DCMAKE_C_COMPILER=/usr/bin/clang
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++
