package ExtUtils::MM_Cygwin;

use strict;

use ExtUtils::MakeMaker::Config;
use File::Spec;

require ExtUtils::MM_Unix;
require ExtUtils::MM_Win32;
our @ISA = qw( ExtUtils::MM_Unix );

our $VERSION = '7.32';
$VERSION = eval $VERSION;


=head1 NAME

ExtUtils::MM_Cygwin - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_Cygwin; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided there.

=over 4

=item os_flavor

We're Unix and Cygwin.

=cut

sub os_flavor {
    return('Unix', 'Cygwin');
}

=item cflags

if configured for dynamic loading, triggers #define EXT in EXTERN.h

=cut

sub cflags {
    my($self,$libperl)=@_;
    return $self->{CFLAGS} if $self->{CFLAGS};
    return '' unless $self->needs_linking();

    my $base = $self->SUPER::cflags($libperl);
    foreach (split /\n/, $base) {
        /^(\S*)\s*=\s*(\S*)$/ and $self->{$1} = $2;
    };
    $self->{CCFLAGS} .= " -DUSEIMPORTLIB" if ($Config{useshrplib} eq 'true');

    return $self->{CFLAGS} = qq{
CCFLAGS = $self->{CCFLAGS}
OPTIMIZE = $self->{OPTIMIZE}
PERLTYPE = $self->{PERLTYPE}
};

}


=item replace_manpage_separator

replaces strings '::' with '.' in MAN*POD man page names

=cut

sub replace_manpage_separator {
    my($self, $man) = @_;
    $man =~ s{/+}{.}g;
    return $man;
}

=item init_linker

points to libperl.a

=cut

sub init_linker {
    my $self = shift;

    if ($Config{useshrplib} eq 'true') {
        my $libperl = '$(PERL_INC)' .'/'. "$Config{libperl}";
        if( $] >= 5.006002 ) {
            $libperl =~ s/(dll\.)?a$/dll.a/;
        }
        $self->{PERL_ARCHIVE} = $libperl;
    } else {
        $self->{PERL_ARCHIVE} =
          '$(PERL_INC)' .'/'. ("$Config{libperl}" or "libperl.a");
    }

    $self->{PERL_ARCHIVEDEP} ||= '';
    $self->{PERL_ARCHIVE_AFTER} ||= '';
    $self->{EXPORT_LIST}  ||= '';
}

=item maybe_command

Determine whether a file is native to Cygwin by checking whether it
resides inside the Cygwin installation (using Windows paths). If so,
use C<ExtUtils::MM_Unix> to determine if it may be a command.
Otherwise use the tests from C<ExtUtils::MM_Win32>.

=cut

sub maybe_command {
    my ($self, $file) = @_;

    my $cygpath = Cygwin::posix_to_win_path('/', 1);
    my $filepath = Cygwin::posix_to_win_path($file, 1);

    return (substr($filepath,0,length($cygpath)) eq $cygpath)
    ? $self->SUPER::maybe_command($file) # Unix
    : ExtUtils::MM_Win32->maybe_command($file); # Win32
}

=item dynamic_lib

Use the default to produce the *.dll's.
But for new archdir dll's use the same rebase address if the old exists.

=cut

sub dynamic_lib {
    my($self, %attribs) = @_;
    my $s = ExtUtils::MM_Unix::dynamic_lib($self, %attribs);
    return '' unless $s;
    return $s unless %{$self->{XS}};

    # do an ephemeral rebase so the new DLL fits to the current rebase map
    $s .= "\t/bin/find \$\(INST_ARCHLIB\)/auto -xdev -name \\*.$self->{DLEXT} | /bin/rebase -sOT -" if (( $Config{myarchname} eq 'i686-cygwin' ) and not ( exists $ENV{CYGPORT_PACKAGE_VERSION} ));
    $s;
}

=item install

Rebase dll's with the global rebase database after installation.

=cut

sub install {
    my($self, %attribs) = @_;
    my $s = ExtUtils::MM_Unix::install($self, %attribs);
    return '' unless $s;
    return $s unless %{$self->{XS}};

    my $INSTALLDIRS = $self->{INSTALLDIRS};
    my $INSTALLLIB = $self->{"INSTALL". ($INSTALLDIRS eq 'perl' ? 'ARCHLIB' : uc($INSTALLDIRS)."ARCH")};
    my $dop = "\$\(DESTDIR\)$INSTALLLIB/auto/";
    my $dll = "$dop/$self->{FULLEXT}/$self->{BASEEXT}.$self->{DLEXT}";
    $s =~ s|^(pure_install :: pure_\$\(INSTALLDIRS\)_install\n\t)\$\(NOECHO\) \$\(NOOP\)\n|$1\$(CHMOD) \$(PERM_RWX) $dll\n\t/bin/find $dop -xdev -name \\*.$self->{DLEXT} \| /bin/rebase -sOT -\n|m if (( $Config{myarchname} eq 'i686-cygwin') and not ( exists $ENV{CYGPORT_PACKAGE_VERSION} ));
    $s;
}

=item all_target

Build man pages, too

=cut

sub all_target {
    ExtUtils::MM_Unix::all_target(shift);
}

=back

=cut

1;
