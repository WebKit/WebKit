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
  url: github_com:google/highway/releases/download/1.2.0/highway-1.2.0.tar.gz
  ref: 58e9d5d41d6573ad15245ad76aec53a69499ca7480c092d899c4424812ed906f
