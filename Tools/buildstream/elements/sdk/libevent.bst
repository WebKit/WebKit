kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
- freedesktop-sdk.bst:components/openssl.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
sources:
- kind: git_repo
  url: github_com:libevent/libevent.git
  track: release-2.*
  ref: release-2.2.1-alpha-0-gfe9dc8f614db0520027e8e2adb95769193d4f0a3
variables:
  conf-local: >-
    --disable-libevent-regress
