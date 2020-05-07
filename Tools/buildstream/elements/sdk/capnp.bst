kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_tag
  url: github_com:capnproto/capnproto
  track: v0.7.0
  ref: v0.7.0-0-g3f44c6db0f0f6c0cab0633f15f15d0a2acd01d19
