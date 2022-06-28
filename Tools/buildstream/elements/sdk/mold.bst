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
  ref: v1.3.0-0-ga9a8205be29f36346bfdbfadee8ed147e06aa1f6
