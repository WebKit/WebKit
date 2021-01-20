kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-base/gst-plugins-base-1.18.0.tar.xz
  ref: 762abdd1a950809a1cea62fff7f86b5f7d6bd5f6841e3e585c700b823cdb7897
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- freedesktop-sdk.bst:components/gdk-pixbuf.bst
- sdk/gstreamer.bst
- sdk/graphene.bst
- sdk/gtk+-3.bst
- freedesktop-sdk.bst:components/pango.bst
- sdk/libsoup.bst
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/alsa-lib.bst
- freedesktop-sdk.bst:components/iso-codes.bst
- freedesktop-sdk.bst:components/libogg.bst
- freedesktop-sdk.bst:components/libtheora.bst
- freedesktop-sdk.bst:components/libvorbis.bst
- freedesktop-sdk.bst:components/mesa-headers.bst
- freedesktop-sdk.bst:components/opus.bst
- freedesktop-sdk.bst:components/orc.bst
- freedesktop-sdk.bst:components/sdl2.bst
- freedesktop-sdk.bst:components/xorg-lib-xv.bst

variables:
  meson-local: >-
    -Dintrospection=disabled
    -Dpackage-origin="webkit-sdk"
    -Dcdparanoia=disabled
    -Dlibvisual=disabled
    -Dtremor=disabled
    -Dexamples=disabled
    -Dgtk_doc=disabled
    -Ddoc=disabled
    -Dtests=disabled

public:
  bst:
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libgstallocators-1.0.so'
        - '%{libdir}/libgstapp-1.0.so'
        - '%{libdir}/libgstaudio-1.0.so'
        - '%{libdir}/libgstfft-1.0.so'
        - '%{libdir}/libgstgl-1.0.so'
        - '%{libdir}/libgstpbutils-1.0.so'
        - '%{libdir}/libgstriff-1.0.so'
        - '%{libdir}/libgstrtp-1.0.so'
        - '%{libdir}/libgstrtsp-1.0.so'
        - '%{libdir}/libgstsdp-1.0.so'
        - '%{libdir}/libgsttag-1.0.so'
        - '%{libdir}/libgstvideo-1.0.so'
