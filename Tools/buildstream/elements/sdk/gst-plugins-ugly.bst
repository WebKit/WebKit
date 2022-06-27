kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-ugly/gst-plugins-ugly-1.20.3.tar.xz
  ref: 8caa20789a09c304b49cf563d33cca9421b1875b84fcc187e4a385fa01d6aefd
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
