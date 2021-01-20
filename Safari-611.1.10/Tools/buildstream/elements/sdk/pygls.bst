kind: pip

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/python3.bst

build-depends:
- freedesktop-sdk.bst:components/python3-pip.bst
- freedesktop-sdk.bst:components/python3-wheel.bst
- freedesktop-sdk.bst:components/python3-poetry.bst

sources:
- kind: tar
  url: pypi:55/d9/8386d331bcd5a6d0aea45e8c9185f2d8b5403a145692d9377c9fc094d7fb/pygls-0.9.1.tar.gz
  ref: c21c942415457315f316c0b6c19d4c743fd2974d457e02a85472154c8f1b6399

public:
  bst:
    integration-commands:
    - |
      python3 -c "from pygls.features import COMPLETION"

