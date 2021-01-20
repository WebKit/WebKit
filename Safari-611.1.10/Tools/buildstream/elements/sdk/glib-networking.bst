kind: meson
sources:
- kind: tar
  url: gnome_downloads:glib-networking/2.62/glib-networking-2.62.3.tar.xz
  ref: 8ca1f86f23a76b5c7640624f7d5490705c78e81375e1741c9a1c41ce7f8f7ff7
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
