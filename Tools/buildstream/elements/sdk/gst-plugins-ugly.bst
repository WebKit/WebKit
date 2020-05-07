kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-ugly/gst-plugins-ugly-1.16.2.tar.xz
  ref: 5500415b865e8b62775d4742cbb9f37146a50caecfc0e7a6fc0160d3c560fbca

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
