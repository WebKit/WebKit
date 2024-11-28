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
  url: https://download.open-mpi.org/release/hwloc/v2.11/hwloc-2.11.1.tar.bz2
  ref: 04cdfbffad225ce15f66184f0f4141327dabf288d10a8b84d13f517acb7870c6

public:
  bst:
    integration-commands:
    - |
      hwloc-info --version
