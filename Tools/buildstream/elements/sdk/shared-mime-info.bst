kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
- freedesktop-sdk.bst:components/itstool.bst
- sdk/intltool.bst
- freedesktop-sdk.bst:components/perl-xml-parser.bst

depends:
- sdk/glib.bst
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/libxml2.bst
sources:
- kind: tar
  url: https://gitlab.freedesktop.org/xdg/shared-mime-info/uploads/6a226038bf42dae45a049a6b8e729abc/shared-mime-info-1.10.tar.xz
  ref: c625a83b4838befc8cafcd54e3619946515d9e44d63d61c4adf7f5513ddfbebf
variables:
  notparallel: True
public:
  bst:
    overlap-whitelist:
    - '**'
