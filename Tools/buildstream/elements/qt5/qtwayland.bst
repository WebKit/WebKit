kind: qmake

depends:
- qt5/qtdeclarative.bst
- freedesktop-sdk.bst:components/libdrm.bst

sources:
- kind: git_repo
  url: kde:qtwayland
  track: kde/5.15
  ref: v5.15.0-58-gd4c41797b61a5a8da47c5821711aca72e756dcbf
