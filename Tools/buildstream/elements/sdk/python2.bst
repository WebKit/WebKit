kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: tar
  url: https://www.python.org/ftp/python/2.7.16/Python-2.7.16.tar.xz
  ref: f222ef602647eecb6853681156d32de4450a2c39f4de93bd5b20235f2e660ed7
variables:
  conf-local: >-
    --with-system-ffi
    --enable-shared
    --with-lto
    --with-ensurepip=install

  make-install:
    make DESTDIR="%{install-root}" DESTSHARED="/usr/lib/python2.7/lib-dynload" install

public:
  bst:
    overlap-whitelist:
    - '**'

