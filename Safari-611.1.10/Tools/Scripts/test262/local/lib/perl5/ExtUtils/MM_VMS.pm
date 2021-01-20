package ExtUtils::MM_VMS;

use strict;

use ExtUtils::MakeMaker::Config;
require Exporter;

BEGIN {
    # so we can compile the thing on non-VMS platforms.
    if( $^O eq 'VMS' ) {
        require VMS::Filespec;
        VMS::Filespec->import;
    }
}

use File::Basename;

our $VERSION = '7.32';
$VERSION = eval $VERSION;

require ExtUtils::MM_Any;
require ExtUtils::MM_Unix;
our @ISA = qw( ExtUtils::MM_Any ExtUtils::MM_Unix );

use ExtUtils::MakeMaker qw($Verbose neatvalue _sprintf562);
our $Revision = $ExtUtils::MakeMaker::Revision;


=head1 NAME

ExtUtils::MM_VMS - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

  Do not use this directly.
  Instead, use ExtUtils::MM and it will figure out which MM_*
  class to use for you.

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=head2 Methods always loaded

=over 4

=item wraplist

Converts a list into a string wrapped at approximately 80 columns.

=cut

sub wraplist {
    my($self) = shift;
    my($line,$hlen) = ('',0);

    foreach my $word (@_) {
      # Perl bug -- seems to occasionally insert extra elements when
      # traversing array (scalar(@array) doesn't show them, but
      # foreach(@array) does) (5.00307)
      next unless $word =~ /\w/;
      $line .= ' ' if length($line);
      if ($hlen > 80) { $line .= "\\\n\t"; $hlen = 0; }
      $line .= $word;
      $hlen += length($word) + 2;
    }
    $line;
}


# This isn't really an override.  It's just here because ExtUtils::MM_VMS
# appears in @MM::ISA before ExtUtils::Liblist::Kid, so if there isn't an ext()
# in MM_VMS, then AUTOLOAD is called, and bad things happen.  So, we just
# mimic inheritance here and hand off to ExtUtils::Liblist::Kid.
# XXX This hackery will die soon. --Schwern
sub ext {
    require ExtUtils::Liblist::Kid;
    goto &ExtUtils::Liblist::Kid::ext;
}

=back

=head2 Methods

Those methods which override default MM_Unix methods are marked
"(override)", while methods unique to MM_VMS are marked "(specific)".
For overridden methods, documentation is limited to an explanation
of why this method overrides the MM_Unix method; see the ExtUtils::MM_Unix
documentation for more details.

=over 4

=item guess_name (override)

Try to determine name of extension being built.  We begin with the name
of the current directory.  Since VMS filenames are case-insensitive,
however, we look for a F<.pm> file whose name matches that of the current
directory (presumably the 'main' F<.pm> file for this extension), and try
to find a C<package> statement from which to obtain the Mixed::Case
package name.

=cut

sub guess_name {
    my($self) = @_;
    my($defname,$defpm,@pm,%xs);
    local *PM;

    $defname = basename(fileify($ENV{'DEFAULT'}));
    $defname =~ s![\d\-_]*\.dir.*$!!;  # Clip off .dir;1 suffix, and package version
    $defpm = $defname;
    # Fallback in case for some reason a user has copied the files for an
    # extension into a working directory whose name doesn't reflect the
    # extension's name.  We'll use the name of a unique .pm file, or the
    # first .pm file with a matching .xs file.
    if (not -e "${defpm}.pm") {
      @pm = glob('*.pm');
      s/.pm$// for @pm;
      if (@pm == 1) { ($defpm = $pm[0]) =~ s/.pm$//; }
      elsif (@pm) {
        %xs = map { s/.xs$//; ($_,1) } glob('*.xs');  ## no critic
        if (keys %xs) {
            foreach my $pm (@pm) {
                $defpm = $pm, last if exists $xs{$pm};
            }
        }
      }
    }
    if (open(my $pm, '<', "${defpm}.pm")){
        while (<$pm>) {
            if (/^\s*package\s+([^;]+)/i) {
                $defname = $1;
                last;
            }
        }
        print "Warning (non-fatal): Couldn't find package name in ${defpm}.pm;\n\t",
                     "defaulting package name to $defname\n"
            if eof($pm);
        close $pm;
    }
    else {
        print "Warning (non-fatal): Couldn't find ${defpm}.pm;\n\t",
                     "defaulting package name to $defname\n";
    }
    $defname =~ s#[\d.\-_]+$##;
    $defname;
}

=item find_perl (override)

Use VMS file specification syntax and CLI commands to find and
invoke Perl images.

=cut

sub find_perl {
    my($self, $ver, $names, $dirs, $trace) = @_;
    my($vmsfile,@sdirs,@snames,@cand);
    my($rslt);
    my($inabs) = 0;
    local *TCF;

    if( $self->{PERL_CORE} ) {
        # Check in relative directories first, so we pick up the current
        # version of Perl if we're running MakeMaker as part of the main build.
        @sdirs = sort { my($absa) = $self->file_name_is_absolute($a);
                        my($absb) = $self->file_name_is_absolute($b);
                        if ($absa && $absb) { return $a cmp $b }
                        else { return $absa ? 1 : ($absb ? -1 : ($a cmp $b)); }
                      } @$dirs;
        # Check miniperl before perl, and check names likely to contain
        # version numbers before "generic" names, so we pick up an
        # executable that's less likely to be from an old installation.
        @snames = sort { my($ba) = $a =~ m!([^:>\]/]+)$!;  # basename
                         my($bb) = $b =~ m!([^:>\]/]+)$!;
                         my($ahasdir) = (length($a) - length($ba) > 0);
                         my($bhasdir) = (length($b) - length($bb) > 0);
                         if    ($ahasdir and not $bhasdir) { return 1; }
                         elsif ($bhasdir and not $ahasdir) { return -1; }
                         else { $bb =~ /\d/ <=> $ba =~ /\d/
                                  or substr($ba,0,1) cmp substr($bb,0,1)
                                  or length($bb) <=> length($ba) } } @$names;
    }
    else {
        @sdirs  = @$dirs;
        @snames = @$names;
    }

    # Image names containing Perl version use '_' instead of '.' under VMS
    s/\.(\d+)$/_$1/ for @snames;
    if ($trace >= 2){
        print "Looking for perl $ver by these names:\n";
        print "\t@snames,\n";
        print "in these dirs:\n";
        print "\t@sdirs\n";
    }
    foreach my $dir (@sdirs){
        next unless defined $dir; # $self->{PERL_SRC} may be undefined
        $inabs++ if $self->file_name_is_absolute($dir);
        if ($inabs == 1) {
            # We've covered relative dirs; everything else is an absolute
            # dir (probably an installed location).  First, we'll try
            # potential command names, to see whether we can avoid a long
            # MCR expression.
            foreach my $name (@snames) {
                push(@cand,$name) if $name =~ /^[\w\-\$]+$/;
            }
            $inabs++; # Should happen above in next $dir, but just in case...
        }
        foreach my $name (@snames){
            push @cand, ($name !~ m![/:>\]]!) ? $self->catfile($dir,$name)
                                              : $self->fixpath($name,0);
        }
    }
    foreach my $name (@cand) {
        print "Checking $name\n" if $trace >= 2;
        # If it looks like a potential command, try it without the MCR
        if ($name =~ /^[\w\-\$]+$/) {
            open(my $tcf, ">", "temp_mmvms.com")
                or die('unable to open temp file');
            print $tcf "\$ set message/nofacil/nosever/noident/notext\n";
            print $tcf "\$ $name -e \"require $ver; print \"\"VER_OK\\n\"\"\"\n";
            close $tcf;
            $rslt = `\@temp_mmvms.com` ;
            unlink('temp_mmvms.com');
            if ($rslt =~ /VER_OK/) {
                print "Using PERL=$name\n" if $trace;
                return $name;
            }
        }
        next unless $vmsfile = $self->maybe_command($name);
        $vmsfile =~ s/;[\d\-]*$//;  # Clip off version number; we can use a newer version as well
        print "Executing $vmsfile\n" if ($trace >= 2);
        open(my $tcf, '>', "temp_mmvms.com")
                or die('unable to open temp file');
        print $tcf "\$ set message/nofacil/nosever/noident/notext\n";
        print $tcf "\$ mcr $vmsfile -e \"require $ver; print \"\"VER_OK\\n\"\"\" \n";
        close $tcf;
        $rslt = `\@temp_mmvms.com`;
        unlink('temp_mmvms.com');
        if ($rslt =~ /VER_OK/) {
            print "Using PERL=MCR $vmsfile\n" if $trace;
            return "MCR $vmsfile";
        }
    }
    print "Unable to find a perl $ver (by these names: @$names, in these dirs: @$dirs)\n";
    0; # false and not empty
}

=item _fixin_replace_shebang (override)

Helper routine for MM->fixin(), overridden because there's no such thing as an
actual shebang line that will be interpreted by the shell, so we just prepend
$Config{startperl} and preserve the shebang line argument for any switches it
may contain.

=cut

sub _fixin_replace_shebang {
    my ( $self, $file, $line ) = @_;

    my ( undef, $arg ) = split ' ', $line, 2;

    return $Config{startperl} . "\n" . $Config{sharpbang} . "perl $arg\n";
}

=item maybe_command (override)

Follows VMS naming conventions for executable files.
If the name passed in doesn't exactly match an executable file,
appends F<.Exe> (or equivalent) to check for executable image, and F<.Com>
to check for DCL procedure.  If this fails, checks directories in DCL$PATH
and finally F<Sys$System:> for an executable file having the name specified,
with or without the F<.Exe>-equivalent suffix.

=cut

sub maybe_command {
    my($self,$file) = @_;
    return $file if -x $file && ! -d _;
    my(@dirs) = ('');
    my(@exts) = ('',$Config{'exe_ext'},'.exe','.com');

    if ($file !~ m![/:>\]]!) {
        for (my $i = 0; defined $ENV{"DCL\$PATH;$i"}; $i++) {
            my $dir = $ENV{"DCL\$PATH;$i"};
            $dir .= ':' unless $dir =~ m%[\]:]$%;
            push(@dirs,$dir);
        }
        push(@dirs,'Sys$System:');
        foreach my $dir (@dirs) {
            my $sysfile = "$dir$file";
            foreach my $ext (@exts) {
                return $file if -x "$sysfile$ext" && ! -d _;
            }
        }
    }
    return 0;
}


=item pasthru (override)

