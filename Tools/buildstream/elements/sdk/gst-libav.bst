kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-libav/gst-libav-1.20.4.tar.xz
  ref: 04ccbdd58fb31dd94098da599209834a0e7661638c5703381dd0a862c56fc532
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
