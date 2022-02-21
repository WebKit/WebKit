kind: make
depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

variables:
  make: make PREFIX="%{prefix}" LTO=1
sources:
- kind: git_tag
  url: github_com:rui314/mold.git
  checkout-submodules: false
  track-tags: true
  track: main
  ref: v1.0.2-0-g6fb64656b92553ae47a9b058da8c93083be779d5
