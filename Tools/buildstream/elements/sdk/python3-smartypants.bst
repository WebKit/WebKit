kind: pip

depends:
- freedesktop-sdk.bst:components/python3.bst

build-depends:
- freedesktop-sdk.bst:components/python3-pip.bst

sources:
- kind: git_repo
  url: github_com:leohemsted/smartypants.py
  track: master
  ref: v2.0.1-4-gc46d26c559d706b6e0aa423190ab2d6edf1fdfcd

public:
  bst:
    integration-commands:
    - |
      python3 -c "import smartypants"
