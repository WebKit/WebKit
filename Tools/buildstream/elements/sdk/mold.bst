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
  ref: v2.3.1-0-g34c53777fbb4fe6bff59ed745321d92dbf7e4fab
public:
  bst:
    integration-commands:
    - |
      mold --version
