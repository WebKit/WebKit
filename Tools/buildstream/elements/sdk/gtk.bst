kind: meson

sources:
- kind: git_tag
  url: gitlab_gnome_org:GNOME/gtk.git
  track: master
  track-tags: true
  ref: 3.99.5.1-0-g398a74d8712d41eecd15bf8cf4efcdb50f4dd423
build-depends:
- sdk-build-depends/sassc.bst
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- freedesktop-sdk.bst:components/at-spi2-atk.bst
- freedesktop-sdk.bst:components/atk.bst
- freedesktop-sdk.bst:components/gdk-pixbuf.bst
- sdk/glib.bst
- sdk/graphene.bst
- sdk/gst-plugins-bad.bst
- sdk/gst-plugins-base.bst
- sdk/gstreamer.bst
- freedesktop-sdk.bst:components/pango.bst
- freedesktop-sdk.bst:components/cups.bst
- freedesktop-sdk.bst:components/hicolor-icon-theme.bst
- freedesktop-sdk.bst:components/libepoxy.bst
- freedesktop-sdk.bst:components/libxkbcommon.bst
- freedesktop-sdk.bst:components/wayland-protocols.bst
- freedesktop-sdk.bst:components/wayland.bst
- freedesktop-sdk.bst:bootstrap-import.bst

variables:
  meson-local: >-
    -Denable-broadway-backend=true
    -Dprint-cloudprint=disabled
    -Dmedia-ffmpeg=disabled
    -Dvulkan=disabled

public:
  bst:
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libgtk-4.so'