The list of macro definitions to be passed through must be specified using
the /MACRO qualifier and must not add another /DEFINE qualifier.  We prepend
our own comma here to the contents of $(PASTHRU_DEFINE) because it is often
empty and a comma always present in CCFLAGS would generate a missing
qualifier value error.

=cut

sub pasthru {
    my($self) = shift;
    my $pasthru = $self->SUPER::pasthru;
    $pasthru =~ s|(PASTHRU\s*=\s*)|$1/MACRO=(|;
    $pasthru =~ s|\n\z|)\n|m;
    $pasthru =~ s|/defi?n?e?=\(?([^\),]+)\)?|,$1|ig;

    return $pasthru;
}


=item pm_to_blib (override)

VMS wants a dot in every file so we can't have one called 'pm_to_blib',
it becomes 'pm_to_blib.' and MMS/K isn't smart enough to know that when
you have a target called 'pm_to_blib' it should look for 'pm_to_blib.'.

So in VMS its pm_to_blib.ts.

=cut

sub pm_to_blib {
    my $self = shift;

    my $make = $self->SUPER::pm_to_blib;

    $make =~ s{^pm_to_blib :}{pm_to_blib.ts :}m;
    $make =~ s{\$\(TOUCH\) pm_to_blib}{\$(TOUCH) pm_to_blib.ts};

    $make = <<'MAKE' . $make;
# Dummy target to match Unix target name; we use pm_to_blib.ts as
# timestamp file to avoid repeated invocations under VMS
pm_to_blib : pm_to_blib.ts
    $(NOECHO) $(NOOP)

MAKE

    return $make;
}


=item perl_script (override)

If name passed in doesn't specify a readable file, appends F<.com> or
F<.pl> and tries again, since it's customary to have file types on all files
under VMS.

=cut

sub perl_script {
    my($self,$file) = @_;
    return $file if -r $file && ! -d _;
    return "$file.com" if -r "$file.com";
    return "$file.pl" if -r "$file.pl";
    return '';
}


=item replace_manpage_separator

Use as separator a character which is legal in a VMS-syntax file name.

=cut

sub replace_manpage_separator {
    my($self,$man) = @_;
    $man = unixify($man);
    $man =~ s#/+#__#g;
    $man;
}

=item init_DEST

(override) Because of the difficulty concatenating VMS filepaths we
must pre-expand the DEST* variables.

=cut

sub init_DEST {
    my $self = shift;

    $self->SUPER::init_DEST;

    # Expand DEST variables.
    foreach my $var ($self->installvars) {
        my $destvar = 'DESTINSTALL'.$var;
        $self->{$destvar} = $self->eliminate_macros($self->{$destvar});
    }
}


=item init_DIRFILESEP

No separator between a directory path and a filename on VMS.

=cut

sub init_DIRFILESEP {
    my($self) = shift;

    $self->{DIRFILESEP} = '';
    return 1;
}


=item init_main (override)


=cut

sub init_main {
    my($self) = shift;

    $self->SUPER::init_main;

    $self->{DEFINE} ||= '';
    if ($self->{DEFINE} ne '') {
        my(@terms) = split(/\s+/,$self->{DEFINE});
        my(@defs,@udefs);
        foreach my $def (@terms) {
            next unless $def;
            my $targ = \@defs;
            if ($def =~ s/^-([DU])//) {    # If it was a Unix-style definition
                $targ = \@udefs if $1 eq 'U';
                $def =~ s/='(.*)'$/=$1/;  # then remove shell-protection ''
                $def =~ s/^'(.*)'$/$1/;   # from entire term or argument
            }
            if ($def =~ /=/) {
                $def =~ s/"/""/g;  # Protect existing " from DCL
                $def = qq["$def"]; # and quote to prevent parsing of =
            }
            push @$targ, $def;
        }

        $self->{DEFINE} = '';
        if (@defs)  {
            $self->{DEFINE}  = '/Define=(' . join(',',@defs)  . ')';
        }
        if (@udefs) {
            $self->{DEFINE} .= '/Undef=('  . join(',',@udefs) . ')';
        }
    }
}

=item init_tools (override)

Provide VMS-specific forms of various utility commands.

Sets DEV_NULL to nothing because I don't know how to do it on VMS.

Changes EQUALIZE_TIMESTAMP to set revision date of target file to
one second later than source file, since MMK interprets precisely
equal revision dates for a source and target file as a sign that the
target needs to be updated.

=cut

sub init_tools {
    my($self) = @_;

    $self->{NOOP}               = 'Continue';
    $self->{NOECHO}             ||= '@ ';

    $self->{MAKEFILE}           ||= $self->{FIRST_MAKEFILE} || 'Descrip.MMS';
    $self->{FIRST_MAKEFILE}     ||= $self->{MAKEFILE};
    $self->{MAKE_APERL_FILE}    ||= 'Makeaperl.MMS';
    $self->{MAKEFILE_OLD}       ||= $self->eliminate_macros('$(FIRST_MAKEFILE)_old');
#
#   If an extension is not specified, then MMS/MMK assumes an
#   an extension of .MMS.  If there really is no extension,
#   then a trailing "." needs to be appended to specify a
#   a null extension.
#
    $self->{MAKEFILE} .= '.' unless $self->{MAKEFILE} =~ m/\./;
    $self->{FIRST_MAKEFILE} .= '.' unless $self->{FIRST_MAKEFILE} =~ m/\./;
    $self->{MAKE_APERL_FILE} .= '.' unless $self->{MAKE_APERL_FILE} =~ m/\./;
    $self->{MAKEFILE_OLD} .= '.' unless $self->{MAKEFILE_OLD} =~ m/\./;

    $self->{MACROSTART}         ||= '/Macro=(';
    $self->{MACROEND}           ||= ')';
    $self->{USEMAKEFILE}        ||= '/Descrip=';

    $self->{EQUALIZE_TIMESTAMP} ||= '$(ABSPERLRUN) -we "open F,qq{>>$ARGV[1]};close F;utime(0,(stat($ARGV[0]))[9]+1,$ARGV[1])"';

    $self->{MOD_INSTALL} ||=
      $self->oneliner(<<'CODE', ['-MExtUtils::Install']);
install([ from_to => {split('\|', <STDIN>)}, verbose => '$(VERBINST)', uninstall_shadows => '$(UNINST)', dir_mode => '$(PERM_DIR)' ]);
CODE

    $self->{UMASK_NULL} = '! ';

    $self->SUPER::init_tools;

    # Use the default shell
    $self->{SHELL}    ||= 'Posix';

    # Redirection on VMS goes before the command, not after as on Unix.
    # $(DEV_NULL) is used once and its not worth going nuts over making
    # it work.  However, Unix's DEV_NULL is quite wrong for VMS.
    $self->{DEV_NULL}   = '';

    return;
}

=item init_platform (override)

Add PERL_VMS, MM_VMS_REVISION and MM_VMS_VERSION.

MM_VMS_REVISION is for backwards compatibility before MM_VMS had a
$VERSION.

=cut

sub init_platform {
    my($self) = shift;

    $self->{MM_VMS_REVISION} = $Revision;
    $self->{MM_VMS_VERSION}  = $VERSION;
    $self->{PERL_VMS} = $self->catdir($self->{PERL_SRC}, 'VMS')
      if $self->{PERL_SRC};
}


=item platform_constants

=cut

sub platform_constants {
    my($self) = shift;
    my $make_frag = '';

    foreach my $macro (qw(PERL_VMS MM_VMS_REVISION MM_VMS_VERSION))
    {
        next unless defined $self->{$macro};
        $make_frag .= "$macro = $self->{$macro}\n";
    }

    return $make_frag;
}


=item init_VERSION (override)

Override the *DEFINE_VERSION macros with VMS semantics.  Translate the
MAKEMAKER filepath to VMS style.

=cut

sub init_VERSION {
    my $self = shift;

    $self->SUPER::init_VERSION;

    $self->{DEFINE_VERSION}    = '"$(VERSION_MACRO)=""$(VERSION)"""';
    $self->{XS_DEFINE_VERSION} = '"$(XS_VERSION_MACRO)=""$(XS_VERSION)"""';
    $self->{MAKEMAKER} = vmsify($INC{'ExtUtils/MakeMaker.pm'});
}


=item constants (override)

Fixes up numerous file and directory macros to insure VMS syntax
regardless of input syntax.  Also makes lists of files
comma-separated.

=cut

sub constants {
    my($self) = @_;

    # Be kind about case for pollution
    for (@ARGV) { $_ = uc($_) if /POLLUTE/i; }

    # Cleanup paths for directories in MMS macros.
    foreach my $macro ( qw [
            INST_BIN INST_SCRIPT INST_LIB INST_ARCHLIB
            PERL_LIB PERL_ARCHLIB
            PERL_INC PERL_SRC ],
                        (map { 'INSTALL'.$_ } $self->installvars)
                      )
    {
        next unless defined $self->{$macro};
        next if $macro =~ /MAN/ && $self->{$macro} eq 'none';
        $self->{$macro} = $self->fixpath($self->{$macro},1);
    }

    # Cleanup paths for files in MMS macros.
    foreach my $macro ( qw[LIBPERL_A FIRST_MAKEFILE MAKEFILE_OLD
                           MAKE_APERL_FILE MYEXTLIB] )
    {
        next unless defined $self->{$macro};
        $self->{$macro} = $self->fixpath($self->{$macro},0);
    }

    # Fixup files for MMS macros
    # XXX is this list complete?
    for my $macro (qw/
                   FULLEXT VERSION_FROM
          /    ) {
        next unless defined $self->{$macro};
        $self->{$macro} = $self->fixpath($self->{$macro},0);
    }


    for my $macro (qw/
                   OBJECT LDFROM
          /    ) {
        next unless defined $self->{$macro};

        # Must expand macros before splitting on unescaped whitespace.
        $self->{$macro} = $self->eliminate_macros($self->{$macro});
        if ($self->{$macro} =~ /(?<!\^)\s/) {
            $self->{$macro} =~ s/(\\)?\n+\s+/ /g;
            $self->{$macro} = $self->wraplist(
                map $self->fixpath($_,0), split /,?(?<!\^)\s+/, $self->{$macro}
            );
        }
        else {
            $self->{$macro} = $self->fixpath($self->{$macro},0);
        }
    }

    for my $macro (qw/ XS MAN1PODS MAN3PODS PM /) {
        # Where is the space coming from? --jhi
        next unless $self ne " " && defined $self->{$macro};
        my %tmp = ();
        for my $key (keys %{$self->{$macro}}) {
            $tmp{$self->fixpath($key,0)} =
                                     $self->fixpath($self->{$macro}{$key},0);
        }
        $self->{$macro} = \%tmp;
    }

    for my $macro (qw/ C O_FILES H /) {
        next unless defined $self->{$macro};
        my @tmp = ();
        for my $val (@{$self->{$macro}}) {
            push(@tmp,$self->fixpath($val,0));
        }
        $self->{$macro} = \@tmp;
    }

    # mms/k does not define a $(MAKE) macro.
    $self->{MAKE} = '$(MMS)$(MMSQUALIFIERS)';

    return $self->SUPER::constants;
}


