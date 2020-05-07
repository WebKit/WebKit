kind: meson
sources:
- kind: tar
  url: gnome_downloads:at-spi2-core/2.33/at-spi2-core-2.33.2.tar.xz
  ref: 0e3f44707cccf9ec9098f84abdf91ea654c919eac3c688400f8d261383cc2abe
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- sdk/glib.bst
- sdk/gobject-introspection.bst
- freedesktop-sdk.bst:components/dbus.bst
- freedesktop-sdk.bst:bootstrap-import.bst
public:
  bst:
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libatspi.so'
