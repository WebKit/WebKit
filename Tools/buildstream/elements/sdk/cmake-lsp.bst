kind: pip

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/python3.bst
- freedesktop-sdk.bst:components/python3-pyparsing.bst
- sdk/pygls.bst

build-depends:
- freedesktop-sdk.bst:components/python3-pip.bst
- freedesktop-sdk.bst:components/python3-wheel.bst
- freedesktop-sdk.bst:components/python3-poetry.bst

sources:
- kind: tar
  url: pypi:33/e8/bef214936127fc5becd4ae4b94764a76800f1007cdd178bd96858b4c85af/cmake-language-server-0.1.2.tar.gz
  ref: a0b8cc43555a06b7964359c80dd35c8cfcbcdea1b2213b2e64378d5ea721c100

public:
  bst:
    integration-commands:
    - |
      cmake-language-server --version

