kind: qmake

depends:
- qt5/qtbase.bst

sources:
- kind: git_repo
  url: kde:qtx11extras
  track: kde/5.15
  ref: v5.15.0-7-g3898f5484fd4864b047729bfeda9a1222f32364f
