kind: meson
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/mesa-headers.bst
- freedesktop-sdk.bst:components/glslang.bst
- freedesktop-sdk.bst:components/sdl2.bst
- freedesktop-sdk.bst:components/vulkan-headers.bst
- freedesktop-sdk.bst:components/vulkan-icd-loader.bst
- sdk/eigen.bst
- sdk/gst-plugins-base.bst

variables:
  meson-local: >-
    -Ddocs=disabled
    -Dtracking=disabled

public:
  bst:
    integration-commands:
    - |
      monado-cli test

sources:
- kind: git_tag
  url: gitlab_freedesktop_org:monado/monado
  track: master
  ref: v0.4.1-305-g948101308104052aec0173d3166fcfb72dbfc88b
