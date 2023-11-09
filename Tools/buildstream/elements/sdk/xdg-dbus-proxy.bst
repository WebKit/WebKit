kind: autotools
sources:
- kind: tar
  url: github_com:flatpak/xdg-dbus-proxy/releases/download/0.1.5/xdg-dbus-proxy-0.1.5.tar.xz
  ref: 061dcfaf8a0650e5fd9d5432dfe88bda749ea0d079dc136304bfecfbce0661fb
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:components/glib.bst
- freedesktop-sdk.bst:bootstrap-import.bst
