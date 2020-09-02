kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
- freedesktop-sdk.bst:components/perl.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/expat.bst
- test-infra/apr.bst
- test-infra/apr-util.bst
sources:
- kind: git
  url: github_com:apache/httpd.git
  ref: 6431bbb0da47dd984110dc32728a84e5cc990313
- kind: patch
  path: patches/httpd-0001-configure-use-pkg-config-for-PCRE-detection.patch
variables:
  autogen: |
    aclocal;
    autoheader;
    autoreconf --install;
    automake --add-missing || true;
  conf-local: >-
    --enable-mpms-shared=all
    --enable-modules=all
    --with-apr=/usr/bin/apr-1-config
    --with-apr-util=/usr/bin/apu-1-config
    --with-pcre=/usr
    --enable-authnz-fcgi
    --enable-cgi
    --enable-cgid
    --libexecdir=/usr/lib/apache2/modules