=item special_targets

Clear the default .SUFFIXES and put in our own list.

=cut

sub special_targets {
    my $self = shift;

    my $make_frag .= <<'MAKE_FRAG';
.SUFFIXES :
.SUFFIXES : $(OBJ_EXT) .c .cpp .cxx .xs

MAKE_FRAG

    return $make_frag;
}

=item cflags (override)

Bypass shell script and produce qualifiers for CC directly (but warn
user if a shell script for this extension exists).  Fold multiple
/Defines into one, since some C compilers pay attention to only one
instance of this qualifier on the command line.

=cut

sub cflags {
    my($self,$libperl) = @_;
    my($quals) = $self->{CCFLAGS} || $Config{'ccflags'};
    my($definestr,$undefstr,$flagoptstr) = ('','','');
    my($incstr) = '/Include=($(PERL_INC)';
    my($name,$sys,@m);

    ( $name = $self->{NAME} . "_cflags" ) =~ s/:/_/g ;
    print "Unix shell script ".$Config{"$self->{'BASEEXT'}_cflags"}.
         " required to modify CC command for $self->{'BASEEXT'}\n"
    if ($Config{$name});

    if ($quals =~ / -[DIUOg]/) {
    while ($quals =~ / -([Og])(\d*)\b/) {
        my($type,$lvl) = ($1,$2);
        $quals =~ s/ -$type$lvl\b\s*//;
        if ($type eq 'g') { $flagoptstr = '/NoOptimize'; }
        else { $flagoptstr = '/Optimize' . (defined($lvl) ? "=$lvl" : ''); }
    }
    while ($quals =~ / -([DIU])(\S+)/) {
        my($type,$def) = ($1,$2);
        $quals =~ s/ -$type$def\s*//;
        $def =~ s/"/""/g;
        if    ($type eq 'D') { $definestr .= qq["$def",]; }
        elsif ($type eq 'I') { $incstr .= ',' . $self->fixpath($def,1); }
        else                 { $undefstr  .= qq["$def",]; }
    }
    }
    if (length $quals and $quals !~ m!/!) {
    warn "MM_VMS: Ignoring unrecognized CCFLAGS elements \"$quals\"\n";
    $quals = '';
    }
    $definestr .= q["PERL_POLLUTE",] if $self->{POLLUTE};
    if (length $definestr) { chop($definestr); $quals .= "/Define=($definestr)"; }
    if (length $undefstr)  { chop($undefstr);  $quals .= "/Undef=($undefstr)";   }
    # Deal with $self->{DEFINE} here since some C compilers pay attention
    # to only one /Define clause on command line, so we have to
    # conflate the ones from $Config{'ccflags'} and $self->{DEFINE}
    # ($self->{DEFINE} has already been VMSified in constants() above)
    if ($self->{DEFINE}) { $quals .= $self->{DEFINE}; }
    for my $type (qw(Def Undef)) {
    my(@terms);
    while ($quals =~ m:/${type}i?n?e?=([^/]+):ig) {
        my $term = $1;
        $term =~ s:^\((.+)\)$:$1:;
        push @terms, $term;
    }
    if ($type eq 'Def') {
        push @terms, qw[ $(DEFINE_VERSION) $(XS_DEFINE_VERSION) ];
    }
    if (@terms) {
        $quals =~ s:/${type}i?n?e?=[^/]+::ig;
            # PASTHRU_DEFINE will have its own comma
        $quals .= "/${type}ine=(" . join(',',@terms) . ($type eq 'Def' ? '$(PASTHRU_DEFINE)' : '') . ')';
    }
    }

    $libperl or $libperl = $self->{LIBPERL_A} || "libperl.olb";

    # Likewise with $self->{INC} and /Include
    if ($self->{'INC'}) {
    my(@includes) = split(/\s+/,$self->{INC});
    foreach (@includes) {
        s/^-I//;
        $incstr .= ','.$self->fixpath($_,1);
    }
    }
    $quals .= "$incstr)";
#    $quals =~ s/,,/,/g; $quals =~ s/\(,/(/g;
    $self->{CCFLAGS} = $quals;

    $self->{PERLTYPE} ||= '';

    $self->{OPTIMIZE} ||= $flagoptstr || $Config{'optimize'};
    if ($self->{OPTIMIZE} !~ m!/!) {
    if    ($self->{OPTIMIZE} =~ m!-g!) { $self->{OPTIMIZE} = '/Debug/NoOptimize' }
    elsif ($self->{OPTIMIZE} =~ /-O(\d*)/) {
        $self->{OPTIMIZE} = '/Optimize' . (defined($1) ? "=$1" : '');
    }
    else {
        warn "MM_VMS: Can't parse OPTIMIZE \"$self->{OPTIMIZE}\"; using default\n" if length $self->{OPTIMIZE};
        $self->{OPTIMIZE} = '/Optimize';
    }
    }

    return $self->{CFLAGS} = qq{
CCFLAGS = $self->{CCFLAGS}
OPTIMIZE = $self->{OPTIMIZE}
PERLTYPE = $self->{PERLTYPE}
};
}

=item const_cccmd (override)

Adds directives to point C preprocessor to the right place when
handling #include E<lt>sys/foo.hE<gt> directives.  Also constructs CC
command line a bit differently than MM_Unix method.

=cut

sub const_cccmd {
    my($self,$libperl) = @_;
    my(@m);

    return $self->{CONST_CCCMD} if $self->{CONST_CCCMD};
    return '' unless $self->needs_linking();
    if ($Config{'vms_cc_type'} eq 'gcc') {
        push @m,'
.FIRST
    ',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" Then Define/NoLog SYS GNU_CC_Include:[VMS]';
    }
    elsif ($Config{'vms_cc_type'} eq 'vaxc') {
        push @m,'
.FIRST
    ',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" .and. F$TrnLnm("VAXC$Include").eqs."" Then Define/NoLog SYS Sys$Library
    ',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" .and. F$TrnLnm("VAXC$Include").nes."" Then Define/NoLog SYS VAXC$Include';
    }
    else {
        push @m,'
.FIRST
    ',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" .and. F$TrnLnm("DECC$System_Include").eqs."" Then Define/NoLog SYS ',
        ($Config{'archname'} eq 'VMS_AXP' ? 'Sys$Library' : 'DECC$Library_Include'),'
    ',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" .and. F$TrnLnm("DECC$System_Include").nes."" Then Define/NoLog SYS DECC$System_Include';
    }

    push(@m, "\n\nCCCMD = $Config{'cc'} \$(CCFLAGS)\$(OPTIMIZE)\n");

    $self->{CONST_CCCMD} = join('',@m);
}


=item tools_other (override)

Throw in some dubious extra macros for Makefile args.

Also keep around the old $(SAY) macro in case somebody's using it.

=cut

sub tools_other {
    my($self) = @_;

    # XXX Are these necessary?  Does anyone override them?  They're longer
    # than just typing the literal string.
    my $extra_tools = <<'EXTRA_TOOLS';

# Just in case anyone is using the old macro.
USEMACROS = $(MACROSTART)
SAY = $(ECHO)

EXTRA_TOOLS

    return $self->SUPER::tools_other . $extra_tools;
}

=item init_dist (override)

VMSish defaults for some values.

  macro         description                     default

  ZIPFLAGS      flags to pass to ZIP            -Vu

  COMPRESS      compression command to          gzip
                use for tarfiles
  SUFFIX        suffix to put on                -gz
                compressed files

  SHAR          shar command to use             vms_share

  DIST_DEFAULT  default target to use to        tardist
                create a distribution

  DISTVNAME     Use VERSION_SYM instead of      $(DISTNAME)-$(VERSION_SYM)
                VERSION for the name

=cut

sub init_dist {
    my($self) = @_;
    $self->{ZIPFLAGS}     ||= '-Vu';
    $self->{COMPRESS}     ||= 'gzip';
    $self->{SUFFIX}       ||= '-gz';
    $self->{SHAR}         ||= 'vms_share';
    $self->{DIST_DEFAULT} ||= 'zipdist';

    $self->SUPER::init_dist;

    $self->{DISTVNAME} = "$self->{DISTNAME}-$self->{VERSION_SYM}"
      unless $self->{ARGS}{DISTVNAME};

    return;
}

=item c_o (override)

Use VMS syntax on command line.  In particular, $(DEFINE) and
$(PERL_INC) have been pulled into $(CCCMD).  Also use MM[SK] macros.

=cut

sub c_o {
    my($self) = @_;
    return '' unless $self->needs_linking();
    '
.c$(OBJ_EXT) :
    $(CCCMD) $(CCCDLFLAGS) $(MMS$TARGET_NAME).c /OBJECT=$(MMS$TARGET_NAME)$(OBJ_EXT)

.cpp$(OBJ_EXT) :
    $(CCCMD) $(CCCDLFLAGS) $(MMS$TARGET_NAME).cpp /OBJECT=$(MMS$TARGET_NAME)$(OBJ_EXT)

.cxx$(OBJ_EXT) :
    $(CCCMD) $(CCCDLFLAGS) $(MMS$TARGET_NAME).cxx /OBJECT=$(MMS$TARGET_NAME)$(OBJ_EXT)

';
}

=item xs_c (override)

Use MM[SK] macros.

=cut

sub xs_c {
    my($self) = @_;
    return '' unless $self->needs_linking();
    '
.xs.c :
    $(XSUBPPRUN) $(XSPROTOARG) $(XSUBPPARGS) $(MMS$TARGET_NAME).xs >$(MMS$TARGET_NAME).xsc
    $(MV) $(MMS$TARGET_NAME).xsc $(MMS$TARGET_NAME).c
';
}

=item xs_o (override)

Use MM[SK] macros, and VMS command line for C compiler.

=cut

