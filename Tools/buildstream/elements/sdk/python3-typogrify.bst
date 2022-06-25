kind: pip

depends:
- freedesktop-sdk.bst:components/python3.bst

build-depends:
- freedesktop-sdk.bst:components/python3-pip.bst

sources:
- kind: tar
  url: pypi:8a/bf/64959d6187d42472acb846bcf462347c9124952c05bd57e5769d5f28f9a6/typogrify-2.0.7.tar.gz
  ref: 8be4668cda434163ce229d87ca273a11922cb1614cb359970b7dc96eed13cb38

public:
  bst:
    integration-commands:
    - |
      python3 -c "import typogrify"
