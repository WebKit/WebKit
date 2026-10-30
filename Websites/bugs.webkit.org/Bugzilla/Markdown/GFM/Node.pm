package Bugzilla::Markdown::GFM::Node;

use 5.10.1;
use strict;
use warnings;

sub SETUP {
  my ($class, $FFI) = @_;

  $FFI->custom_type(
    markdown_node_t => {
      native_type    => 'opaque',
      native_to_perl => sub {
        bless \$_[0], $class if $_[0];
      },
      perl_to_native => sub { ${$_[0]} },
    }
  );

  $FFI->attach([cmark_node_free => 'DESTROY'], ['markdown_node_t'] => 'void');

  $FFI->attach(
    [cmark_render_html => 'render_html'],
    ['markdown_node_t', 'markdown_options_t',
      'markdown_syntax_extension_list_t'] => 'string',
  );
}

1;

__END__
