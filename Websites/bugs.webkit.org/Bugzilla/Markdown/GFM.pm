package Bugzilla::Markdown::GFM;

use 5.10.1;
use strict;
use warnings;

use Alien::libcmark_gfm;
use FFI::Platypus;
use FFI::Platypus::Buffer qw( scalar_to_buffer );
use Exporter qw(import);

use Bugzilla::Markdown::GFM::SyntaxExtension;
use Bugzilla::Markdown::GFM::SyntaxExtensionList;
use Bugzilla::Markdown::GFM::Parser;
use Bugzilla::Markdown::GFM::Node;

our @EXPORT_OK = qw(cmark_markdown_to_html);

my %OPTIONS = (
  default                       => 0,
  sourcepos                     => (1 << 1),
  hardbreaks                    => (1 << 2),
  safe                          => (1 << 3),
  nobreaks                      => (1 << 4),
  normalize                     => (1 << 8),
  validate_utf8                 => (1 << 9),
  smart                         => (1 << 10),
  github_pre_lang               => (1 << 11),
  liberal_html_tag              => (1 << 12),
  footnotes                     => (1 << 13),
  strikethrough_double_tilde    => (1 << 14),
  table_prefer_style_attributes => (1 << 15),
);

my $FFI = FFI::Platypus->new(
  lib => [grep { not -l $_ } Alien::libcmark_gfm->dynamic_libs],);

$FFI->custom_type(
  markdown_options_t => {
    native_type    => 'int',
    native_to_perl => sub {
      my ($options) = @_;
      my $result = {};
      foreach my $key (keys %OPTIONS) {
        $result->{$key} = ($options & $OPTIONS{$key}) != 0;
      }
      return $result;
    },
    perl_to_native => sub {
      my ($options) = @_;
      my $result = 0;
      foreach my $key (keys %OPTIONS) {
        if ($options->{$key}) {
          $result |= $OPTIONS{$key};
        }
      }
      return $result;
    }
  }
);

$FFI->attach(
  cmark_markdown_to_html => ['opaque', 'int', 'markdown_options_t'] => 'string',
  sub {
    my $c_func = shift;
    my ($markdown, $markdown_length) = scalar_to_buffer $_[0];
    return $c_func->($markdown, $markdown_length, $_[1]);
  }
);

# This has to happen after something from the main lib is loaded
$FFI->attach('cmark_gfm_core_extensions_ensure_registered' => [] => 'void');

cmark_gfm_core_extensions_ensure_registered();

Bugzilla::Markdown::GFM::SyntaxExtension->SETUP($FFI);
Bugzilla::Markdown::GFM::SyntaxExtensionList->SETUP($FFI);
Bugzilla::Markdown::GFM::Node->SETUP($FFI);
Bugzilla::Markdown::GFM::Parser->SETUP($FFI);

1;

__END__

=head1 NAME

Bugzilla::Markdown::GFM - Sets up the FFI to libcmark_gfm.

=head1 DESCRIPTION

This modules mainly just does setup work. See L<Bugzilla::Markdown::GFM::Parser>
to actually render markdown to HTML.