sub xs_o {
    my ($self) = @_;
    return '' unless $self->needs_linking();
    my $frag = '
.xs$(OBJ_EXT) :
    $(XSUBPPRUN) $(XSPROTOARG) $(XSUBPPARGS) $(MMS$TARGET_NAME).xs >$(MMS$TARGET_NAME).xsc
    $(MV) $(MMS$TARGET_NAME).xsc $(MMS$TARGET_NAME).c
    $(CCCMD) $(CCCDLFLAGS) $(MMS$TARGET_NAME).c /OBJECT=$(MMS$TARGET_NAME)$(OBJ_EXT)
';
    if ($self->{XSMULTI}) {
    for my $ext ($self->_xs_list_basenames) {
        my $version = $self->parse_version("$ext.pm");
        my $ccflags = $self->{CCFLAGS};
        $ccflags =~ s/\$\(DEFINE_VERSION\)/\"VERSION_MACRO=\\"\"$version\\"\"/;
        $ccflags =~ s/\$\(XS_DEFINE_VERSION\)/\"XS_VERSION_MACRO=\\"\"$version\\"\"/;
        $self->_xsbuild_replace_macro($ccflags, 'xs', $ext, 'INC');
        $self->_xsbuild_replace_macro($ccflags, 'xs', $ext, 'DEFINE');

        $frag .= _sprintf562 <<'EOF', $ext, $ccflags;

%1$s$(OBJ_EXT) : %1$s.xs
    $(XSUBPPRUN) $(XSPROTOARG) $(XSUBPPARGS) $(MMS$TARGET_NAME).xs > $(MMS$TARGET_NAME).xsc
    $(MV) $(MMS$TARGET_NAME).xsc $(MMS$TARGET_NAME).c
    $(CC)%2$s$(OPTIMIZE) $(CCCDLFLAGS) $(MMS$TARGET_NAME).c /OBJECT=$(MMS$TARGET_NAME)$(OBJ_EXT)
EOF
    }
    }
    $frag;
}

=item _xsbuild_replace_macro (override)

There is no simple replacement possible since a qualifier and all its
subqualifiers must be considered together, so we use our own utility
routine for the replacement.

=cut

sub _xsbuild_replace_macro {
    my ($self, undef, $xstype, $ext, $varname) = @_;
    my $value = $self->_xsbuild_value($xstype, $ext, $varname);
    return unless defined $value;
    $_[1] = _vms_replace_qualifier($self, $_[1], $value, $varname);
}

=item _xsbuild_value (override)

Convert the extension spec to Unix format, as that's what will
match what's in the XSBUILD data structure.

=cut

sub _xsbuild_value {
    my ($self, $xstype, $ext, $varname) = @_;
    $ext = unixify($ext);
    return $self->SUPER::_xsbuild_value($xstype, $ext, $varname);
}

