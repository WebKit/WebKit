kind: cmake
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- freedesktop-sdk.bst:components/python3.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- sdk/capnp.bst

variables:
  cmake-local: |
    -Ddisable32bit=ON \
    -DBUILD_TESTS=OFF
sources:
- kind: git_repo
  url: github_com:rr-debugger/rr.git
  track: master
  ref: 27d2cb542c8eeb6d9206d46bf93d661690681f46
- kind: patch
  path: patches/rr/0001-cmake-Look-for-resources-in-cmake-s-install-lib-dir.patch
