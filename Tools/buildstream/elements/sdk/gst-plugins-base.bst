kind: meson
sources:
- kind: tar
  url: gst_downloads:gst-plugins-base/gst-plugins-base-1.16.2.tar.xz
  ref: b13e73e2fe74a4166552f9577c3dcb24bed077021b9c7fa600d910ec6987816a
- kind: patch
  path: patches/gst-plugins-base-0001-playbin-Handle-error-message-with-redirection-indica.patch
- kind: patch
  path: patches/gst-plugins-base-0001-glupload-Fix-fallback-from-direct-dmabuf-to-dmabuf-u.patch
- kind: patch
  path: patches/gst-plugins-base-0002-glupload-fix-segfault.patch
- kind: patch
  path: patches/gst-plugins-base-0003-glbasefilter-add-support-for-changing-the-display.patch
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