sub _vms_replace_qualifier {
    my ($self, $flags, $newflag, $macro) = @_;
    my $qual_type;
    my $type_suffix;
    my $quote_subquals = 0;
    my @subquals_new = split /\s+/, $newflag;

    if ($macro eq 'DEFINE') {
        $qual_type = 'Def';
        $type_suffix = 'ine';
        map { $_ =~ s/^-D// } @subquals_new;
        $quote_subquals = 1;
    }
    elsif ($macro eq 'INC') {
        $qual_type = 'Inc';
        $type_suffix = 'lude';
        map { $_ =~ s/^-I//; $_ = $self->fixpath($_) } @subquals_new;
    }

    my @subquals = ();
    while ($flags =~ m:/${qual_type}\S{0,4}=([^/]+):ig) {
        my $term = $1;
        $term =~ s/\"//g;
        $term =~ s:^\((.+)\)$:$1:;
        push @subquals, split /,/, $term;
    }
    for my $new (@subquals_new) {
        my ($sq_new, $sqval_new) = split /=/, $new;
        my $replaced_old = 0;
        for my $old (@subquals) {
            my ($sq, $sqval) = split /=/, $old;
            if ($sq_new eq $sq) {
                $old = $sq_new;
                $old .= '=' . $sqval_new if defined($sqval_new) and length($sqval_new);
                $replaced_old = 1;
                last;
            }
        }
        push @subquals, $new unless $replaced_old;
    }

    if (@subquals) {
        $flags =~ s:/${qual_type}\S{0,4}=[^/]+::ig;
        # add quotes if requested but not for unexpanded macros
        map { $_ = qq/"$_"/ if $_ !~ m/^\$\(/ } @subquals if $quote_subquals;
        $flags .= "/${qual_type}$type_suffix=(" . join(',',@subquals) . ')';
    }

    return $flags;
}


sub xs_dlsyms_ext {
    '.opt';
}

=item dlsyms (override)

Create VMS linker options files specifying universal symbols for this
extension's shareable image(s), and listing other shareable images or
libraries to which it should be linked.

=cut

sub dlsyms {
    my ($self, %attribs) = @_;
    return '' unless $self->needs_linking;
    $self->xs_dlsyms_iterator;
}

sub xs_make_dlsyms {
    my ($self, $attribs, $target, $dep, $name, $dlbase, $funcs, $funclist, $imports, $vars, $extra) = @_;
    my @m;
    my $instloc;
    if ($self->{XSMULTI}) {
    my ($v, $d, $f) = File::Spec->splitpath($target);
    my @d = File::Spec->splitdir($d);
    shift @d if $d[0] eq 'lib';
    $instloc = $self->catfile('$(INST_ARCHLIB)', 'auto', @d, $f);
    push @m,"\ndynamic :: $instloc\n\t\$(NOECHO) \$(NOOP)\n"
      unless $self->{SKIPHASH}{'dynamic'};
    push @m,"\nstatic :: $instloc\n\t\$(NOECHO) \$(NOOP)\n"
      unless $self->{SKIPHASH}{'static'};
    push @m, "\n", sprintf <<'EOF', $instloc, $target;
%s : %s
    $(CP) $(MMS$SOURCE) $(MMS$TARGET)
EOF
    }
    else {
    push @m,"\ndynamic :: \$(INST_ARCHAUTODIR)$self->{BASEEXT}.opt\n\t\$(NOECHO) \$(NOOP)\n"
      unless $self->{SKIPHASH}{'dynamic'};
    push @m,"\nstatic :: \$(INST_ARCHAUTODIR)$self->{BASEEXT}.opt\n\t\$(NOECHO) \$(NOOP)\n"
      unless $self->{SKIPHASH}{'static'};
    push @m, "\n", sprintf <<'EOF', $target;
$(INST_ARCHAUTODIR)$(BASEEXT).opt : %s
    $(CP) $(MMS$SOURCE) $(MMS$TARGET)
EOF
    }
    push @m,
     "\n$target : $dep\n\t",
     q!$(PERLRUN) -MExtUtils::Mksymlists -e "Mksymlists('NAME'=>'!, $name,
     q!', 'DLBASE' => '!,$dlbase,
     q!', 'DL_FUNCS' => !,neatvalue($funcs),
     q!, 'FUNCLIST' => !,neatvalue($funclist),
     q!, 'IMPORTS' => !,neatvalue($imports),
     q!, 'DL_VARS' => !, neatvalue($vars);
    push @m, $extra if defined $extra;
    push @m, qq!);"\n\t!;
    # Can't use dlbase as it's been through mod2fname.
    my $olb_base = basename($target, '.opt');
    if ($self->{XSMULTI}) {
        # We've been passed everything but the kitchen sink -- and the location of the
        # static library we're using to build the dynamic library -- so concoct that
        # location from what we do have.
        my $olb_dir = $self->catdir(dirname($instloc), $olb_base);
        push @m, qq!\$(PERL) -e "print ""${olb_dir}${olb_base}\$(LIB_EXT)/Include=!;
        push @m, ($Config{d_vms_case_sensitive_symbols} ? uc($olb_base) : $olb_base);
        push @m, '\n' . $olb_dir . $olb_base . '$(LIB_EXT)/Library\n"";" >>$(MMS$TARGET)',"\n";
    }
    else {
        push @m, qq!\$(PERL) -e "print ""\$(INST_ARCHAUTODIR)${olb_base}\$(LIB_EXT)/Include=!;
        if ($self->{OBJECT} =~ /\bBASEEXT\b/ or
            $self->{OBJECT} =~ /\b$self->{BASEEXT}\b/i) {
            push @m, ($Config{d_vms_case_sensitive_symbols}
                  ? uc($self->{BASEEXT}) :'$(BASEEXT)');
        }
        else {  # We don't have a "main" object file, so pull 'em all in
            # Upcase module names if linker is being case-sensitive
            my($upcase) = $Config{d_vms_case_sensitive_symbols};
            my(@omods) = split ' ', $self->eliminate_macros($self->{OBJECT});
            for (@omods) {
                s/\.[^.]*$//;         # Trim off file type
                s[\$\(\w+_EXT\)][];   # even as a macro
                s/.*[:>\/\]]//;       # Trim off dir spec
                $_ = uc if $upcase;
            };
            my(@lines);
            my $tmp = shift @omods;
            foreach my $elt (@omods) {
                $tmp .= ",$elt";
                if (length($tmp) > 80) { push @lines, $tmp;  $tmp = ''; }
            }
            push @lines, $tmp;
            push @m, '(', join( qq[, -\\n\\t"";" >>\$(MMS\$TARGET)\n\t\$(PERL) -e "print ""], @lines),')';
        }
        push @m, '\n$(INST_ARCHAUTODIR)' . $olb_base . '$(LIB_EXT)/Library\n"";" >>$(MMS$TARGET)',"\n";
    }
    if (length $self->{LDLOADLIBS}) {
        my($line) = '';
        foreach my $lib (split ' ', $self->{LDLOADLIBS}) {
            $lib =~ s%\$%\\\$%g;  # Escape '$' in VMS filespecs
            if (length($line) + length($lib) > 160) {
                push @m, "\t\$(PERL) -e \"print qq{$line}\" >>\$(MMS\$TARGET)\n";
                $line = $lib . '\n';
            }
            else { $line .= $lib . '\n'; }
        }
        push @m, "\t\$(PERL) -e \"print qq{$line}\" >>\$(MMS\$TARGET)\n" if $line;
    }
    join '', @m;
}


=item xs_obj_opt

Override to fixup -o flags.

=cut

sub xs_obj_opt {
    my ($self, $output_file) = @_;
    "/OBJECT=$output_file";
}

=item dynamic_lib (override)

Use VMS Link command.

=cut

sub xs_dynamic_lib_macros {
    my ($self, $attribs) = @_;
    my $otherldflags = $attribs->{OTHERLDFLAGS} || "";
    my $inst_dynamic_dep = $attribs->{INST_DYNAMIC_DEP} || "";
    sprintf <<'EOF', $otherldflags, $inst_dynamic_dep;
# This section creates the dynamically loadable objects from relevant
# objects and possibly $(MYEXTLIB).
OTHERLDFLAGS = %s
INST_DYNAMIC_DEP = %s
EOF
}

sub xs_make_dynamic_lib {
    my ($self, $attribs, $from, $to, $todir, $ldfrom, $exportlist) = @_;
    my $shr = $Config{'dbgprefix'} . 'PerlShr';
    $exportlist =~ s/.def$/.opt/;  # it's a linker options file
    #                    1    2       3            4     5
    _sprintf562 <<'EOF', $to, $todir, $exportlist, $shr, "$shr Sys\$Share:$shr.$Config{'dlext'}";
%1$s : $(INST_STATIC) $(PERL_INC)perlshr_attr.opt %2$s$(DFSEP).exists %3$s $(PERL_ARCHIVE) $(INST_DYNAMIC_DEP)
    If F$TrnLNm("%4$s").eqs."" Then Define/NoLog/User %5$s
    Link $(LDFLAGS) /Shareable=$(MMS$TARGET)$(OTHERLDFLAGS) %3$s/Option,$(PERL_INC)perlshr_attr.opt/Option
EOF
}

=item xs_make_static_lib (override)

Use VMS commands to manipulate object library.

=cut

sub xs_make_static_lib {
    my ($self, $object, $to, $todir) = @_;

    my @objects;
    if ($self->{XSMULTI}) {
        # The extension name should be the main object file name minus file type.
        my $lib = $object;
        $lib =~ s/\$\(OBJ_EXT\)\z//;
        my $override = $self->_xsbuild_value('xs', $lib, 'OBJECT');
        $object = $override if defined $override;
        @objects = map { $self->fixpath($_,0) } split /(?<!\^)\s+/, $object;
    }
    else {
        push @objects, $object;
    }

    my @m;
    for my $obj (@objects) {
        push(@m, sprintf "\n%s : %s\$(DFSEP).exists", $obj, $todir);
    }
    push(@m, sprintf "\n\n%s : %s \$(MYEXTLIB)\n", $to, (join ' ', @objects));

    # If this extension has its own library (eg SDBM_File)
    # then copy that to $(INST_STATIC) and add $(OBJECT) into it.
    push(@m, "\t",'$(CP) $(MYEXTLIB) $(MMS$TARGET)',"\n") if $self->{MYEXTLIB};

    push(@m,"\t",'If F$Search("$(MMS$TARGET)").eqs."" Then Library/Object/Create $(MMS$TARGET)',"\n");

    # if there was a library to copy, then we can't use MMS$SOURCE_LIST,
    # 'cause it's a library and you can't stick them in other libraries.
    # In that case, we use $OBJECT instead and hope for the best
    if ($self->{MYEXTLIB}) {
        for my $obj (@objects) {
            push(@m,"\t",'Library/Object/Replace $(MMS$TARGET) ' . $obj,"\n");
        }
    }
    else {
      push(@m,"\t",'Library/Object/Replace $(MMS$TARGET) $(MMS$SOURCE_LIST)',"\n");
    }

    push @m, "\t\$(NOECHO) \$(PERL) -e 1 >\$(INST_ARCHAUTODIR)extralibs.ld\n";
    foreach my $lib (split ' ', $self->{EXTRALIBS}) {
      push(@m,"\t",'$(NOECHO) $(PERL) -e "print qq{',$lib,'\n}" >>$(INST_ARCHAUTODIR)extralibs.ld',"\n");
    }
    join('',@m);
}


=item static_lib_pure_cmd (override)

Use VMS commands to manipulate object library.

=cut

sub static_lib_pure_cmd {
    my ($self, $from) = @_;

    sprintf <<'MAKE_FRAG', $from;
    If F$Search("$(MMS$TARGET)").eqs."" Then Library/Object/Create $(MMS$TARGET)
    Library/Object/Replace $(MMS$TARGET) %s
MAKE_FRAG
}

=item xs_static_lib_is_xs

=cut

sub xs_static_lib_is_xs {
    return 1;
}

=item extra_clean_files

Clean up some OS specific files.  Plus the temp file used to shorten
a lot of commands.  And the name mangler database.

=cut

sub extra_clean_files {
    return qw(
              *.Map *.Dmp *.Lis *.cpp *.$(DLEXT) *.Opt $(BASEEXT).bso
              .MM_Tmp cxx_repository
             );
}


=item zipfile_target

=item tarfile_target

=item shdist_target

Syntax for invoking shar, tar and zip differs from that for Unix.

=cut

sub zipfile_target {
    my($self) = shift;

    return <<'MAKE_FRAG';
$(DISTVNAME).zip : distdir
    $(PREOP)
    $(ZIP) "$(ZIPFLAGS)" $(MMS$TARGET) [.$(DISTVNAME)...]*.*;
    $(RM_RF) $(DISTVNAME)
    $(POSTOP)
MAKE_FRAG
}

sub tarfile_target {
    my($self) = shift;

    return <<'MAKE_FRAG';
$(DISTVNAME).tar$(SUFFIX) : distdir
    $(PREOP)
    $(TO_UNIX)
    $(TAR) "$(TARFLAGS)" $(DISTVNAME).tar [.$(DISTVNAME)...]
    $(RM_RF) $(DISTVNAME)
    $(COMPRESS) $(DISTVNAME).tar
    $(POSTOP)
MAKE_FRAG
}

sub shdist_target {
    my($self) = shift;

    return <<'MAKE_FRAG';
shdist : distdir
    $(PREOP)
    $(SHAR) [.$(DISTVNAME)...]*.*; $(DISTVNAME).share
    $(RM_RF) $(DISTVNAME)
    $(POSTOP)
MAKE_FRAG
}


# --- Test and Installation Sections ---

=item install (override)

Work around DCL's 255 character limit several times,and use
VMS-style command line quoting in a few cases.

=cut

sub install {
    my($self, %attribs) = @_;
    my(@m);

    push @m, q[
install :: all pure_install doc_install
    $(NOECHO) $(NOOP)

install_perl :: all pure_perl_install doc_perl_install
    $(NOECHO) $(NOOP)

install_site :: all pure_site_install doc_site_install
    $(NOECHO) $(NOOP)

install_vendor :: all pure_vendor_install doc_vendor_install
    $(NOECHO) $(NOOP)

pure_install :: pure_$(INSTALLDIRS)_install
    $(NOECHO) $(NOOP)

doc_install :: doc_$(INSTALLDIRS)_install
    $(NOECHO) $(NOOP)

pure__install : pure_site_install
    $(NOECHO) $(ECHO) "INSTALLDIRS not defined, defaulting to INSTALLDIRS=site"

doc__install : doc_site_install
    $(NOECHO) $(ECHO) "INSTALLDIRS not defined, defaulting to INSTALLDIRS=site"

# This hack brought to you by DCL's 255-character command line limit
pure_perl_install ::
];
    push @m,
q[    $(NOECHO) $(PERLRUN) "-MFile::Spec" -e "print 'read|'.File::Spec->catfile('$(PERL_ARCHLIB)','auto','$(FULLEXT)','.packlist').'|'" >.MM_tmp
    $(NOECHO) $(PERLRUN) "-MFile::Spec" -e "print 'write|'.File::Spec->catfile('$(DESTINSTALLARCHLIB)','auto','$(FULLEXT)','.packlist').'|'" >>.MM_tmp
] unless $self->{NO_PACKLIST};

    push @m,
q[    $(NOECHO) $(ECHO_N) "$(INST_LIB)|$(DESTINSTALLPRIVLIB)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_ARCHLIB)|$(DESTINSTALLARCHLIB)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_BIN)|$(DESTINSTALLBIN)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_SCRIPT)|$(DESTINSTALLSCRIPT)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_MAN1DIR) $(DESTINSTALLMAN1DIR) " >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_MAN3DIR)|$(DESTINSTALLMAN3DIR)" >>.MM_tmp
    $(NOECHO) $(MOD_INSTALL) <.MM_tmp
    $(NOECHO) $(RM_F) .MM_tmp
    $(NOECHO) $(WARN_IF_OLD_PACKLIST) "].$self->catfile($self->{SITEARCHEXP},'auto',$self->{FULLEXT},'.packlist').q["

# Likewise
pure_site_install ::
];
    push @m,
q[    $(NOECHO) $(PERLRUN) "-MFile::Spec" -e "print 'read|'.File::Spec->catfile('$(SITEARCHEXP)','auto','$(FULLEXT)','.packlist').'|'" >.MM_tmp
    $(NOECHO) $(PERLRUN) "-MFile::Spec" -e "print 'write|'.File::Spec->catfile('$(DESTINSTALLSITEARCH)','auto','$(FULLEXT)','.packlist').'|'" >>.MM_tmp
] unless $self->{NO_PACKLIST};

    push @m,
q[    $(NOECHO) $(ECHO_N) "$(INST_LIB)|$(DESTINSTALLSITELIB)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_ARCHLIB)|$(DESTINSTALLSITEARCH)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_BIN)|$(DESTINSTALLSITEBIN)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_SCRIPT)|$(DESTINSTALLSCRIPT)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_MAN1DIR)|$(DESTINSTALLSITEMAN1DIR)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_MAN3DIR)|$(DESTINSTALLSITEMAN3DIR)" >>.MM_tmp
    $(NOECHO) $(MOD_INSTALL) <.MM_tmp
    $(NOECHO) $(RM_F) .MM_tmp
    $(NOECHO) $(WARN_IF_OLD_PACKLIST) "].$self->catfile($self->{PERL_ARCHLIB},'auto',$self->{FULLEXT},'.packlist').q["

pure_vendor_install ::
];
    push @m,
q[    $(NOECHO) $(PERLRUN) "-MFile::Spec" -e "print 'read|'.File::Spec->catfile('$(VENDORARCHEXP)','auto','$(FULLEXT)','.packlist').'|'" >.MM_tmp
    $(NOECHO) $(PERLRUN) "-MFile::Spec" -e "print 'write|'.File::Spec->catfile('$(DESTINSTALLVENDORARCH)','auto','$(FULLEXT)','.packlist').'|'" >>.MM_tmp
] unless $self->{NO_PACKLIST};

    push @m,
q[    $(NOECHO) $(ECHO_N) "$(INST_LIB)|$(DESTINSTALLVENDORLIB)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_ARCHLIB)|$(DESTINSTALLVENDORARCH)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_BIN)|$(DESTINSTALLVENDORBIN)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_SCRIPT)|$(DESTINSTALLSCRIPT)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_MAN1DIR)|$(DESTINSTALLVENDORMAN1DIR)|" >>.MM_tmp
    $(NOECHO) $(ECHO_N) "$(INST_MAN3DIR)|$(DESTINSTALLVENDORMAN3DIR)" >>.MM_tmp
    $(NOECHO) $(MOD_INSTALL) <.MM_tmp
    $(NOECHO) $(RM_F) .MM_tmp

];

    push @m, q[
# Ditto
doc_perl_install ::
    $(NOECHO) $(NOOP)

# And again
doc_site_install ::
    $(NOECHO) $(NOOP)

doc_vendor_install ::
    $(NOECHO) $(NOOP)

] if $self->{NO_PERLLOCAL};

    push @m, q[
# Ditto
doc_perl_install ::
    $(NOECHO) $(ECHO) "Appending installation info to ].$self->catfile($self->{DESTINSTALLARCHLIB}, 'perllocal.pod').q["
    $(NOECHO) $(MKPATH) $(DESTINSTALLARCHLIB)
    $(NOECHO) $(ECHO_N) "installed into|$(INSTALLPRIVLIB)|" >.MM_tmp
    $(NOECHO) $(ECHO_N) "LINKTYPE|$(LINKTYPE)|VERSION|$(VERSION)|EXE_FILES|$(EXE_FILES) " >>.MM_tmp
    $(NOECHO) $(DOC_INSTALL) "Module" "$(NAME)" <.MM_tmp >>].$self->catfile($self->{DESTINSTALLARCHLIB},'perllocal.pod').q[
    $(NOECHO) $(RM_F) .MM_tmp

# And again
doc_site_install ::
    $(NOECHO) $(ECHO) "Appending installation info to ].$self->catfile($self->{DESTINSTALLARCHLIB}, 'perllocal.pod').q["
    $(NOECHO) $(MKPATH) $(DESTINSTALLARCHLIB)
    $(NOECHO) $(ECHO_N) "installed into|$(INSTALLSITELIB)|" >.MM_tmp
    $(NOECHO) $(ECHO_N) "LINKTYPE|$(LINKTYPE)|VERSION|$(VERSION)|EXE_FILES|$(EXE_FILES) " >>.MM_tmp
    $(NOECHO) $(DOC_INSTALL) "Module" "$(NAME)" <.MM_tmp >>].$self->catfile($self->{DESTINSTALLARCHLIB},'perllocal.pod').q[
    $(NOECHO) $(RM_F) .MM_tmp

doc_vendor_install ::
    $(NOECHO) $(ECHO) "Appending installation info to ].$self->catfile($self->{DESTINSTALLARCHLIB}, 'perllocal.pod').q["
    $(NOECHO) $(MKPATH) $(DESTINSTALLARCHLIB)
    $(NOECHO) $(ECHO_N) "installed into|$(INSTALLVENDORLIB)|" >.MM_tmp
    $(NOECHO) $(ECHO_N) "LINKTYPE|$(LINKTYPE)|VERSION|$(VERSION)|EXE_FILES|$(EXE_FILES) " >>.MM_tmp
    $(NOECHO) $(DOC_INSTALL) "Module" "$(NAME)" <.MM_tmp >>].$self->catfile($self->{DESTINSTALLARCHLIB},'perllocal.pod').q[
    $(NOECHO) $(RM_F) .MM_tmp

] unless $self->{NO_PERLLOCAL};

    push @m, q[
uninstall :: uninstall_from_$(INSTALLDIRS)dirs
    $(NOECHO) $(NOOP)

uninstall_from_perldirs ::
    $(NOECHO) $(UNINSTALL) ].$self->catfile($self->{PERL_ARCHLIB},'auto',$self->{FULLEXT},'.packlist').q[

uninstall_from_sitedirs ::
    $(NOECHO) $(UNINSTALL) ].$self->catfile($self->{SITEARCHEXP},'auto',$self->{FULLEXT},'.packlist').q[

uninstall_from_vendordirs ::
    $(NOECHO) $(UNINSTALL) ].$self->catfile($self->{VENDORARCHEXP},'auto',$self->{FULLEXT},'.packlist').q[
];

    join('',@m);
}

