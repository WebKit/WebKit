kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-libav/gst-libav-1.18.4.tar.xz
  ref: 344a463badca216c2cef6ee36f9510c190862bdee48dc4591c0a430df7e8c396
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
