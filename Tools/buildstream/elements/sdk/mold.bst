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
  ref: v2.30.0-0-gc7f6a91da512ef8a2634a1bef5d4ba82104659fe
public:
  bst:
    integration-commands:
    - |
      mold --version
