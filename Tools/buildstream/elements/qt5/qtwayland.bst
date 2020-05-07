kind: qmake

depends:
- qt5/qtdeclarative.bst
- freedesktop-sdk.bst:components/libdrm.bst

sources:
- kind: tar
  url: http://ftp1.nluug.nl/languages/qt/archive/qt/5.14/5.14.1/submodules/qtwayland-everywhere-src-5.14.1.tar.xz
  ref: 2a03b9f554e88c5824ef237c814b3dd45844c022e97be0e091f4a502ca4c9520
- kind: patch
  path: patches/qtwayland-use-gnome-platform-theme-on-gnome-based-desktops.patch
