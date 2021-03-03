kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-base/gst-plugins-base-1.18.3.tar.xz
  ref: dbfa20283848f0347a223dd8523dfb62e09e5220b21b1d157a8b0c8b67ba9f52
# Backport of https://gitlab.freedesktop.org/gstreamer/gst-plugins-base/-/merge_requests/1031 to be shipped in 1.18.4
- kind: patch
  path: patches/gst-plugins-base-0001-videodecoder-Fix-racy-critical-when-pool-negotiation.patch
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- freedesktop-sdk.bst:components/gdk-pixbuf.bst
- sdk/gstreamer.bst
- sdk/graphene.bst
- sdk/gtk+-3.bst
- freedesktop-sdk.bst:components/pango.bst
- sdk/libdrm.bst
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
