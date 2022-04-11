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
  ref: 5.5.0-180-g4aca8b4d44618b8c27915c30a2c43cd2cf7b22d0
- kind: patch
  path: patches/rr/0001-cmake-Look-for-resources-in-cmake-s-install-lib-dir.patch
