kind: qmake

depends:
- qt5/qtbase.bst

sources:
- kind: tar
  url: qt:5.15/5.15.0/submodules/qtdeclarative-everywhere-src-5.15.0.tar.xz
  ref: 9c3c93fb7d340b2f7d738d12408c047318c78973cb45bfc5ff6b3a57e1fef699
- kind: patch
  path: patches/qtdeclarative-use-python3.patch
