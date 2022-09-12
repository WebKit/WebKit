kind: autotools
sources:
- kind: tar
  url: github_com:flatpak/xdg-dbus-proxy/releases/download/0.1.2/xdg-dbus-proxy-0.1.2.tar.xz
  ref: 1749d6f9f46dcc9edc87725641cf56cf91dcad1b01707891ea0850c1000c520f
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:components/glib.bst
- freedesktop-sdk.bst:bootstrap-import.bst
