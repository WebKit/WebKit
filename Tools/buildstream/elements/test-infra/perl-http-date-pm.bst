kind: makemaker
depends:
- freedesktop-sdk.bst:components/perl.bst        

sources:
- kind: tar
  url: https://cpan.metacpan.org/authors/id/G/GA/GAAS/HTTP-Date-6.02.tar.gz
  ref: e8b9941da0f9f0c9c01068401a5e81341f0e3707d1c754f8e11f42a7e629e333
public:
  bst:
    overlap-whitelist:
    - '**'
