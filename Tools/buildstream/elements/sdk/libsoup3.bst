kind: meson
sources:
- kind: tar
  url: gnome_downloads:libsoup/2.99/libsoup-2.99.8.tar.xz
  ref: f4b95bd9678b4b0f275f2327587814b612d9e6333e3626e797f93392e569dc1a
build-depends:
- freedesktop-sdk.bst:components/gobject-introspection.bst
- freedesktop-sdk.bst:components/python3.bst
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- freedesktop-sdk.bst:components/brotli.bst
- freedesktop-sdk.bst:components/gnutls.bst
- sdk/glib.bst
- sdk/krb5.bst
- sdk/nghttp2.bst
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
    -Dhttp2_tests=disabled
public:
  bst:
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libsoup-3.0.so'
