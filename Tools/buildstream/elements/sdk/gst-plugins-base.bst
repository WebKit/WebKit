kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-base/gst-plugins-base-1.20.4.tar.xz
  ref: 8d181b7abe4caf23ee9f9ec5b4d3e232640452464e39495bfffb6d776fc97225
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- freedesktop-sdk.bst:components/gdk-pixbuf.bst
- sdk/gstreamer.bst
- sdk/graphene.bst
- sdk/gtk+-3.bst
- freedesktop-sdk.bst:components/pango.bst
- freedesktop-sdk.bst:components/libdrm.bst
- freedesktop-sdk.bst:components/libsoup.bst
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
- freedesktop-sdk.bst:components/wayland-protocols.bst

variables:
  meson-local: >-
    -Dintrospection=disabled
    -Dpackage-origin="webkit-sdk"
    -Dcdparanoia=disabled
    -Dlibvisual=disabled
    -Dtremor=disabled
    -Dexamples=disabled
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
