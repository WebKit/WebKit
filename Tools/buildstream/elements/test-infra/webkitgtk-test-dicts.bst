kind: manual
depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_repo
  url: github_com:mrobinson/webkitgtk-test-dicts.git
  track: 0.0.1
  ref: 0.0.1-0-ga63aa52f09ddbf2532304a5751d22e5712299b0d
config:
  build-commands:
  - make

  install-commands:
  - |
    make DESTDIR="%{install-root}/usr/share" install
