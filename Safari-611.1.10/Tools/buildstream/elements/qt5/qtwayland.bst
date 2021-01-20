kind: qmake

depends:
- qt5/qtdeclarative.bst
- freedesktop-sdk.bst:components/libdrm.bst

sources:
- kind: tar
  url: qt:5.15/5.15.0/submodules/qtwayland-everywhere-src-5.15.0.tar.xz
  ref: 084133e10bfbd32a28125639660c59975f23457bba6a79b30a25802cec76a9fb
