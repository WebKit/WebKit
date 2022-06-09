kind: meson

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
- freedesktop-sdk.bst:components/docbook-xsl.bst
- freedesktop-sdk.bst:components/itstool.bst
- freedesktop-sdk.bst:components/xmlto.bst

depends:
- sdk/glib.bst
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/libxml2.bst

sources:
- kind: git_tag
  url: gitlab_freedesktop_org:xdg/shared-mime-info
  track: '2.1'
  ref: 2.1-0-g18e558fa1c8b90b86757ade09a4ba4d6a6cf8f70
public:
  bst:
    overlap-whitelist:
    - '**'
    integration-commands:
    - |
      PKGSYSTEM_ENABLE_FSYNC=0 update-mime-database "%{datadir}/mime"
