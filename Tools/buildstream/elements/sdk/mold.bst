kind: make
depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- sdk/tbb.bst

variables:
  make: make PREFIX="%{prefix}" LTO=1 SYSTEM_TBB=1

sources:
- kind: git_tag
  url: github_com:rui314/mold.git
  checkout-submodules: false
  track-tags: true
  track: main
  ref: v1.2.1-0-gc8d8f86a52084c96e2663d9f692c51e98c04cc2f
