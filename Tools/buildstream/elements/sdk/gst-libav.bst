kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-libav/gst-libav-1.20.1.tar.xz
  ref: 91a71fb633b75e1bd52e22a457845cb0ba563a2972ba5954ec88448f443a9fc7
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
