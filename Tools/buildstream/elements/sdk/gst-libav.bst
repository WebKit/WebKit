kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-libav/gst-libav-1.18.2.tar.xz
  ref: 36969ad44c5f0756a8a90215410710d6c39713d58f6cee13d663be9774557f49
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
- freedesktop-sdk.bst:components/nasm.bst
depends:
- sdk/ffmpeg.bst
- sdk/gst-plugins-base.bst
- freedesktop-sdk.bst:components/orc.bst
- freedesktop-sdk.bst:bootstrap-import.bst

variables:
  meson-local: >-
    -Dc_args=-DTRIPLET=\"%{gcc_triplet}\"
    -Ddoc=disabled

public:
  bst:
    overlap-whitelist:
    - '**'

    integration-commands:
    - |
      gst-inspect-1.0 libav
