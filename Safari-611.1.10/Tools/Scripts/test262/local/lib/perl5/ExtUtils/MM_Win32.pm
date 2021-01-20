package ExtUtils::MM_Win32;

use strict;


=head1 NAME

ExtUtils::MM_Win32 - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_Win32; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=cut

use ExtUtils::MakeMaker::Config;
use File::Basename;
use File::Spec;
use ExtUtils::MakeMaker qw(neatvalue _sprintf562);

require ExtUtils::MM_Any;
require ExtUtils::MM_Unix;
our @ISA = qw( ExtUtils::MM_Any ExtUtils::MM_Unix );
our $VERSION = '7.32';
$VERSION = eval $VERSION;

$ENV{EMXSHELL} = 'sh'; # to run `commands`

my ( $BORLAND, $GCC, $MSVC ) = _identify_compiler_environment( \%Config );

sub _identify_compiler_environment {
    my ( $config ) = @_;

    my $BORLAND = $config->{cc} =~ /\bbcc/i ? 1 : 0;
    my $GCC     = $config->{cc} =~ /\bgcc\b/i ? 1 : 0;
    my $MSVC    = $config->{cc} =~ /\b(?:cl|icl)/i ? 1 : 0; # MSVC can come as clarm.exe, icl=Intel C

    return ( $BORLAND, $GCC, $MSVC );
}


=head2 Overridden methods

=over 4

=item B<dlsyms>

=cut

sub dlsyms {
    my($self,%attribs) = @_;
    return '' if $self->{SKIPHASH}{'dynamic'};
    $self->xs_dlsyms_iterator(\%attribs);
}

=item xs_dlsyms_ext

On Win32, is C<.def>.

=cut

sub xs_dlsyms_ext {
    '.def';
}

=item replace_manpage_separator

Changes the path separator with .

=cut

sub replace_manpage_separator {
    my($self,$man) = @_;
    $man =~ s,/+,.,g;
    $man;
}


=item B<maybe_command>

Since Windows has nothing as simple as an executable bit, we check the
file extension.

The PATHEXT env variable will be used to get a list of extensions that
might indicate a command, otherwise .com, .exe, .bat and .cmd will be
used by default.

=cut

sub maybe_command {
    my($self,$file) = @_;
    my @e = exists($ENV{'PATHEXT'})
          ? split(/;/, $ENV{PATHEXT})
      : qw(.com .exe .bat .cmd);
    my $e = '';
    for (@e) { $e .= "\Q$_\E|" }
    chop $e;
    # see if file ends in one of the known extensions
    if ($file =~ /($e)$/i) {
    return $file if -e $file;
    }
    else {
    for (@e) {
        return "$file$_" if -e "$file$_";
    }
    }
    return;
}


=item B<init_DIRFILESEP>

Using \ for Windows, except for "gmake" where it is /.

=cut

sub init_DIRFILESEP {
    my($self) = shift;

    # The ^ makes sure its not interpreted as an escape in nmake
    $self->{DIRFILESEP} = $self->is_make_type('nmake') ? '^\\' :
                          $self->is_make_type('dmake') ? '\\\\' :
                          $self->is_make_type('gmake') ? '/'
                                                       : '\\';
}

=item init_tools

Override some of the slower, portable commands with Windows specific ones.

=cut

sub init_tools {
    my ($self) = @_;

    $self->{NOOP}     ||= 'rem';
    $self->{DEV_NULL} ||= '> NUL';

    $self->{FIXIN}    ||= $self->{PERL_CORE} ?
      "\$(PERLRUN) $self->{PERL_SRC}\\win32\\bin\\pl2bat.pl" :
      'pl2bat.bat';

    $self->SUPER::init_tools;

    # Setting SHELL from $Config{sh} can break dmake.  Its ok without it.
    delete $self->{SHELL};

    return;
}


=item init_others

Override the default link and compile tools.

LDLOADLIBS's default is changed to $Config{libs}.

Adjustments are made for Borland's quirks needing -L to come first.

=cut

