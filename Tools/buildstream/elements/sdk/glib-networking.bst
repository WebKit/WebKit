kind: meson
sources:
- kind: tar
  url: gnome_downloads:glib-networking/2.70/glib-networking-2.70.alpha.tar.xz
  ref: 638c7e7028653a3f8f0b8dafe5a6676d70d93e5d87a3b8abe056161c255af055
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- sdk/glib.bst
- freedesktop-sdk.bst:components/gsettings-desktop-schemas.bst
- freedesktop-sdk.bst:components/ca-certificates.bst
- freedesktop-sdk.bst:components/gnutls.bst
- freedesktop-sdk.bst:components/libproxy.bst
- freedesktop-sdk.bst:bootstrap-import.bst
variables:
  meson-local: >-
    -Dinstalled_tests=true
public:
  bst:
    overlap-whitelist:
    - '**'
