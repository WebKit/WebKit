kind: pip

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- sdk/python2.bst
- freedesktop-sdk.bst:components/cairo.bst
build-depends:
- freedesktop-sdk.bst:components/pkg-config.bst

variables:
  pip: pip2

sources:
- kind: git_tag
  url: github_com:pygobject/pycairo
  track: v1.15.4
  ref: v1.15.4-0-g88478f2bb18537ef3e58f9ecb94c509f24e3300b
