kind: cmake
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- sdk/capnp.bst

variables:
  cmake-local: |
    -Ddisable32bit=ON \
    -DBUILD_TESTS=OFF
sources:
- kind: git_tag
  url: github_com:rr-debugger/rr
  track: master
  ref: 5.6.0-25-ge231941fa978e73f29a2a3a346e2892453ac767e
- kind: patch
  path: patches/rr/0001-cmake-Look-for-resources-in-cmake-s-install-lib-dir.patch
