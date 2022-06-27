kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-libav/gst-libav-1.20.3.tar.xz
  ref: 3fedd10560fcdfaa1b6462cbf79a38c4e7b57d7f390359393fc0cef6dbf27dfe
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
      PAGER=this-is-not-a-pager gst-inspect-1.0 libav
