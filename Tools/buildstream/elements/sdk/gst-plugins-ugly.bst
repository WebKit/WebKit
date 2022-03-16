kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-ugly/gst-plugins-ugly-1.20.1.tar.xz
  ref: 42035145e29983308d2828207bb4ef933ed0407bb587fb3a569738c6a57fdb19
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
