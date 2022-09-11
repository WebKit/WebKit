kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_tag
  url: github_com:capnproto/capnproto
  track: v0.10.2
  ref: v0.10.2-0-g2e88221d3dde22266bfccf40eaee6ff9b40d113d
