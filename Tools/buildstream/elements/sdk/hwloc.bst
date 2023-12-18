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
  url: https://download.open-mpi.org/release/hwloc/v2.9/hwloc-2.9.3.tar.bz2
  ref: 5c4062ce556f6d3451fc177ffb8673a2120f81df6835dea6a21a90fbdfff0dec

public:
  bst:
    integration-commands:
    - |
      hwloc-info --version
