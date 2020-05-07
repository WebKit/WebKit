kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
- freedesktop-sdk.bst:components/libxml2.bst
- test-infra/httpd.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
sources:
- kind: tar
  url: https://php.net/distributions/php-7.2.6.tar.xz
  ref: 1f004e049788a3effc89ef417f06a6cf704c95ae2a718b2175185f2983381ae7

variables:
  conf-local: >-
    --disable-xml
    --disable-dom
    --disable-libxml
    --disable-simplexml
    --disable-xmlreader
    --disable-xmlwriter
    --without-pear
    --with-apxs2
    --with-zlib
    --with-libzip
    --enable-sockets=shared
  make-install:
    install -D libs/libphp7.so %{install-root}/lib/apache2/modules/libphp7.so


