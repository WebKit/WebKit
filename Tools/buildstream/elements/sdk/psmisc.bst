kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_repo
  url: gitlab_com:psmisc/psmisc.git
  track: master
  ref: v23.6-9-gc22d1e4edbfec6e24346cd8d89b822cb07cd6f5c
