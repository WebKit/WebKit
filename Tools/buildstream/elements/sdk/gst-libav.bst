kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-libav/gst-libav-1.18.3.tar.xz
  ref: ad20546bcd78ac1e7cf194666d73c4f33efeb62647d2b6af22993b540699e91c
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
