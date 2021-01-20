kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: tar
  url: http://www.oberhumer.com/opensource/lzo/download/lzo-2.10.tar.gz
  ref: c0f892943208266f9b6543b3ae308fab6284c5c90e627931446fb49b4221a072
