kind: meson
sources:
- kind: git_repo
  url: gitlab_gnome_org:GNOME/libmanette.git
  track: master
  ref: 0.2.6-9-g697f5b09716d153969cbf7f5d6f86a2ca6d6aded

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- freedesktop-sdk.bst:components/glib.bst
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/libevdev.bst

variables:
  meson-local: >-
    -Dvapi=false
    -Dintrospection=false
    -Dgudev=disabled
