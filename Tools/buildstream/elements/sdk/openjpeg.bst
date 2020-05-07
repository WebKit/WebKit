kind: cmake
sources:
- kind: tar
  url: github_com:uclouvain/openjpeg/archive/v2.3.1.tar.gz
  ref: 63f5a4713ecafc86de51bfad89cc07bb788e9bba24ebbf0c4ca637621aadb6a9
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
