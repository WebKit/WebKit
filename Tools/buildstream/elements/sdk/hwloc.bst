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
  url: https://download.open-mpi.org/release/hwloc/v2.8/hwloc-2.8.0.tar.bz2
  ref: 348a72fcd48c32a823ee1da149ae992203e7ad033549e64aed6ea6eeb01f42c1
