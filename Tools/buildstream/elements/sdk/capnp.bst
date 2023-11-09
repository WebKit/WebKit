kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_repo
  url: github_com:capnproto/capnproto
  track: v0.10.4
  ref: v0.10.4-0-ga91ec65323c2577732224d0cf6f5bf1e79d3a724
