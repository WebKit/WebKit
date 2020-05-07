kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_tag
  url: github_com:stevegrubb/libcap-ng.git
  ref: v0.7.9
variables:
  conf-local: >-
    --with-python=no
    --with-python3=no