=item perldepend (override)

Use VMS-style syntax for files; it's cheaper to just do it directly here
than to have the MM_Unix method call C<catfile> repeatedly.  Also, if
we have to rebuild Config.pm, use MM[SK] to do it.

=cut

sub perldepend {
    my($self) = @_;
    my(@m);

    if ($self->{OBJECT}) {
        # Need to add an object file dependency on the perl headers.
        # this is very important for XS modules in perl.git development.

        push @m, $self->_perl_header_files_fragment(""); # empty separator on VMS as its in the $(PERL_INC)
    }

    if ($self->{PERL_SRC}) {
    my(@macros);
    my($mmsquals) = '$(USEMAKEFILE)[.vms]$(FIRST_MAKEFILE)';
    push(@macros,'__AXP__=1') if $Config{'archname'} eq 'VMS_AXP';
    push(@macros,'DECC=1')    if $Config{'vms_cc_type'} eq 'decc';
    push(@macros,'GNUC=1')    if $Config{'vms_cc_type'} eq 'gcc';
    push(@macros,'SOCKET=1')  if $Config{'d_has_sockets'};
    push(@macros,qq["CC=$Config{'cc'}"])  if $Config{'cc'} =~ m!/!;
    $mmsquals .= '$(USEMACROS)' . join(',',@macros) . '$(MACROEND)' if @macros;
    push(@m,q[
# Check for unpropagated config.sh changes. Should never happen.
# We do NOT just update config.h because that is not sufficient.
# An out of date config.h is not fatal but complains loudly!
$(PERL_INC)config.h : $(PERL_SRC)config.sh
    $(NOOP)

$(PERL_ARCHLIB)Config.pm : $(PERL_SRC)config.sh
    $(NOECHO) Write Sys$Error "$(PERL_ARCHLIB)Config.pm may be out of date with config.h or genconfig.pl"
    olddef = F$Environment("Default")
    Set Default $(PERL_SRC)
    $(MMS)],$mmsquals,);
    if ($self->{PERL_ARCHLIB} =~ m|\[-| && $self->{PERL_SRC} =~ m|(\[-+)|) {
        my($prefix,$target) = ($1,$self->fixpath('$(PERL_ARCHLIB)Config.pm',0));
        $target =~ s/\Q$prefix/[/;
        push(@m," $target");
    }
    else { push(@m,' $(MMS$TARGET)'); }
    push(@m,q[
    Set Default 'olddef'
]);
    }

    push(@m, join(" ", map($self->fixpath($_,0),sort values %{$self->{XS}}))." : \$(XSUBPPDEPS)\n")
      if %{$self->{XS}};

    join('',@m);
}


=item makeaperl (override)

Undertake to build a new set of Perl images using VMS commands.  Since
VMS does dynamic loading, it's not necessary to statically link each
extension into the Perl image, so this isn't the normal build path.
Consequently, it hasn't really been tested, and may well be incomplete.

=cut

our %olbs;  # needs to be localized

sub makeaperl {
    my($self, %attribs) = @_;
    my($makefilename, $searchdirs, $static, $extra, $perlinc, $target, $tmpdir, $libperl) =
      @attribs{qw(MAKE DIRS STAT EXTRA INCL TARGET TMP LIBPERL)};
    my(@m);
    push @m, "
# --- MakeMaker makeaperl section ---
MAP_TARGET    = $target
";
    return join '', @m if $self->{PARENT};

    my($dir) = join ":", @{$self->{DIR}};

    unless ($self->{MAKEAPERL}) {
    push @m, q{
$(MAKE_APERL_FILE) : $(FIRST_MAKEFILE)
    $(NOECHO) $(ECHO) "Writing ""$(MMS$TARGET)"" for this $(MAP_TARGET)"
    $(NOECHO) $(PERLRUNINST) \
        Makefile.PL DIR=}, $dir, q{ \
        FIRST_MAKEFILE=$(MAKE_APERL_FILE) LINKTYPE=static \
        MAKEAPERL=1 NORECURS=1 };

    push @m, map(q[ \\\n\t\t"$_"], @ARGV),q{

$(MAP_TARGET) :: $(MAKE_APERL_FILE)
    $(MAKE)$(USEMAKEFILE)$(MAKE_APERL_FILE) static $(MMS$TARGET)
};
    push @m, "\n";

    return join '', @m;
    }


    my($linkcmd,@optlibs,@staticpkgs,$extralist,$targdir,$libperldir,%libseen);
    local($_);

    # The front matter of the linkcommand...
    $linkcmd = join ' ', $Config{'ld'},
        grep($_, @Config{qw(large split ldflags ccdlflags)});
    $linkcmd =~ s/\s+/ /g;

    # Which *.olb files could we make use of...
    local(%olbs);       # XXX can this be lexical?
    $olbs{$self->{INST_ARCHAUTODIR}} = "$self->{BASEEXT}\$(LIB_EXT)";
    require File::Find;
    File::Find::find(sub {
    return unless m/\Q$self->{LIB_EXT}\E$/;
    return if m/^libperl/;

    if( exists $self->{INCLUDE_EXT} ){
        my $found = 0;

        (my $xx = $File::Find::name) =~ s,.*?/auto/,,;
        $xx =~ s,/?$_,,;
        $xx =~ s,/,::,g;

        # Throw away anything not explicitly marked for inclusion.
        # DynaLoader is implied.
        foreach my $incl ((@{$self->{INCLUDE_EXT}},'DynaLoader')){
            if( $xx eq $incl ){
                $found++;
                last;
            }
        }
        return unless $found;
    }
    elsif( exists $self->{EXCLUDE_EXT} ){
        (my $xx = $File::Find::name) =~ s,.*?/auto/,,;
        $xx =~ s,/?$_,,;
        $xx =~ s,/,::,g;

        # Throw away anything explicitly marked for exclusion
        foreach my $excl (@{$self->{EXCLUDE_EXT}}){
            return if( $xx eq $excl );
        }
    }

    $olbs{$ENV{DEFAULT}} = $_;
    }, grep( -d $_, @{$searchdirs || []}));

    # We trust that what has been handed in as argument will be buildable
    $static = [] unless $static;
    @olbs{@{$static}} = (1) x @{$static};

    $extra = [] unless $extra && ref $extra eq 'ARRAY';
    # Sort the object libraries in inverse order of
    # filespec length to try to insure that dependent extensions
    # will appear before their parents, so the linker will
    # search the parent library to resolve references.
    # (e.g. Intuit::DWIM will precede Intuit, so unresolved
    # references from [.intuit.dwim]dwim.obj can be found
    # in [.intuit]intuit.olb).
    for (sort { length($a) <=> length($b) || $a cmp $b } keys %olbs) {
    next unless $olbs{$_} =~ /\Q$self->{LIB_EXT}\E$/;
    my($dir) = $self->fixpath($_,1);
    my($extralibs) = $dir . "extralibs.ld";
    my($extopt) = $dir . $olbs{$_};
    $extopt =~ s/$self->{LIB_EXT}$/.opt/;
    push @optlibs, "$dir$olbs{$_}";
    # Get external libraries this extension will need
    if (-f $extralibs ) {
        my %seenthis;
        open my $list, "<", $extralibs or warn $!,next;
        while (<$list>) {
        chomp;
        # Include a library in the link only once, unless it's mentioned
        # multiple times within a single extension's options file, in which
        # case we assume the builder needed to search it again later in the
        # link.
        my $skip = exists($libseen{$_}) && !exists($seenthis{$_});
        $libseen{$_}++;  $seenthis{$_}++;
        next if $skip;
        push @$extra,$_;
        }
    }
    # Get full name of extension for ExtUtils::Miniperl
    if (-f $extopt) {
        open my $opt, '<', $extopt or die $!;
        while (<$opt>) {
        next unless /(?:UNIVERSAL|VECTOR)=boot_([\w_]+)/;
        my $pkg = $1;
        $pkg =~ s#__*#::#g;
        push @staticpkgs,$pkg;
        }
    }
    }
    # Place all of the external libraries after all of the Perl extension
    # libraries in the final link, in order to maximize the opportunity
    # for XS code from multiple extensions to resolve symbols against the
    # same external library while only including that library once.
    push @optlibs, @$extra;

    $target = "Perl$Config{'exe_ext'}" unless $target;
    my $shrtarget;
    ($shrtarget,$targdir) = fileparse($target);
    $shrtarget =~ s/^([^.]*)/$1Shr/;
    $shrtarget = $targdir . $shrtarget;
    $target = "Perlshr.$Config{'dlext'}" unless $target;
    $tmpdir = "[]" unless $tmpdir;
    $tmpdir = $self->fixpath($tmpdir,1);
    if (@optlibs) { $extralist = join(' ',@optlibs); }
    else          { $extralist = ''; }
    # Let ExtUtils::Liblist find the necessary libs for us (but skip PerlShr)
    # that's what we're building here).
    push @optlibs, grep { !/PerlShr/i } split ' ', +($self->ext())[2];
    if ($libperl) {
    unless (-f $libperl || -f ($libperl = $self->catfile($Config{'installarchlib'},'CORE',$libperl))) {
        print "Warning: $libperl not found\n";
        undef $libperl;
    }
    }
    unless ($libperl) {
    if (defined $self->{PERL_SRC}) {
        $libperl = $self->catfile($self->{PERL_SRC},"libperl$self->{LIB_EXT}");
    } elsif (-f ($libperl = $self->catfile($Config{'installarchlib'},'CORE',"libperl$self->{LIB_EXT}")) ) {
    } else {
        print "Warning: $libperl not found
    If you're going to build a static perl binary, make sure perl is installed
    otherwise ignore this warning\n";
    }
    }
    $libperldir = $self->fixpath((fileparse($libperl))[1],1);

    push @m, '
# Fill in the target you want to produce if it\'s not perl
MAP_TARGET    = ',$self->fixpath($target,0),'
MAP_SHRTARGET = ',$self->fixpath($shrtarget,0),"
MAP_LINKCMD   = $linkcmd
MAP_PERLINC   = ", $perlinc ? map('"$_" ',@{$perlinc}) : '',"
MAP_EXTRA     = $extralist
MAP_LIBPERL = ",$self->fixpath($libperl,0),'
';


    push @m,"\n${tmpdir}Makeaperl.Opt : \$(MAP_EXTRA)\n";
    foreach (@optlibs) {
    push @m,'    $(NOECHO) $(PERL) -e "print q{',$_,'}" >>$(MMS$TARGET)',"\n";
    }
    push @m,"\n${tmpdir}PerlShr.Opt :\n\t";
    push @m,'$(NOECHO) $(PERL) -e "print q{$(MAP_SHRTARGET)}" >$(MMS$TARGET)',"\n";

    push @m,'
$(MAP_SHRTARGET) : $(MAP_LIBPERL) Makeaperl.Opt ',"${libperldir}Perlshr_Attr.Opt",'
    $(MAP_LINKCMD)/Shareable=$(MMS$TARGET) $(MAP_LIBPERL), Makeaperl.Opt/Option ',"${libperldir}Perlshr_Attr.Opt/Option",'
$(MAP_TARGET) : $(MAP_SHRTARGET) ',"${tmpdir}perlmain\$(OBJ_EXT) ${tmpdir}PerlShr.Opt",'
    $(MAP_LINKCMD) ',"${tmpdir}perlmain\$(OBJ_EXT)",', PerlShr.Opt/Option
    $(NOECHO) $(ECHO) "To install the new ""$(MAP_TARGET)"" binary, say"
    $(NOECHO) $(ECHO) "    $(MAKE)$(USEMAKEFILE)$(FIRST_MAKEFILE) inst_perl $(USEMACROS)MAP_TARGET=$(MAP_TARGET)$(ENDMACRO)"
    $(NOECHO) $(ECHO) "To remove the intermediate files, say
    $(NOECHO) $(ECHO) "    $(MAKE)$(USEMAKEFILE)$(FIRST_MAKEFILE) map_clean"
';
    push @m,"\n${tmpdir}perlmain.c : \$(FIRST_MAKEFILE)\n\t\$(NOECHO) \$(PERL) -e 1 >${tmpdir}Writemain.tmp\n";
    push @m, "# More from the 255-char line length limit\n";
    foreach (@staticpkgs) {
    push @m,'    $(NOECHO) $(PERL) -e "print q{',$_,qq[}" >>${tmpdir}Writemain.tmp\n];
    }

    push @m, sprintf <<'MAKE_FRAG', $tmpdir, $tmpdir;
    $(NOECHO) $(PERL) $(MAP_PERLINC) -ane "use ExtUtils::Miniperl; writemain(@F)" %sWritemain.tmp >$(MMS$TARGET)
    $(NOECHO) $(RM_F) %sWritemain.tmp
MAKE_FRAG

    push @m, q[
# Still more from the 255-char line length limit
doc_inst_perl :
    $(NOECHO) $(MKPATH) $(DESTINSTALLARCHLIB)
    $(NOECHO) $(ECHO) "Perl binary $(MAP_TARGET)|" >.MM_tmp
    $(NOECHO) $(ECHO) "MAP_STATIC|$(MAP_STATIC)|" >>.MM_tmp
    $(NOECHO) $(PERL) -pl040 -e " " ].$self->catfile('$(INST_ARCHAUTODIR)','extralibs.all'),q[ >>.MM_tmp
    $(NOECHO) $(ECHO) -e "MAP_LIBPERL|$(MAP_LIBPERL)|" >>.MM_tmp
    $(NOECHO) $(DOC_INSTALL) <.MM_tmp >>].$self->catfile('$(DESTINSTALLARCHLIB)','perllocal.pod').q[
    $(NOECHO) $(RM_F) .MM_tmp
];

    push @m, "
inst_perl : pure_inst_perl doc_inst_perl
    \$(NOECHO) \$(NOOP)

pure_inst_perl : \$(MAP_TARGET)
    $self->{CP} \$(MAP_SHRTARGET) ",$self->fixpath($Config{'installbin'},1),"
    $self->{CP} \$(MAP_TARGET) ",$self->fixpath($Config{'installbin'},1),"

clean :: map_clean
    \$(NOECHO) \$(NOOP)

map_clean :
    \$(RM_F) ${tmpdir}perlmain\$(OBJ_EXT) ${tmpdir}perlmain.c \$(FIRST_MAKEFILE)
    \$(RM_F) ${tmpdir}Makeaperl.Opt ${tmpdir}PerlShr.Opt \$(MAP_TARGET)
";

    join '', @m;
}


# --- Output postprocessing section ---

=item maketext_filter (override)

Ensure that colons marking targets are preceded by space, in order
to distinguish the target delimiter from a colon appearing as
part of a filespec.

=cut

sub maketext_filter {
    my($self, $text) = @_;

    $text =~ s/^([^\s:=]+)(:+\s)/$1 $2/mg;
    return $text;
}

=item prefixify (override)

prefixifying on VMS is simple.  Each should simply be:

    perl_root:[some.dir]

which can just be converted to:

    volume:[your.prefix.some.dir]

otherwise you get the default layout.

In effect, your search prefix is ignored and $Config{vms_prefix} is
used instead.

=cut

sub prefixify {
    my($self, $var, $sprefix, $rprefix, $default) = @_;

    # Translate $(PERLPREFIX) to a real path.
    $rprefix = $self->eliminate_macros($rprefix);
    $rprefix = vmspath($rprefix) if $rprefix;
    $sprefix = vmspath($sprefix) if $sprefix;

    $default = vmsify($default)
      unless $default =~ /\[.*\]/;

    (my $var_no_install = $var) =~ s/^install//;
    my $path = $self->{uc $var} ||
               $ExtUtils::MM_Unix::Config_Override{lc $var} ||
               $Config{lc $var} || $Config{lc $var_no_install};

    if( !$path ) {
        warn "  no Config found for $var.\n" if $Verbose >= 2;
        $path = $self->_prefixify_default($rprefix, $default);
    }
    elsif( !$self->{ARGS}{PREFIX} || !$self->file_name_is_absolute($path) ) {
        # do nothing if there's no prefix or if its relative
    }
    elsif( $sprefix eq $rprefix ) {
        warn "  no new prefix.\n" if $Verbose >= 2;
    }
    else {

        warn "  prefixify $var => $path\n"     if $Verbose >= 2;
        warn "    from $sprefix to $rprefix\n" if $Verbose >= 2;

        my($path_vol, $path_dirs) = $self->splitpath( $path );
        if( $path_vol eq $Config{vms_prefix}.':' ) {
            warn "  $Config{vms_prefix}: seen\n" if $Verbose >= 2;

            $path_dirs =~ s{^\[}{\[.} unless $path_dirs =~ m{^\[\.};
            $path = $self->_catprefix($rprefix, $path_dirs);
        }
        else {
            $path = $self->_prefixify_default($rprefix, $default);
        }
    }

    print "    now $path\n" if $Verbose >= 2;
    return $self->{uc $var} = $path;
}


sub _prefixify_default {
    my($self, $rprefix, $default) = @_;

    warn "  cannot prefix, using default.\n" if $Verbose >= 2;

    if( !$default ) {
        warn "No default!\n" if $Verbose >= 1;
        return;
    }
    if( !$rprefix ) {
        warn "No replacement prefix!\n" if $Verbose >= 1;
        return '';
    }

    return $self->_catprefix($rprefix, $default);
}

sub _catprefix {
    my($self, $rprefix, $default) = @_;

    my($rvol, $rdirs) = $self->splitpath($rprefix);
    if( $rvol ) {
        return $self->catpath($rvol,
                                   $self->catdir($rdirs, $default),
                                   ''
                                  )
    }
    else {
        return $self->catdir($rdirs, $default);
    }
}


=item cd

=cut

sub cd {
    my($self, $dir, @cmds) = @_;

    $dir = vmspath($dir);

    my $cmd = join "\n\t", map "$_", @cmds;

    # No leading tab makes it look right when embedded
    my $make_frag = sprintf <<'MAKE_FRAG', $dir, $cmd;
startdir = F$Environment("Default")
    Set Default %s
    %s
    Set Default 'startdir'
MAKE_FRAG

    # No trailing newline makes this easier to embed
    chomp $make_frag;

    return $make_frag;
}


=item oneliner

=cut

sub oneliner {
    my($self, $cmd, $switches) = @_;
    $switches = [] unless defined $switches;

    # Strip leading and trailing newlines
    $cmd =~ s{^\n+}{};
    $cmd =~ s{\n+$}{};

    my @cmds = split /\n/, $cmd;
    $cmd = join " \n\t  -e ", map $self->quote_literal($_), @cmds;
    $cmd = $self->escape_newlines($cmd);

    # Switches must be quoted else they will be lowercased.
    $switches = join ' ', map { qq{"$_"} } @$switches;

    return qq{\$(ABSPERLRUN) $switches -e $cmd "--"};
}


=item B<echo>

perl trips up on "<foo>" thinking it's an input redirect.  So we use the
native Write command instead.  Besides, it's faster.

=cut

sub echo {
    my($self, $text, $file, $opts) = @_;

    # Compatibility with old options
    if( !ref $opts ) {
        my $append = $opts;
        $opts = { append => $append || 0 };
    }
    my $opencmd = $opts->{append} ? 'Open/Append' : 'Open/Write';

    $opts->{allow_variables} = 0 unless defined $opts->{allow_variables};

    my $ql_opts = { allow_variables => $opts->{allow_variables} };

    my @cmds = ("\$(NOECHO) $opencmd MMECHOFILE $file ");
    push @cmds, map { '$(NOECHO) Write MMECHOFILE '.$self->quote_literal($_, $ql_opts) }
                split /\n/, $text;
    push @cmds, '$(NOECHO) Close MMECHOFILE';
    return @cmds;
}


=item quote_literal

=cut

sub quote_literal {
    my($self, $text, $opts) = @_;
    $opts->{allow_variables} = 1 unless defined $opts->{allow_variables};

    # I believe this is all we should need.
    $text =~ s{"}{""}g;

    $text = $opts->{allow_variables}
      ? $self->escape_dollarsigns($text) : $self->escape_all_dollarsigns($text);

    return qq{"$text"};
}

=item escape_dollarsigns

Quote, don't escape.

=cut

sub escape_dollarsigns {
    my($self, $text) = @_;

    # Quote dollar signs which are not starting a variable
    $text =~ s{\$ (?!\() }{"\$"}gx;

    return $text;
}


=item escape_all_dollarsigns

Quote, don't escape.

=cut

sub escape_all_dollarsigns {
    my($self, $text) = @_;

    # Quote dollar signs
    $text =~ s{\$}{"\$\"}gx;

    return $text;
}

=item escape_newlines

=cut

sub escape_newlines {
    my($self, $text) = @_;

    $text =~ s{\n}{-\n}g;

    return $text;
}

=item max_exec_len

256 characters.

=cut

sub max_exec_len {
    my $self = shift;

    return $self->{_MAX_EXEC_LEN} ||= 256;
}

=item init_linker

=cut

sub init_linker {
    my $self = shift;
    $self->{EXPORT_LIST} ||= '$(BASEEXT).opt';

    my $shr = $Config{dbgprefix} . 'PERLSHR';
    if ($self->{PERL_SRC}) {
        $self->{PERL_ARCHIVE} ||=
          $self->catfile($self->{PERL_SRC}, "$shr.$Config{'dlext'}");
    }
    else {
        $self->{PERL_ARCHIVE} ||=
          $ENV{$shr} ? $ENV{$shr} : "Sys\$Share:$shr.$Config{'dlext'}";
    }

    $self->{PERL_ARCHIVEDEP} ||= '';
    $self->{PERL_ARCHIVE_AFTER} ||= '';
}


=item catdir (override)

=item catfile (override)

Eliminate the macros in the output to the MMS/MMK file.

(File::Spec::VMS used to do this for us, but it's being removed)

=cut

sub catdir {
    my $self = shift;

    # Process the macros on VMS MMS/MMK
    my @args = map { m{\$\(} ? $self->eliminate_macros($_) : $_  } @_;

    my $dir = $self->SUPER::catdir(@args);

    # Fix up the directory and force it to VMS format.
    $dir = $self->fixpath($dir, 1);

    return $dir;
}

sub catfile {
    my $self = shift;

    # Process the macros on VMS MMS/MMK
    my @args = map { m{\$\(} ? $self->eliminate_macros($_) : $_  } @_;

    my $file = $self->SUPER::catfile(@args);

    $file = vmsify($file);

    return $file
}


=item eliminate_macros

Expands MM[KS]/Make macros in a text string, using the contents of
identically named elements of C<%$self>, and returns the result
as a file specification in Unix syntax.

NOTE:  This is the canonical version of the method.  The version in
File::Spec::VMS is deprecated.

=cut

sub eliminate_macros {
    my($self,$path) = @_;
    return '' unless $path;
    $self = {} unless ref $self;

    my($npath) = unixify($path);
    # sometimes unixify will return a string with an off-by-one trailing null
    $npath =~ s{\0$}{};

    my($complex) = 0;
    my($head,$macro,$tail);

    # perform m##g in scalar context so it acts as an iterator
    while ($npath =~ m#(.*?)\$\((\S+?)\)(.*)#gs) {
        if (defined $self->{$2}) {
            ($head,$macro,$tail) = ($1,$2,$3);
            if (ref $self->{$macro}) {
                if (ref $self->{$macro} eq 'ARRAY') {
                    $macro = join ' ', @{$self->{$macro}};
                }
                else {
                    print "Note: can't expand macro \$($macro) containing ",ref($self->{$macro}),
                          "\n\t(using MMK-specific deferred substitutuon; MMS will break)\n";
                    $macro = "\cB$macro\cB";
                    $complex = 1;
                }
            }
            else {
                $macro = $self->{$macro};
                # Don't unixify if there is unescaped whitespace
                $macro = unixify($macro) unless ($macro =~ /(?<!\^)\s/);
                $macro =~ s#/\Z(?!\n)##;
            }
            $npath = "$head$macro$tail";
        }
    }
    if ($complex) { $npath =~ s#\cB(.*?)\cB#\${$1}#gs; }
    $npath;
}

=item fixpath

   my $path = $mm->fixpath($path);
   my $path = $mm->fixpath($path, $is_dir);

Catchall routine to clean up problem MM[SK]/Make macros.  Expands macros
in any directory specification, in order to avoid juxtaposing two
VMS-syntax directories when MM[SK] is run.  Also expands expressions which
are all macro, so that we can tell how long the expansion is, and avoid
overrunning DCL's command buffer when MM[KS] is running.

fixpath() checks to see whether the result matches the name of a
directory in the current default directory and returns a directory or
file specification accordingly.  C<$is_dir> can be set to true to
force fixpath() to consider the path to be a directory or false to force
it to be a file.

NOTE:  This is the canonical version of the method.  The version in
File::Spec::VMS is deprecated.

=cut

sub fixpath {
    my($self,$path,$force_path) = @_;
    return '' unless $path;
    $self = bless {}, $self unless ref $self;
    my($fixedpath,$prefix,$name);

    if ($path =~ m#^\$\([^\)]+\)\Z(?!\n)#s || $path =~ m#[/:>\]]#) {
        if ($force_path or $path =~ /(?:DIR\)|\])\Z(?!\n)/) {
            $fixedpath = vmspath($self->eliminate_macros($path));
        }
        else {
            $fixedpath = vmsify($self->eliminate_macros($path));
        }
    }
    elsif ((($prefix,$name) = ($path =~ m#^\$\(([^\)]+)\)(.+)#s)) && $self->{$prefix}) {
        my($vmspre) = $self->eliminate_macros("\$($prefix)");
        # is it a dir or just a name?
        $vmspre = ($vmspre =~ m|/| or $prefix =~ /DIR\Z(?!\n)/) ? vmspath($vmspre) : '';
        $fixedpath = ($vmspre ? $vmspre : $self->{$prefix}) . $name;
        $fixedpath = vmspath($fixedpath) if $force_path;
    }
    else {
        $fixedpath = $path;
        $fixedpath = vmspath($fixedpath) if $force_path;
    }
    # No hints, so we try to guess
    if (!defined($force_path) and $fixedpath !~ /[:>(.\]]/) {
        $fixedpath = vmspath($fixedpath) if -d $fixedpath;
    }

    # Trim off root dirname if it's had other dirs inserted in front of it.
    $fixedpath =~ s/\.000000([\]>])/$1/;
    # Special case for VMS absolute directory specs: these will have had device
    # prepended during trip through Unix syntax in eliminate_macros(), since
    # Unix syntax has no way to express "absolute from the top of this device's
    # directory tree".
    if ($path =~ /^[\[>][^.\-]/) { $fixedpath =~ s/^[^\[<]+//; }

    return $fixedpath;
}


=item os_flavor

VMS is VMS.

=cut

sub os_flavor {
    return('VMS');
}


=item is_make_type (override)

None of the make types being checked for is viable on VMS,
plus our $self->{MAKE} is an unexpanded (and unexpandable)
macro whose value is known only to the make utility itself.

=cut

sub is_make_type {
    my($self, $type) = @_;
    return 0;
}


=item make_type (override)

Returns a suitable string describing the type of makefile being written.

=cut

sub make_type { "$Config{make}-style"; }


=back


=head1 AUTHOR

Original author Charles Bailey F<bailey@newman.upenn.edu>

Maintained by Michael G Schwern F<schwern@pobox.com>

See L<ExtUtils::MakeMaker> for patching and contact information.


=cut

1;

