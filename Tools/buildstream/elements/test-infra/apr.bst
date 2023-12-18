kind: autotools
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
- freedesktop-sdk.bst:components/python3.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/libxml2.bst

variables:
  autogen: |
    ./buildconf

config:
  install-commands:
  - |
    make -j1 install DESTDIR="%{install-root}"
    mkdir -p %{install-root}%{datadir}/aclocal/
    cp build/*.m4 %{install-root}%{datadir}/aclocal/

public:
  bst:
    overlap-whitelist:
    - '**'

sources:
- kind: git_repo
  url: github_com:apache/apr
  ref: ab59882d46f992fe13e696e0275a244123d51349
