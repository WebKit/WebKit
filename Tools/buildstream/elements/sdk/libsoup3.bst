kind: meson
sources:
- kind: tar
  url: gnome_downloads:libsoup/3.0/libsoup-3.0.0.tar.xz
  ref: cde05f3e20114ce985cbd03d575ff74e5e41e70a218e5eb3068c4bf4710dbf2b
build-depends:
- freedesktop-sdk.bst:components/gobject-introspection.bst
- freedesktop-sdk.bst:components/python3.bst
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- freedesktop-sdk.bst:components/brotli.bst
- freedesktop-sdk.bst:components/gnutls.bst
- sdk/glib.bst
- sdk/krb5.bst
- freedesktop-sdk.bst:components/libnghttp2.bst
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
