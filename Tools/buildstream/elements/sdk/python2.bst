kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/openssl.bst

sources:
- kind: tar
  url: https://www.python.org/ftp/python/2.7.18/Python-2.7.18.tar.xz
  ref: b62c0e7937551d0cc02b8fd5cb0f544f9405bafc9a54d3808ed4594812edef43
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

