package Bugzilla::Markdown::GFM::Parser;

use 5.10.1;
use strict;
use warnings;

use FFI::Platypus::Buffer qw( scalar_to_buffer );

sub new {
  my ($class, $args) = @_;
  my $options    = {%{$args // {}}};
  my $extensions = delete $options->{extensions} // [];
  my $parser     = $class->_new($options);
  $parser->{_options} = $options;

  foreach my $name (@$extensions) {
    my $extension = Bugzilla::Markdown::GFM::SyntaxExtension->find($name)
      or die "unknown extension: $name";
    $parser->attach_syntax_extension($extension);
  }

  return $parser;
}

sub render_html {
  my ($self, $markdown) = @_;
  $self->feed($markdown);
  my $node = $self->finish;
  return $node->render_html($self->{_options}, $self->get_syntax_extensions);
}

sub SETUP {
  my ($class, $FFI) = @_;

  $FFI->custom_type(
    markdown_parser_t => {
      native_type    => 'opaque',
      native_to_perl => sub {
        bless {_pointer => $_[0]}, $class;
      },
      perl_to_native => sub { $_[0]->{_pointer} },
    }
  );

  $FFI->attach(
    [cmark_parser_new => '_new'],
    ['markdown_options_t'] => 'markdown_parser_t',
    sub {
      my $c_func = shift;
      return $c_func->($_[1]);
    }
  );

  $FFI->attach([cmark_parser_free => 'DESTROY'], ['markdown_parser_t'] => 'void');

  $FFI->attach(
    [cmark_parser_feed => 'feed'],
    ['markdown_parser_t', 'opaque', 'int'] => 'void',
    sub {
      my $c_func = shift;
      $c_func->($_[0], scalar_to_buffer $_[1]);
    }
  );

  $FFI->attach([cmark_parser_finish => 'finish'],
    ['markdown_parser_t'] => 'markdown_node_t',);

  $FFI->attach(
    [cmark_parser_attach_syntax_extension => 'attach_syntax_extension'],
    ['markdown_parser_t', 'markdown_syntax_extension_t'] => 'void',
  );

  $FFI->attach(
    [cmark_parser_get_syntax_extensions => 'get_syntax_extensions'],
    ['markdown_parser_t'] => 'markdown_syntax_extension_list_t',
  );
}

1;

__END__

=head1 NAME

Bugzilla::Markdown::GFM::Parser - Transforms markdown into HTML via libcmark_gfm.

=head1 SYNOPSIS

    use Bugzilla::Markdown::GFM;
    use Bugzilla::Markdown::GFM::Parser;

    my $parser = Bugzilla::Markdown::GFM::Parser->new({
        extensions => [qw( autolink tagfilter table strikethrough )]
    });

    say $parser->render_html(<<'MARKDOWN');
    # My header

    This is **markdown**!

    - list item 1
    - list item 2
    MARKDOWN
