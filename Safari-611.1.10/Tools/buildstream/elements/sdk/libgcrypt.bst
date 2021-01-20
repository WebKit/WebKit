kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
- freedesktop-sdk.bst:components/texinfo.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/libgpg-error.bst

sources:
- kind: git_tag
  url: https://dev.gnupg.org/source/libgcrypt.git
  ref: libgcrypt-1.8.6
- kind: patch
  path: patches/libgcrypt-0001-Use-pkg-config-to-detect-gpg-error.patch
- kind: patch
  path: patches/libgcrypt-use-only-dev-urandom-for-testing.patch
variables:
  conf-local: >-
    --disable-doc
public:
  bst:
    overlap-whitelist:
    - '**'
