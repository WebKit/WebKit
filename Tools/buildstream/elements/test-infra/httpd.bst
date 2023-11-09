kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
- freedesktop-sdk.bst:components/perl.bst
- test-infra/apr.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/expat.bst
- freedesktop-sdk.bst:components/openssl.bst

variables:
  autogen: |
    cp /usr/share/aclocal/apr_* build/
    ./buildconf --with-apr=/usr/bin/apr-2-config
  conf-local: >-
    --enable-mpms-shared=all
    --enable-modules=all
    --with-apr-util=/usr/bin/apu-2-config
    --with-pcre=/usr
    --enable-authnz-fcgi
    --enable-cgi
    --enable-cgid
    --libexecdir=/usr/lib/apache2/modules

sources:
- kind: git_repo
  url: github_com:apache/httpd
  track: 2.4.x
  ref: 3feff7c0c48c65639e7102e29f2bb552dabb10a2
