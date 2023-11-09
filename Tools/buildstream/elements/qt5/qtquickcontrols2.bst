kind: qmake

depends:
- qt5/qtdeclarative.bst

sources:
- kind: git_repo
  url: kde:qtquickcontrols2
  track: kde/5.15
  ref: v5.15.0-57-ga2593ff9cf5d0af885c20c2e9f9faa6ca4f1c1a3
