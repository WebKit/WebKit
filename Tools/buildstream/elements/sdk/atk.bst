kind: meson
sources:
- kind: tar
  url: gnome_downloads:atk/2.33/atk-2.33.3.tar.xz
  ref: 532d1081e87b9f0a8d71733101e791818442fa1896531621d1aecc189e1a4ffe
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- sdk/glib.bst
- sdk/gobject-introspection.bst
- freedesktop-sdk.bst:bootstrap-import.bst
public:
  bst:
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libatk-1.0.so'
