kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- freedesktop-sdk.bst:components/pkg-config.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

variables:
  cmake-local: |
    -DENABLE_LIB_ONLY=ON

sources:
- kind: git_tag
  url: github_com:nghttp2/nghttp2
  track: v1.43.0
  ref: v1.43.0-0-gfb0bd22979711c89d92d98ac5db57ebc6f4fd148
