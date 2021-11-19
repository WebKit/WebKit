kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

variables:
  cmake-local: |
    -DBUILD_TESTING=OFF \
    -DHWY_WARNINGS_ARE_ERRORS=OFF \
    -DHWY_ENABLE_EXAMPLES=OFF \
    -DHWY_ENABLE_INSTALL=ON

sources:
- kind: tar
  url: https://github.com/google/highway/archive/refs/tags/0.15.0.tar.gz
  ref: 4bbd4439eae08cf038f1c5cc5d9ebc6a1a50f2c610c13a1483adccacfa24c150
