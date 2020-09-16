kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-libav/gst-libav-1.18.0.tar.xz
  ref: 42f93f5ce9a3fc22051e5f783a4574b56ebf213f331f75dcbc3552459bd3a06a
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
