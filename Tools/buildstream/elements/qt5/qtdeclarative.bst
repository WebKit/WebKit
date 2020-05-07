kind: qmake
description: json2bst conversion of qt5/qtdeclarative

depends:
- qt5/qtbase.bst

sources:
- kind: tar
  url: http://download.qt.io/archive/qt/5.14/5.14.1/submodules/qtdeclarative-everywhere-src-5.14.1.tar.xz
  ref: 762fe495d2f97fd70f06dc7d3929506ea3b5e3151ad813e0629209b7bc504c8a
- kind: patch
  path: patches/qtdeclarative-use-python3.patch
