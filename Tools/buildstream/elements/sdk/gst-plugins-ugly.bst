kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-ugly/gst-plugins-ugly-1.20.2.tar.xz
  ref: b43fb4df94459afbf67ec22003ca58ffadcd19e763f276dca25b64c848adb7bf
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- sdk/gst-plugins-base.bst
- freedesktop-sdk.bst:components/orc.bst
- freedesktop-sdk.bst:bootstrap-import.bst
- sdk/x264.bst
variables:
  meson-local: >-
    -Dpackage-origin="webkit-sdk"
    -Da52dec=disabled
    -Damrnb=disabled
    -Damrwbdec=disabled
    -Dcdio=disabled
    -Ddoc=disabled
    -Ddvdread=disabled
    -Dgpl=enabled
    -Dmpeg2dec=disabled
    -Dsidplay=disabled
    -Dtests=disabled
