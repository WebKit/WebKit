package ExtUtils::MM_OS2;

use strict;

use ExtUtils::MakeMaker qw(neatvalue);
use File::Spec;

our $VERSION = '7.32';
$VERSION = eval $VERSION;

require ExtUtils::MM_Any;
require ExtUtils::MM_Unix;
our @ISA = qw(ExtUtils::MM_Any ExtUtils::MM_Unix);

=pod

=head1 NAME

ExtUtils::MM_OS2 - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_OS2; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=head1 METHODS

=over 4

=item init_dist

Define TO_UNIX to convert OS2 linefeeds to Unix style.

=cut

sub init_dist {
    my($self) = @_;

    $self->{TO_UNIX} ||= <<'MAKE_TEXT';
$(NOECHO) $(TEST_F) tmp.zip && $(RM_F) tmp.zip; $(ZIP) -ll -mr tmp.zip $(DISTVNAME) && unzip -o tmp.zip && $(RM_F) tmp.zip
MAKE_TEXT

    $self->SUPER::init_dist;
}

sub dlsyms {
    my($self,%attribs) = @_;
    if ($self->{IMPORTS} && %{$self->{IMPORTS}}) {
    # Make import files (needed for static build)
    -d 'tmp_imp' or mkdir 'tmp_imp', 0777 or die "Can't mkdir tmp_imp";
    open my $imp, '>', 'tmpimp.imp' or die "Can't open tmpimp.imp";
    foreach my $name (sort keys %{$self->{IMPORTS}}) {
        my $exp = $self->{IMPORTS}->{$name};
        my ($lib, $id) = ($exp =~ /(.*)\.(.*)/) or die "Malformed IMPORT `$exp'";
        print $imp "$name $lib $id ?\n";
    }
    close $imp or die "Can't close tmpimp.imp";
    # print "emximp -o tmpimp$Config::Config{lib_ext} tmpimp.imp\n";
    system "emximp -o tmpimp$Config::Config{lib_ext} tmpimp.imp"
        and die "Cannot make import library: $!, \$?=$?";
    # May be running under miniperl, so have no glob...
    eval { unlink <tmp_imp/*>; 1 } or system "rm tmp_imp/*";
    system "cd tmp_imp; $Config::Config{ar} x ../tmpimp$Config::Config{lib_ext}"
        and die "Cannot extract import objects: $!, \$?=$?";
    }
    return '' if $self->{SKIPHASH}{'dynamic'};
    $self->xs_dlsyms_iterator(\%attribs);
}

sub xs_dlsyms_ext {
    '.def';
}

sub xs_dlsyms_extra {
    join '', map { qq{, "$_" => "\$($_)"} } qw(VERSION DISTNAME INSTALLDIRS);
}

sub static_lib_pure_cmd {
    my($self) = @_;
    my $old = $self->SUPER::static_lib_pure_cmd;
    return $old unless $self->{IMPORTS} && %{$self->{IMPORTS}};
    $old . <<'EOC';
    $(AR) $(AR_STATIC_ARGS) "$@" tmp_imp/*
    $(RANLIB) "$@"
EOC
}

sub replace_manpage_separator {
    my($self,$man) = @_;
    $man =~ s,/+,.,g;
    $man;
}

sub maybe_command {
    my($self,$file) = @_;
    $file =~ s,[/\\]+,/,g;
    return $file if -x $file && ! -d _;
    return "$file.exe" if -x "$file.exe" && ! -d _;
    return "$file.cmd" if -x "$file.cmd" && ! -d _;
    return;
}

=item init_linker

=cut

sub init_linker {
    my $self = shift;

    $self->{PERL_ARCHIVE} = "\$(PERL_INC)/libperl\$(LIB_EXT)";

    $self->{PERL_ARCHIVEDEP} ||= '';
    $self->{PERL_ARCHIVE_AFTER} = $OS2::is_aout
      ? ''
      : '$(PERL_INC)/libperl_override$(LIB_EXT)';
    $self->{EXPORT_LIST} = '$(BASEEXT).def';
}

=item os_flavor

OS/2 is OS/2

=cut

sub os_flavor {
    return('OS/2');
}

=item xs_static_lib_is_xs

=cut

sub xs_static_lib_is_xs {
    return 1;
}

=back

=cut

1;
