kind: manual
depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_tag
  url: github_com:WebKitGTK/webkitgtk-test-fonts.git
  track: 0.0.8
  ref: 0.0.8-0-gf6382ed93932cca128c8d9edec3088f85a7592d9
config:
  install-commands:
  - |
    make DESTDIR="%{install-root}/usr/share" install
