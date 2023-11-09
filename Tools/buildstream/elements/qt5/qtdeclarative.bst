kind: qmake

depends:
- qt5/qtbase.bst

sources:
- kind: git_repo
  url: kde:qtdeclarative
  track: kde/5.15
  ref: v5.15.0-201-g6683c414c5cc6ab46197c41bb1361c518ca84d3e
