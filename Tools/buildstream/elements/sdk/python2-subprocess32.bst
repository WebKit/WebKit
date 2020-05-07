kind: pip

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- sdk/python2.bst

variables:
  pip: pip2

sources:
- kind: git_tag
  url: github_com:google/python-subprocess32
  track: 3.5.4

  ref: 3.5.4-0-gf5d96717bb266ed2c10fbd8d8dc65b30aab534e9
