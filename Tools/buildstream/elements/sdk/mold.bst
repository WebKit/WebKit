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
- kind: git_repo
  url: github_com:rui314/mold.git
  track: v2.*
  ref: v2.33.0-0-g1ab637cea4e76401d7a85039f68be8cd10d58324
public:
  bst:
    integration-commands:
    - |
      mold --version
