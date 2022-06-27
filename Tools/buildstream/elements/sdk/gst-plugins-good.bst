kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-good/gst-plugins-good-1.20.3.tar.xz
  ref: f8f3c206bf5cdabc00953920b47b3575af0ef15e9f871c0b6966f6d0aa5868b7
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- freedesktop-sdk.bst:components/gdk-pixbuf.bst
- sdk/gstreamer.bst
- sdk/gst-plugins-base.bst
- sdk/gtk+-3.bst
- freedesktop-sdk.bst:components/libsoup.bst
- freedesktop-sdk.bst:components/v4l-utils.bst
- qt5/qtdeclarative.bst
- qt5/qtquickcontrols2.bst
- qt5/qtquickcontrols.bst
- freedesktop-sdk.bst:components/cairo.bst
- freedesktop-sdk.bst:components/jpeg.bst
- freedesktop-sdk.bst:components/lame.bst
- freedesktop-sdk.bst:components/libflac.bst
- freedesktop-sdk.bst:components/libpng.bst
- freedesktop-sdk.bst:components/libpulse.bst
- freedesktop-sdk.bst:components/libvpx.bst
- freedesktop-sdk.bst:components/mpg123.bst
- freedesktop-sdk.bst:components/nasm.bst
- freedesktop-sdk.bst:components/orc.bst
- freedesktop-sdk.bst:components/speex.bst
- freedesktop-sdk.bst:components/wavpack.bst
- freedesktop-sdk.bst:bootstrap-import.bst
variables:
  meson-local: >-
    -Dpackage-origin="webkit-sdk"
    -Daalib=disabled
    -Ddoc=disabled
    -Dexamples=disabled
    -Djack=disabled
    -Dlibcaca=disabled
    -Ddv=disabled
    -Ddv1394=disabled
    -Drpicamsrc=disabled
    -Dshout2=disabled
    -Dtaglib=disabled
    -Dtwolame=disabled
    -Dv4l2-gudev=disabled
