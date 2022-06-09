kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-ugly/gst-plugins-ugly-1.18.4.tar.xz
  ref: 218df0ce0d31e8ca9cdeb01a3b0c573172cc9c21bb3d41811c7820145623d13c
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
    -Ddvdread=disabled
    -Dmpeg2dec=disabled
    -Dsidplay=disabled
    -Dtests=disabled
    -Ddoc=disabled
