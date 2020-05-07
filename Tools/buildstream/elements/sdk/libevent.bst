kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
sources:
- kind: git_tag
  url: github_com:libevent/libevent.git
  ref: release-2.1.8-stable-ge7ff4ef2b4fc950a765008c18e74281cdb5e7668

variables:
  conf-local: >-
    --disable-libevent-regress
