kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

variables:
  conf-local: >-
    --disable-cairo
    --disable-cuda
    --disable-picky
    --enable-plugins
    --without-x

sources:
- kind: tar
  url: https://www.open-mpi.org/software/hwloc/v2.7/downloads/hwloc-2.7.1.tar.bz2
  ref: 0d4e1d36c3a72c5d61901bfd477337f5a4c7e0a975da57165237d00e35ef528d
