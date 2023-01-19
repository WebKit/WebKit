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
  url: https://download.open-mpi.org/release/hwloc/v2.9/hwloc-2.9.0.tar.bz2
  ref: 2070e963596a2421b9af8eca43bdec113ee1107aaf7ccb475d4d3767a8856887

public:
  bst:
    integration-commands:
    - |
      hwloc-info --version
