kind: meson

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
- freedesktop-sdk.bst:components/nasm.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

variables:
  meson-local: >-
    -Denable_tests=false
    -Dxxhash_muxer=disabled

public:
  bst:
    split-rules:
      devel:
        (>):
        - '%{libdir}/libdav1d.so'

sources:
- kind: git_repo
  url: https://code.videolan.org/videolan/dav1d.git
  track: '*.*.*'
  ref: 1.4.1-0-g872e470ebf3e65b0b956f3a70329e885a2df1c2a
