kind: pip

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- sdk/python2.bst

variables:
  pip: pip2

sources:
- kind: git_tag
  url: github_com:benjaminp/six.git
  track: 1.14.0
  ref: 1.14.0-0-g3a3db7510b33eb22c63ad94bc735a9032949249f
