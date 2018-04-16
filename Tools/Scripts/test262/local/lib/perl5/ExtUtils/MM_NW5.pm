package ExtUtils::MM_NW5;

=head1 NAME

ExtUtils::MM_NW5 - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_NW5; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=over

=cut

use strict;
use ExtUtils::MakeMaker::Config;
use File::Basename;

our $VERSION = '7.32';
$VERSION = eval $VERSION;

require ExtUtils::MM_Win32;
our @ISA = qw(ExtUtils::MM_Win32);

use ExtUtils::MakeMaker qw(&neatvalue &_sprintf562);

$ENV{EMXSHELL} = 'sh'; # to run `commands`

my $BORLAND  = $Config{'cc'} =~ /\bbcc/i;
my $GCC      = $Config{'cc'} =~ /\bgcc/i;


=item os_flavor

We're Netware in addition to being Windows.

=cut

sub os_flavor {
    my $self = shift;
    return ($self->SUPER::os_flavor, 'Netware');
}

=item init_platform

Add Netware macros.

LIBPTH, BASE_IMPORT, NLM_VERSION, MPKTOOL, TOOLPATH, BOOT_SYMBOL,
NLM_SHORT_NAME, INCLUDE, PATH, MM_NW5_REVISION


=item platform_constants

Add Netware macros initialized above to the Makefile.

=cut

sub init_platform {
    my($self) = shift;

    # To get Win32's setup.
    $self->SUPER::init_platform;

    # incpath is copied to makefile var INCLUDE in constants sub, here just
    # make it empty
    my $libpth = $Config{'libpth'};
    $libpth =~ s( )(;);
    $self->{'LIBPTH'} = $libpth;

    $self->{'BASE_IMPORT'} = $Config{'base_import'};

    # Additional import file specified from Makefile.pl
    if($self->{'base_import'}) {
        $self->{'BASE_IMPORT'} .= ', ' . $self->{'base_import'};
    }

    $self->{'NLM_VERSION'} = $Config{'nlm_version'};
    $self->{'MPKTOOL'}     = $Config{'mpktool'};
    $self->{'TOOLPATH'}    = $Config{'toolpath'};

    (my $boot = $self->{'NAME'}) =~ s/:/_/g;
    $self->{'BOOT_SYMBOL'}=$boot;

    # If the final binary name is greater than 8 chars,
    # truncate it here.
    if(length($self->{'BASEEXT'}) > 8) {
        $self->{'NLM_SHORT_NAME'} = substr($self->{'BASEEXT'},0,8);
    }

    # Get the include path and replace the spaces with ;
    # Copy this to makefile as INCLUDE = d:\...;d:\;
    ($self->{INCLUDE} = $Config{'incpath'}) =~ s/([ ]*)-I/;/g;

    # Set the path to CodeWarrior binaries which might not have been set in
    # any other place
    $self->{PATH} = '$(PATH);$(TOOLPATH)';

    $self->{MM_NW5_VERSION} = $VERSION;
}

sub platform_constants {
    my($self) = shift;
    my $make_frag = '';

    # Setup Win32's constants.
    $make_frag .= $self->SUPER::platform_constants;

    foreach my $macro (qw(LIBPTH BASE_IMPORT NLM_VERSION MPKTOOL
                          TOOLPATH BOOT_SYMBOL NLM_SHORT_NAME INCLUDE PATH
                          MM_NW5_VERSION
                      ))
    {
        next unless defined $self->{$macro};
        $make_frag .= "$macro = $self->{$macro}\n";
    }

    return $make_frag;
}

=item static_lib_pure_cmd

Defines how to run the archive utility

=cut

sub static_lib_pure_cmd {
    my ($self, $src) = @_;
    $src =~ s/(\$\(\w+)(\))/$1:^"+"$2/g if $BORLAND;
    sprintf qq{\t\$(AR) %s\n}, ($BORLAND ? '$@ ' . $src
                          : ($GCC ? '-ru $@ ' . $src
                                  : '-type library -o $@ ' . $src));
}

=item xs_static_lib_is_xs

=cut

sub xs_static_lib_is_xs {
    return 1;
}

=item dynamic_lib

Override of utility methods for OS-specific work.

=cut

sub xs_make_dynamic_lib {
    my ($self, $attribs, $from, $to, $todir, $ldfrom, $exportlist) = @_;
    my @m;
    # Taking care of long names like FileHandle, ByteLoader, SDBM_File etc
    if ($to =~ /^\$/) {
        if ($self->{NLM_SHORT_NAME}) {
            # deal with shortnames
            my $newto = q{$(INST_AUTODIR)\\$(NLM_SHORT_NAME).$(DLEXT)};
            push @m, "$to: $newto\n\n";
            $to = $newto;
        }
    } else {
        my ($v, $d, $f) = File::Spec->splitpath($to);
        # relies on $f having a literal "." in it, unlike for $(OBJ_EXT)
        if ($f =~ /[^\.]{9}\./) {
            # 9+ chars before '.', need to shorten
            $f = substr $f, 0, 8;
        }
        my $newto = File::Spec->catpath($v, $d, $f);
        push @m, "$to: $newto\n\n";
        $to = $newto;
    }
    # bits below should be in dlsyms, not here
    #                                   1    2      3       4
    push @m, _sprintf562 <<'MAKE_FRAG', $to, $from, $todir, $exportlist;
# Create xdc data for an MT safe NLM in case of mpk build
%1$s: %2$s $(MYEXTLIB) $(BOOTSTRAP) %3$s$(DFSEP).exists
    $(NOECHO) $(ECHO) Export boot_$(BOOT_SYMBOL) > %4$s
    $(NOECHO) $(ECHO) $(BASE_IMPORT) >> %4$s
    $(NOECHO) $(ECHO) Import @$(PERL_INC)\perl.imp >> %4$s
MAKE_FRAG
    if ( $self->{CCFLAGS} =~ m/ -DMPK_ON /) {
        (my $xdc = $exportlist) =~ s#def\z#xdc#;
        $xdc = '$(BASEEXT).xdc';
        push @m, sprintf <<'MAKE_FRAG', $xdc, $exportlist;
    $(MPKTOOL) $(XDCFLAGS) %s
    $(NOECHO) $(ECHO) xdcdata $(BASEEXT).xdc >> %s
MAKE_FRAG
    }
    # Reconstruct the X.Y.Z version.
    my $version = join '.', map { sprintf "%d", $_ }
                              $] =~ /(\d)\.(\d{3})(\d{2})/;
    push @m, sprintf <<'EOF', $from, $version, $to, $exportlist;
    $(LD) $(LDFLAGS) %s -desc "Perl %s Extension ($(BASEEXT))  XS_VERSION: $(XS_VERSION)" -nlmversion $(NLM_VERSION) -o %s $(MYEXTLIB) $(PERL_INC)\Main.lib -commandfile %s
    $(CHMOD) 755 $@
EOF
    join '', @m;
}

1;
__END__

=back

=cut
