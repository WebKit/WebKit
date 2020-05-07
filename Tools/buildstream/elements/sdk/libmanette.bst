kind: meson
sources:
- kind: git_tag
  url: gitlab_gnome_org:aplazas/libmanette.git
  track: master
  ref: 0.2.4-0-ge92a35af43853eee0ce255754e622dbe10e71f2a

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- sdk/glib.bst
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/libgudev.bst
- freedesktop-sdk.bst:components/libevdev.bst

variables:
  meson-local: >-
    -Dvapi=false
    -Dintrospection=false
