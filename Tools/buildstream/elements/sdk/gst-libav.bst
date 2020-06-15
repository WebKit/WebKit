kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-libav/gst-libav-1.16.2.tar.xz
  ref: c724f612700c15a933c7356fbeabb0bb9571fb5538f8b1b54d4d2d94188deef2
- kind: patch
  path: patches/libav-0001-gstavviddec-Limit-default-number-of-decoder-threads.patch
- kind: patch
  path: patches/gst-libav-stop-caching-codecs.patch

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
- freedesktop-sdk.bst:components/nasm.bst
depends:
- sdk/ffmpeg.bst
- sdk/gst-plugins-base.bst
- freedesktop-sdk.bst:components/orc.bst
- freedesktop-sdk.bst:bootstrap-import.bst

variables:
  meson-local: |
    '-Dc_args=-DTRIPLET=\"%{gcc_triplet}\"'

public:
  bst:
    overlap-whitelist:
    - '**'

    integration-commands:
    - |
      gst-inspect-1.0 libav