sub init_others {
    my $self = shift;

    $self->{LD}     ||= 'link';
    $self->{AR}     ||= 'lib';

    $self->SUPER::init_others;

    $self->{LDLOADLIBS} ||= $Config{libs};
    # -Lfoo must come first for Borland, so we put it in LDDLFLAGS
    if ($BORLAND) {
        my $libs = $self->{LDLOADLIBS};
        my $libpath = '';
        while ($libs =~ s/(?:^|\s)(("?)-L.+?\2)(?:\s|$)/ /) {
            $libpath .= ' ' if length $libpath;
            $libpath .= $1;
        }
        $self->{LDLOADLIBS} = $libs;
        $self->{LDDLFLAGS} ||= $Config{lddlflags};
        $self->{LDDLFLAGS} .= " $libpath";
    }

    return;
}


=item init_platform

Add MM_Win32_VERSION.

=item platform_constants

=cut

sub init_platform {
    my($self) = shift;

    $self->{MM_Win32_VERSION} = $VERSION;

    return;
}

sub platform_constants {
    my($self) = shift;
    my $make_frag = '';

    foreach my $macro (qw(MM_Win32_VERSION))
    {
        next unless defined $self->{$macro};
        $make_frag .= "$macro = $self->{$macro}\n";
    }

    return $make_frag;
}

=item specify_shell

Set SHELL to $ENV{COMSPEC} only if make is type 'gmake'.

=cut

sub specify_shell {
    my $self = shift;
    return '' unless $self->is_make_type('gmake');
    "\nSHELL = $ENV{COMSPEC}\n";
}

=item constants

Add MAXLINELENGTH for dmake before all the constants are output.

=cut

sub constants {
    my $self = shift;

    my $make_text = $self->SUPER::constants;
    return $make_text unless $self->is_make_type('dmake');

    # dmake won't read any single "line" (even those with escaped newlines)
    # larger than a certain size which can be as small as 8k.  PM_TO_BLIB
    # on large modules like DateTime::TimeZone can create lines over 32k.
    # So we'll crank it up to a <ironic>WHOPPING</ironic> 64k.
    #
    # This has to come here before all the constants and not in
    # platform_constants which is after constants.
    my $size = $self->{MAXLINELENGTH} || 800000;
    my $prefix = qq{
# Get dmake to read long commands like PM_TO_BLIB
MAXLINELENGTH = $size

};

    return $prefix . $make_text;
}


=item special_targets

Add .USESHELL target for dmake.

=cut

sub special_targets {
    my($self) = @_;

    my $make_frag = $self->SUPER::special_targets;

    $make_frag .= <<'MAKE_FRAG' if $self->is_make_type('dmake');
.USESHELL :
MAKE_FRAG

    return $make_frag;
}

=item static_lib_pure_cmd

Defines how to run the archive utility

=cut

sub static_lib_pure_cmd {
    my ($self, $from) = @_;
    $from =~ s/(\$\(\w+)(\))/$1:^"+"$2/g if $BORLAND;
    sprintf qq{\t\$(AR) %s\n}, ($BORLAND ? '$@ ' . $from
                          : ($GCC ? '-ru $@ ' . $from
                                  : '-out:$@ ' . $from));
}

=item dynamic_lib

Methods are overridden here: not dynamic_lib itself, but the utility
ones that do the OS-specific work.

=cut

sub xs_make_dynamic_lib {
    my ($self, $attribs, $from, $to, $todir, $ldfrom, $exportlist) = @_;
    my @m = sprintf '%s : %s $(MYEXTLIB) %s$(DFSEP).exists %s $(PERL_ARCHIVEDEP) $(INST_DYNAMIC_DEP)'."\n", $to, $from, $todir, $exportlist;
    if ($GCC) {
      # per https://rt.cpan.org/Ticket/Display.html?id=78395 no longer
      # uses dlltool - relies on post 2002 MinGW
      #                             1            2
      push @m, _sprintf562 <<'EOF', $exportlist, $ldfrom;
    $(LD) %1$s -o $@ $(LDDLFLAGS) %2$s $(OTHERLDFLAGS) $(MYEXTLIB) "$(PERL_ARCHIVE)" $(LDLOADLIBS) -Wl,--enable-auto-image-base
EOF
    } elsif ($BORLAND) {
      my $ldargs = $self->is_make_type('dmake')
          ? q{"$(PERL_ARCHIVE:s,/,\,)" $(LDLOADLIBS:s,/,\,) $(MYEXTLIB:s,/,\,),}
          : q{"$(subst /,\,$(PERL_ARCHIVE))" $(subst /,\,$(LDLOADLIBS)) $(subst /,\,$(MYEXTLIB)),};
      my $subbed;
      if ($exportlist eq '$(EXPORT_LIST)') {
          $subbed = $self->is_make_type('dmake')
              ? q{$(EXPORT_LIST:s,/,\,)}
              : q{$(subst /,\,$(EXPORT_LIST))};
      } else {
            # in XSMULTI, exportlist is per-XS, so have to sub in perl not make
          ($subbed = $exportlist) =~ s#/#\\#g;
      }
      push @m, sprintf <<'EOF', $ldfrom, $ldargs . $subbed;
        $(LD) $(LDDLFLAGS) $(OTHERLDFLAGS) %s,$@,,%s,$(RESFILES)
EOF
    } else {    # VC
      push @m, sprintf <<'EOF', $ldfrom, $exportlist;
    $(LD) -out:$@ $(LDDLFLAGS) %s $(OTHERLDFLAGS) $(MYEXTLIB) "$(PERL_ARCHIVE)" $(LDLOADLIBS) -def:%s
EOF
      # Embed the manifest file if it exists
      push(@m, q{    if exist $@.manifest mt -nologo -manifest $@.manifest -outputresource:$@;2
    if exist $@.manifest del $@.manifest});
    }
    push @m, "\n\t\$(CHMOD) \$(PERM_RWX) \$\@\n";

    join '', @m;
}

sub xs_dynamic_lib_macros {
    my ($self, $attribs) = @_;
    my $otherldflags = $attribs->{OTHERLDFLAGS} || ($BORLAND ? 'c0d32.obj': '');
    my $inst_dynamic_dep = $attribs->{INST_DYNAMIC_DEP} || "";
    sprintf <<'EOF', $otherldflags, $inst_dynamic_dep;
# This section creates the dynamically loadable objects from relevant
# objects and possibly $(MYEXTLIB).
OTHERLDFLAGS = %s
INST_DYNAMIC_DEP = %s
EOF
}

=item extra_clean_files

Clean out some extra dll.{base,exp} files which might be generated by
gcc.  Otherwise, take out all *.pdb files.

=cut

sub extra_clean_files {
    my $self = shift;

    return $GCC ? (qw(dll.base dll.exp)) : ('*.pdb');
}

=item init_linker

=cut

sub init_linker {
    my $self = shift;

    $self->{PERL_ARCHIVE}       = "\$(PERL_INC)\\$Config{libperl}";
    $self->{PERL_ARCHIVEDEP}    = "\$(PERL_INCDEP)\\$Config{libperl}";
    $self->{PERL_ARCHIVE_AFTER} = '';
    $self->{EXPORT_LIST}        = '$(BASEEXT).def';
}


=item perl_script

Checks for the perl program under several common perl extensions.

=cut

sub perl_script {
    my($self,$file) = @_;
    return $file if -r $file && -f _;
    return "$file.pl"  if -r "$file.pl" && -f _;
    return "$file.plx" if -r "$file.plx" && -f _;
    return "$file.bat" if -r "$file.bat" && -f _;
    return;
}

sub can_dep_space {
    my $self = shift;
    1; # with Win32::GetShortPathName
}

=item quote_dep

=cut

sub quote_dep {
    my ($self, $arg) = @_;
    if ($arg =~ / / and not $self->is_make_type('gmake')) {
        require Win32;
        $arg = Win32::GetShortPathName($arg);
        die <<EOF if not defined $arg or $arg =~ / /;
Tried to use make dependency with space for non-GNU make:
  '$arg'
Fallback to short pathname failed.
EOF
        return $arg;
    }
    return $self->SUPER::quote_dep($arg);
}


=item xs_obj_opt

Override to fixup -o flags for MSVC.

=cut

sub xs_obj_opt {
    my ($self, $output_file) = @_;
    ($MSVC ? "/Fo" : "-o ") . $output_file;
}


=item pasthru

All we send is -nologo to nmake to prevent it from printing its damned
banner.

=cut

sub pasthru {
    my($self) = shift;
    my $old = $self->SUPER::pasthru;
    return $old unless $self->is_make_type('nmake');
    $old =~ s/(PASTHRU\s*=\s*)/$1 -nologo /;
    $old;
}


=item arch_check (override)

Normalize all arguments for consistency of comparison.

=cut

sub arch_check {
    my $self = shift;

    # Win32 is an XS module, minperl won't have it.
    # arch_check() is not critical, so just fake it.
    return 1 unless $self->can_load_xs;
    return $self->SUPER::arch_check( map { $self->_normalize_path_name($_) } @_);
}

sub _normalize_path_name {
    my $self = shift;
    my $file = shift;

    require Win32;
    my $short = Win32::GetShortPathName($file);
    return defined $short ? lc $short : lc $file;
}


=item oneliner

These are based on what command.com does on Win98.  They may be wrong
for other Windows shells, I don't know.

=cut

sub oneliner {
    my($self, $cmd, $switches) = @_;
    $switches = [] unless defined $switches;

    # Strip leading and trailing newlines
    $cmd =~ s{^\n+}{};
    $cmd =~ s{\n+$}{};

    $cmd = $self->quote_literal($cmd);
    $cmd = $self->escape_newlines($cmd);

    $switches = join ' ', @$switches;

    return qq{\$(ABSPERLRUN) $switches -e $cmd --};
}


sub quote_literal {
    my($self, $text, $opts) = @_;
    $opts->{allow_variables} = 1 unless defined $opts->{allow_variables};

    # See: http://www.autohotkey.net/~deleyd/parameters/parameters.htm#CPP

    # Apply the Microsoft C/C++ parsing rules
    $text =~ s{\\\\"}{\\\\\\\\\\"}g;  # \\" -> \\\\\"
    $text =~ s{(?<!\\)\\"}{\\\\\\"}g; # \"  -> \\\"
    $text =~ s{(?<!\\)"}{\\"}g;       # "   -> \"
    $text = qq{"$text"} if $text =~ /[ \t]/;

    # Apply the Command Prompt parsing rules (cmd.exe)
    my @text = split /("[^"]*")/, $text;
    # We should also escape parentheses, but it breaks one-liners containing
    # $(MACRO)s in makefiles.
    s{([<>|&^@!])}{^$1}g foreach grep { !/^"[^"]*"$/ } @text;
    $text = join('', @text);

    # dmake expands {{ to { and }} to }.
    if( $self->is_make_type('dmake') ) {
        $text =~ s/{/{{/g;
        $text =~ s/}/}}/g;
    }

    $text = $opts->{allow_variables}
      ? $self->escape_dollarsigns($text) : $self->escape_all_dollarsigns($text);

    return $text;
}


sub escape_newlines {
    my($self, $text) = @_;

    # Escape newlines
    $text =~ s{\n}{\\\n}g;

    return $text;
}


=item cd

dmake can handle Unix style cd'ing but nmake (at least 1.5) cannot.  It
wants:

    cd dir1\dir2
    command
    another_command
    cd ..\..

=cut

sub cd {
    my($self, $dir, @cmds) = @_;

    return $self->SUPER::cd($dir, @cmds) unless $self->is_make_type('nmake');

    my $cmd = join "\n\t", map "$_", @cmds;

    my $updirs = $self->catdir(map { $self->updir } $self->splitdir($dir));

    # No leading tab and no trailing newline makes for easier embedding.
    my $make_frag = sprintf <<'MAKE_FRAG', $dir, $cmd, $updirs;
cd %s
    %s
    cd %s
MAKE_FRAG

    chomp $make_frag;

    return $make_frag;
}


=item max_exec_len

nmake 1.50 limits command length to 2048 characters.

=cut

sub max_exec_len {
    my $self = shift;

    return $self->{_MAX_EXEC_LEN} ||= 2 * 1024;
}


=item os_flavor

Windows is Win32.

=cut

sub os_flavor {
    return('Win32');
}


=item cflags

Defines the PERLDLL symbol if we are configured for static building since all
code destined for the perl5xx.dll must be compiled with the PERLDLL symbol
defined.

=cut

sub cflags {
    my($self,$libperl)=@_;
    return $self->{CFLAGS} if $self->{CFLAGS};
    return '' unless $self->needs_linking();

    my $base = $self->SUPER::cflags($libperl);
    foreach (split /\n/, $base) {
        /^(\S*)\s*=\s*(\S*)$/ and $self->{$1} = $2;
    };
    $self->{CCFLAGS} .= " -DPERLDLL" if ($self->{LINKTYPE} eq 'static');

    return $self->{CFLAGS} = qq{
CCFLAGS = $self->{CCFLAGS}
OPTIMIZE = $self->{OPTIMIZE}
PERLTYPE = $self->{PERLTYPE}
};

}

=item make_type

Returns a suitable string describing the type of makefile being written.

=cut

sub make_type {
    my ($self) = @_;
    my $make = $self->make;
    $make = +( File::Spec->splitpath( $make ) )[-1];
    $make =~ s!\.exe$!!i;
    if ( $make =~ m![^A-Z0-9]!i ) {
      ($make) = grep { m!make!i } split m![^A-Z0-9]!i, $make;
    }
    return "$make-style";
}

1;
__END__

=back
