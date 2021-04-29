kind: meson
sources:
- kind: tar
  url: gnome_downloads:libsoup/2.99/libsoup-2.99.3.tar.xz
  ref: ed0f81490f78a3f4667531220b5500379bc1225cd518bb4f4153741d4bdbcc4a
build-depends:
- freedesktop-sdk.bst:components/gobject-introspection.bst
- freedesktop-sdk.bst:components/python3.bst
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- freedesktop-sdk.bst:components/brotli.bst
- sdk/glib.bst
- sdk/krb5.bst
- freedesktop-sdk.bst:components/libpsl.bst
- freedesktop-sdk.bst:components/libxml2.bst
- freedesktop-sdk.bst:components/sqlite.bst
- freedesktop-sdk.bst:bootstrap-import.bst
variables:
  meson-local: >-
    -Dtls_check=false
    -Dgtk_doc=false
    -Dntlm=disabled
    -Dvapi=disabled
    -Dsysprof=disabled
public:
  bst:
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libsoup-3.0.so'
