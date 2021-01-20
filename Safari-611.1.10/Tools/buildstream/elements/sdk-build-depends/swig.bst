kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: tar
  url: http://prdownloads.sourceforge.net/swig/swig-4.0.2.tar.gz
  ref: d53be9730d8d58a16bf0cbd1f8ac0c0c3e1090573168bfa151b01eb47fa906fc
