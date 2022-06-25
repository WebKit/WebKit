kind: meson
sources:
- kind: tar
  url: gnome_downloads:glib-networking/2.70/glib-networking-2.70.0.tar.xz
  ref: 66b408e7afa86c582fe38963db56133869ab4b57d34e48ec56aba621940d6f35
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
