kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-libav/gst-libav-1.20.2.tar.xz
  ref: b5c531dd8413bf771c79dab66b8e389f20b3991f745115133f0fa0b8e32809f9
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
