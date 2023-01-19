kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/zstd.bst
- sdk/tbb.bst

variables:
  cmake-local: |
    -DMOLD_USE_MOLD=OFF \
    -DMOLD_USE_ASAN=OFF \
    -DMOLD_USE_TSAN=OFF \
    -DMOLD_USE_SYSTEM_MIMALLOC=OFF \
    -DMOLD_USE_SYSTEM_TBB=ON \
    -DMOLD_LTO=ON \
    -DMOLD_MOSTLY_STATIC=OFF

sources:
- kind: git_tag
  url: github_com:rui314/mold.git
  checkout-submodules: false
  track-tags: true
  track: main
  ref: v1.9.0-0-gad0b6d0ac6a9b269935c3fbf4dae2815395431a4

public:
  bst:
    integration-commands:
    - |
      mold --version
