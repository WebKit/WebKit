kind: cmake
sources:
- kind: tar
  url: github_com:uclouvain/openjpeg/archive/v2.4.0.tar.gz
  ref: 8702ba68b442657f11aaeb2b338443ca8d5fb95b0d845757968a7be31ef7f16d
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
depends:
- freedesktop-sdk.bst:bootstrap-import.bst
variables:
  cmake-local: >-
    -DOPENJPEG_INSTALL_LIB_DIR=%{lib}
public:
  bst:
    split-rules:
      devel:
        (>):
        - '%{libdir}/libopenjp2.so'
        - '%{libdir}/openjpeg-2.3'
        - '%{libdir}/openjpeg-2.3/**'
