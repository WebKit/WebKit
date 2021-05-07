kind: meson
sources:
- kind: tar
  url: gnome_downloads:libsoup/2.99/libsoup-2.99.5.tar.xz
  ref: ea67665a72ac0d1cb5564fb0b8dde235b1689bf76305ed2b5c2a1f704895d0a3
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
    -Dtests=false
    -Dautobahn=disabled
public:
  bst:
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libsoup-3.0.so'
