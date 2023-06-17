kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

variables:
  cmake-local: |
    -DHWY_WARNINGS_ARE_ERRORS=OFF \
    -DHWY_ENABLE_EXAMPLES=OFF \
    -DHWY_ENABLE_CONTRIB=OFF \
    -DHWY_ENABLE_TESTS=OFF \
    -DHWY_ENABLE_INSTALL=ON

sources:
- kind: tar
  url: https://github.com/google/highway/archive/refs/tags/1.0.4.tar.gz
  ref: faccd343935c9e98afd1016e9d20e0b8b89d908508d1af958496f8c2d3004ac2
