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
  ref: v1.10.1-0-g040180f933d33fa246f9d2961c2d6e8b74241463

public:
  bst:
    integration-commands:
    - |
      mold --version
