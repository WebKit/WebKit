kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
- sdk-build-depends/utf8proc.bst
- sdk-build-depends/swig.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/sqlite.bst
- freedesktop-sdk.bst:components/expat.bst
- freedesktop-sdk.bst:components/perl.bst
- test-infra/apr.bst
- test-infra/apr-util.bst
- sdk/libserf.bst
- sdk/perl-term-readkey.bst
- sdk/libsecret.bst

sources:
- kind: tar
  url: http://apache.uvigo.es/subversion/subversion-1.14.0.tar.bz2
  ref: 6ba8e218f9f97a83a799e58a3c6da1221d034b18d9d8cbbcb6ec52ab11722102

variables:
  conf-local: >-
    --with-apr=/usr/bin/apr-1-config
    --with-apr-util=/usr/bin/apu-1-config
    --with-lz4=internal
    --with-swig=/usr/bin/swig
    --with-gnome-keyring

config:
  build-commands:
  - |
    %{make}

  - |
    make swig-pl-lib

  install-commands:
  - |
    %{make-install}

  - |
    make install-swig-pl-lib PREFIX="%{prefix}" DESTDIR="%{install-root}"
    cd subversion/bindings/swig/perl/native && perl Makefile.PL PREFIX="%{prefix}" DESTDIR="%{install-root}" && make install

  - |
    find "%{install-root}" -type f -name perllocal.pod -delete

public:
  bst:
    integration-commands:
    - |
      perl -w -e "use SVN::Base qw(Core);"
