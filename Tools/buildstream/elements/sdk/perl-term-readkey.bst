kind: makemaker
depends:
- freedesktop-sdk.bst:components/perl.bst

sources:
- kind: tar
  url: https://cpan.metacpan.org/authors/id/J/JS/JSTOWE/TermReadKey-2.38.tar.gz
  ref: 5a645878dc570ac33661581fbb090ff24ebce17d43ea53fd22e105a856a47290

public:
  bst:
    integration-commands:
    - |
      perl -w -e "use Term::ReadKey;"
