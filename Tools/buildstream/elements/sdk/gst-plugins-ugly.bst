kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-ugly/gst-plugins-ugly-1.18.0.tar.xz
  ref: 686644e45e08258ae240c4519376668ad8d34ea6d0f6ab556473c317bfb7e082
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
