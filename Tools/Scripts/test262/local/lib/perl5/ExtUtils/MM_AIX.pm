package ExtUtils::MM_AIX;

use strict;
our $VERSION = '7.32';
$VERSION = eval $VERSION;

use ExtUtils::MakeMaker::Config;
require ExtUtils::MM_Unix;
our @ISA = qw(ExtUtils::MM_Unix);

=head1 NAME

ExtUtils::MM_AIX - AIX specific subclass of ExtUtils::MM_Unix

=head1 SYNOPSIS

  Don't use this module directly.
  Use ExtUtils::MM and let it choose.

=head1 DESCRIPTION

This is a subclass of ExtUtils::MM_Unix which contains functionality for
AIX.

Unless otherwise stated it works just like ExtUtils::MM_Unix

=head2 Overridden methods

=head3 dlsyms

Define DL_FUNCS and DL_VARS and write the *.exp files.

=cut

sub dlsyms {
    my($self,%attribs) = @_;
    return '' unless $self->needs_linking;
    join "\n", $self->xs_dlsyms_iterator(\%attribs);
}

=head3 xs_dlsyms_ext

On AIX, is C<.exp>.

=cut

sub xs_dlsyms_ext {
    '.exp';
}

sub xs_dlsyms_arg {
    my($self, $file) = @_;
    return qq{-bE:${file}};
}

sub init_others {
    my $self = shift;
    $self->SUPER::init_others;
    # perl "hints" add -bE:$(BASEEXT).exp to LDDLFLAGS. strip that out
    # so right value can be added by xs_make_dynamic_lib to work for XSMULTI
    $self->{LDDLFLAGS} ||= $Config{lddlflags};
    $self->{LDDLFLAGS} =~ s#(\s*)\S*\Q$(BASEEXT)\E\S*(\s*)#$1$2#;
    return;
}

=head1 AUTHOR

Michael G Schwern <schwern@pobox.com> with code from ExtUtils::MM_Unix

=head1 SEE ALSO

L<ExtUtils::MakeMaker>

=cut


1;
