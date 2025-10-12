kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- freedesktop-sdk.bst:components/asciidoc.bst
- freedesktop-sdk.bst:components/git-minimal.bst
- sdk/fmtlib.bst
- freedesktop-sdk.bst:components/perl.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/zstd.bst
- sdk/hiredis.bst

variables:
  cmake-local: >-
    -DREDIS_STORAGE_BACKEND=ON
    -DCCACHE_DEV_MODE=OFF
    -DFMT_LIBRARY="%{libdir}/libfmt.a"
    -DENABLE_TESTING=OFF
    -DENABLE_DOCUMENTATION=OFF

sources:
- kind: git_repo
  url: github_com:ccache/ccache.git
  track: v*
  ref: v4.11.3-0-ga50f01935030e46fd6f35184ba7b1f11105967b7
