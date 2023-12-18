kind: cmake
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/xorg-lib-x11.bst
- qt5/qtbase.bst

variables:
  cmake-local: >-
    -DENABLE_GUI=TRUE
    -DENABLE_TESTS=FALSE

sources:
- kind: git_repo
  url: github_com:apitrace/apitrace
  track: '11.1'
  ref: 11.1-0-g9352fc02bba106fbbeef9e8452ef34643c0d0764
- kind: git_module
  path: thirdparty/gtest
  url: github_com:google/googletest
  ref: ec44c6c1675c25b9827aacd08c02433cccde7780
- kind: git_module
  path: thirdparty/brotli
  url: github_com:google/brotli
  ref: d6d98957ca8ccb1ef45922e978bb10efca0ea541
- kind: git_module
  path: thirdparty/snappy
  url: github_com:google/snappy
  ref: 537f4ad6240e586970fe554614542e9717df7902
- kind: git_module
  path: thirdparty/libbacktrace
  url: github_com:ianlancetaylor/libbacktrace
  ref: dedbe13fda00253fe5d4f2fb812c909729ed5937
- kind: git_module
  path: thirdparty/libpng
  url: github_com:apitrace/libpng
  ref: d42e88d5b39bf964d4aa4b360655cfa2524e7736
- kind: git_module
  path: thirdparty/zlib
  url: github_com:madler/zlib
  ref: cacf7f1d4e3d44d871b605da3b647f07d718623f
