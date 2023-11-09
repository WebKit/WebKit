kind: cmake
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/mesa-headers.bst
- freedesktop-sdk.bst:components/glslang.bst
- freedesktop-sdk.bst:components/sdl2.bst
- freedesktop-sdk.bst:components/vulkan-headers.bst
- freedesktop-sdk.bst:components/vulkan-icd-loader.bst
- sdk/eigen.bst
- freedesktop-sdk.bst:components/gstreamer-plugins-base.bst

public:
  bst:
    integration-commands:
    - |
      monado-cli test

sources:
- kind: git_repo
  url: gitlab_freedesktop_org:monado/monado
  track: main
  ref: v21.0.0-3752-gc1e3d83b5ba267f840913f5f5456dca1a266ee2b
