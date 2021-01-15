kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-ugly/gst-plugins-ugly-1.18.2.tar.xz
  ref: a337a022ed472209b38cab5918dc2f4e7a6adc1f4afacd226a6345e5cb668bdb
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
