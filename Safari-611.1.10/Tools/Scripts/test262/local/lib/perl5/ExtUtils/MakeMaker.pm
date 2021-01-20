# $Id$
package ExtUtils::MakeMaker;

use strict;

BEGIN {require 5.006;}

require Exporter;
use ExtUtils::MakeMaker::Config;
use ExtUtils::MakeMaker::version; # ensure we always have our fake version.pm
use Carp;
use File::Path;
my $CAN_DECODE = eval { require ExtUtils::MakeMaker::Locale; }; # 2 birds, 1 stone
eval { ExtUtils::MakeMaker::Locale::reinit('UTF-8') }
  if $CAN_DECODE and Encode::find_encoding('locale')->name eq 'ascii';

our $Verbose = 0;       # exported
our @Parent;            # needs to be localized
our @Get_from_Config;   # referenced by MM_Unix
our @MM_Sections;
our @Overridable;
my @Prepend_parent;
my %Recognized_Att_Keys;
our %macro_fsentity; # whether a macro is a filesystem name
our %macro_dep; # whether a macro is a dependency

our $VERSION = '7.32';
$VERSION = eval $VERSION;  ## no critic [BuiltinFunctions::ProhibitStringyEval]

# Emulate something resembling CVS $Revision$
(our $Revision = $VERSION) =~ s{_}{};
$Revision = int $Revision * 10000;

our $Filename = __FILE__;   # referenced outside MakeMaker

our @ISA = qw(Exporter);
our @EXPORT    = qw(&WriteMakefile $Verbose &prompt &os_unsupported);
our @EXPORT_OK = qw($VERSION &neatvalue &mkbootstrap &mksymlists
                    &WriteEmptyMakefile &open_for_writing &write_file_via_tmp
                    &_sprintf562);

# These will go away once the last of the Win32 & VMS specific code is
# purged.
my $Is_VMS     = $^O eq 'VMS';
my $Is_Win32   = $^O eq 'MSWin32';
our $UNDER_CORE = $ENV{PERL_CORE}; # needs to be our

full_setup();

require ExtUtils::MM;  # Things like CPAN assume loading ExtUtils::MakeMaker
                       # will give them MM.

require ExtUtils::MY;  # XXX pre-5.8 versions of ExtUtils::Embed expect
                       # loading ExtUtils::MakeMaker will give them MY.
                       # This will go when Embed is its own CPAN module.


# 5.6.2 can't do sprintf "%1$s" - this can only do %s
sub _sprintf562 {
    my ($format, @args) = @_;
    for (my $i = 1; $i <= @args; $i++) {
        $format =~ s#%$i\$s#$args[$i-1]#g;
    }
    $format;
}

sub WriteMakefile {
    croak "WriteMakefile: Need even number of args" if @_ % 2;

    require ExtUtils::MY;
    my %att = @_;

    _convert_compat_attrs(\%att);

    _verify_att(\%att);

    my $mm = MM->new(\%att);
    $mm->flush;

    return $mm;
}


# Basic signatures of the attributes WriteMakefile takes.  Each is the
# reference type.  Empty value indicate it takes a non-reference
# scalar.
my %Att_Sigs;
my %Special_Sigs = (
 AUTHOR             => 'ARRAY',
 C                  => 'ARRAY',
 CONFIG             => 'ARRAY',
 CONFIGURE          => 'CODE',
 DIR                => 'ARRAY',
 DL_FUNCS           => 'HASH',
 DL_VARS            => 'ARRAY',
 EXCLUDE_EXT        => 'ARRAY',
 EXE_FILES          => 'ARRAY',
 FUNCLIST           => 'ARRAY',
 H                  => 'ARRAY',
 IMPORTS            => 'HASH',
 INCLUDE_EXT        => 'ARRAY',
 LIBS               => ['ARRAY',''],
 MAN1PODS           => 'HASH',
 MAN3PODS           => 'HASH',
 META_ADD           => 'HASH',
 META_MERGE         => 'HASH',
 OBJECT             => ['ARRAY', ''],
 PL_FILES           => 'HASH',
 PM                 => 'HASH',
 PMLIBDIRS          => 'ARRAY',
 PMLIBPARENTDIRS    => 'ARRAY',
 PREREQ_PM          => 'HASH',
 BUILD_REQUIRES     => 'HASH',
 CONFIGURE_REQUIRES => 'HASH',
 TEST_REQUIRES      => 'HASH',
 SKIP               => 'ARRAY',
 TYPEMAPS           => 'ARRAY',
 XS                 => 'HASH',
 XSBUILD            => 'HASH',
 VERSION            => ['version',''],
 _KEEP_AFTER_FLUSH  => '',

 clean      => 'HASH',
 depend     => 'HASH',
 dist       => 'HASH',
 dynamic_lib=> 'HASH',
 linkext    => 'HASH',
 macro      => 'HASH',
 postamble  => 'HASH',
 realclean  => 'HASH',
 test       => 'HASH',
 tool_autosplit => 'HASH',
);

@Att_Sigs{keys %Recognized_Att_Keys} = ('') x keys %Recognized_Att_Keys;
@Att_Sigs{keys %Special_Sigs} = values %Special_Sigs;

sub _convert_compat_attrs { #result of running several times should be same
    my($att) = @_;
    if (exists $att->{AUTHOR}) {
        if ($att->{AUTHOR}) {
            if (!ref($att->{AUTHOR})) {
                my $t = $att->{AUTHOR};
                $att->{AUTHOR} = [$t];
            }
        } else {
                $att->{AUTHOR} = [];
        }
    }
}

sub _verify_att {
    my($att) = @_;

    foreach my $key (sort keys %$att) {
        my $val = $att->{$key};
        my $sig = $Att_Sigs{$key};
        unless( defined $sig ) {
            warn "WARNING: $key is not a known parameter.\n";
            next;
        }

        my @sigs   = ref $sig ? @$sig : $sig;
        my $given  = ref $val;
        unless( grep { _is_of_type($val, $_) } @sigs ) {
            my $takes = join " or ", map { _format_att($_) } @sigs;

            my $has = _format_att($given);
            warn "WARNING: $key takes a $takes not a $has.\n".
                 "         Please inform the author.\n";
        }
    }
}


# Check if a given thing is a reference or instance of $type
sub _is_of_type {
    my($thing, $type) = @_;

    return 1 if ref $thing eq $type;

    local $SIG{__DIE__};
    return 1 if eval{ $thing->isa($type) };

    return 0;
}


sub _format_att {
    my $given = shift;

    return $given eq ''        ? "string/number"
         : uc $given eq $given ? "$given reference"
         :                       "$given object"
         ;
}


sub prompt ($;$) {  ## no critic
    my($mess, $def) = @_;
    confess("prompt function called without an argument")
        unless defined $mess;

    my $isa_tty = -t STDIN && (-t STDOUT || !(-f STDOUT || -c STDOUT)) ;

    my $dispdef = defined $def ? "[$def] " : " ";
    $def = defined $def ? $def : "";

    local $|=1;
    local $\;
    print "$mess $dispdef";

    my $ans;
    if ($ENV{PERL_MM_USE_DEFAULT} || (!$isa_tty && eof STDIN)) {
        print "$def\n";
    }
    else {
        $ans = <STDIN>;
        if( defined $ans ) {
            $ans =~ s{\015?\012$}{};
        }
        else { # user hit ctrl-D
            print "\n";
        }
    }

    return (!defined $ans || $ans eq '') ? $def : $ans;
}

sub os_unsupported {
    die "OS unsupported\n";
}

sub eval_in_subdirs {
    my($self) = @_;
    use Cwd qw(cwd abs_path);
    my $pwd = cwd() || die "Can't figure out your cwd!";

    local @INC = map eval {abs_path($_) if -e} || $_, @INC;
    push @INC, '.';     # '.' has to always be at the end of @INC

    foreach my $dir (@{$self->{DIR}}){
        my($abs) = $self->catdir($pwd,$dir);
        eval { $self->eval_in_x($abs); };
        last if $@;
    }
    chdir $pwd;
    die $@ if $@;
}

sub eval_in_x {
    my($self,$dir) = @_;
    chdir $dir or carp("Couldn't change to directory $dir: $!");

    {
        package main;
        do './Makefile.PL';
    };
    if ($@) {
#         if ($@ =~ /prerequisites/) {
#             die "MakeMaker WARNING: $@";
#         } else {
#             warn "WARNING from evaluation of $dir/Makefile.PL: $@";
#         }
        die "ERROR from evaluation of $dir/Makefile.PL: $@";
    }
}


# package name for the classes into which the first object will be blessed
my $PACKNAME = 'PACK000';

sub full_setup {
    $Verbose ||= 0;

    my @dep_macros = qw/
    PERL_INCDEP        PERL_ARCHLIBDEP     PERL_ARCHIVEDEP
    /;

    my @fs_macros = qw/
    FULLPERL XSUBPPDIR

    INST_ARCHLIB INST_SCRIPT INST_BIN INST_LIB INST_MAN1DIR INST_MAN3DIR
    INSTALLDIRS
    DESTDIR PREFIX INSTALL_BASE
    PERLPREFIX      SITEPREFIX      VENDORPREFIX
    INSTALLPRIVLIB  INSTALLSITELIB  INSTALLVENDORLIB
    INSTALLARCHLIB  INSTALLSITEARCH INSTALLVENDORARCH
    INSTALLBIN      INSTALLSITEBIN  INSTALLVENDORBIN
    INSTALLMAN1DIR          INSTALLMAN3DIR
    INSTALLSITEMAN1DIR      INSTALLSITEMAN3DIR
    INSTALLVENDORMAN1DIR    INSTALLVENDORMAN3DIR
    INSTALLSCRIPT   INSTALLSITESCRIPT  INSTALLVENDORSCRIPT
    PERL_LIB        PERL_ARCHLIB
    SITELIBEXP      SITEARCHEXP

    MAKE LIBPERL_A LIB PERL_SRC PERL_INC
    PPM_INSTALL_EXEC PPM_UNINSTALL_EXEC
    PPM_INSTALL_SCRIPT PPM_UNINSTALL_SCRIPT
    /;

    my @attrib_help = qw/

    AUTHOR ABSTRACT ABSTRACT_FROM BINARY_LOCATION
    C CAPI CCFLAGS CONFIG CONFIGURE DEFINE DIR DISTNAME DISTVNAME
    DL_FUNCS DL_VARS
    EXCLUDE_EXT EXE_FILES FIRST_MAKEFILE
    FULLPERLRUN FULLPERLRUNINST
    FUNCLIST H IMPORTS

    INC INCLUDE_EXT LDFROM LIBS LICENSE
    LINKTYPE MAKEAPERL MAKEFILE MAKEFILE_OLD MAN1PODS MAN3PODS MAP_TARGET
    META_ADD META_MERGE MIN_PERL_VERSION BUILD_REQUIRES CONFIGURE_REQUIRES
    MYEXTLIB NAME NEEDS_LINKING NOECHO NO_META NO_MYMETA NO_PACKLIST NO_PERLLOCAL
    NORECURS NO_VC OBJECT OPTIMIZE PERL_MALLOC_OK PERL PERLMAINCC PERLRUN
    PERLRUNINST PERL_CORE
    PERM_DIR PERM_RW PERM_RWX MAGICXS
    PL_FILES PM PM_FILTER PMLIBDIRS PMLIBPARENTDIRS POLLUTE
    PREREQ_FATAL PREREQ_PM PREREQ_PRINT PRINT_PREREQ
    SIGN SKIP TEST_REQUIRES TYPEMAPS UNINST VERSION VERSION_FROM XS
    XSBUILD XSMULTI XSOPT XSPROTOARG XS_VERSION
    clean depend dist dynamic_lib linkext macro realclean tool_autosplit

    MAN1EXT MAN3EXT

    MACPERL_SRC MACPERL_LIB MACLIBS_68K MACLIBS_PPC MACLIBS_SC MACLIBS_MRC
    MACLIBS_ALL_68K MACLIBS_ALL_PPC MACLIBS_SHARED
        /;
    push @attrib_help, @fs_macros;
    @macro_fsentity{@fs_macros, @dep_macros} = (1) x (@fs_macros+@dep_macros);
    @macro_dep{@dep_macros} = (1) x @dep_macros;

    # IMPORTS is used under OS/2 and Win32

    # @Overridable is close to @MM_Sections but not identical.  The
    # order is important. Many subroutines declare macros. These
    # depend on each other. Let's try to collect the macros up front,
    # then pasthru, then the rules.

    # MM_Sections are the sections we have to call explicitly
    # in Overridable we have subroutines that are used indirectly


    @MM_Sections =
        qw(

 post_initialize const_config constants platform_constants
 tool_autosplit tool_xsubpp tools_other

 makemakerdflt

 dist macro depend cflags const_loadlibs const_cccmd
 post_constants

 pasthru

 special_targets
 c_o xs_c xs_o
 top_targets blibdirs linkext dlsyms dynamic_bs dynamic
 dynamic_lib static static_lib manifypods processPL
 installbin subdirs
 clean_subdirs clean realclean_subdirs realclean
 metafile signature
 dist_basics dist_core distdir dist_test dist_ci distmeta distsignature
 install force perldepend makefile staticmake test ppd

          ); # loses section ordering

    @Overridable = @MM_Sections;
    push @Overridable, qw[

 libscan makeaperl needs_linking
 subdir_x test_via_harness test_via_script

 init_VERSION init_dist init_INST init_INSTALL init_DEST init_dirscan
 init_PM init_MANPODS init_xs init_PERL init_DIRFILESEP init_linker
                         ];

    push @MM_Sections, qw[

 pm_to_blib selfdocument

                         ];

    # Postamble needs to be the last that was always the case
    push @MM_Sections, "postamble";
    push @Overridable, "postamble";

    # All sections are valid keys.
    @Recognized_Att_Keys{@MM_Sections} = (1) x @MM_Sections;

    # we will use all these variables in the Makefile
    @Get_from_Config =
        qw(
           ar cc cccdlflags ccdlflags dlext dlsrc exe_ext full_ar ld
           lddlflags ldflags libc lib_ext obj_ext osname osvers ranlib
           sitelibexp sitearchexp so
          );

    # 5.5.3 doesn't have any concept of vendor libs
    push @Get_from_Config, qw( vendorarchexp vendorlibexp ) if $] >= 5.006;

    foreach my $item (@attrib_help){
        $Recognized_Att_Keys{$item} = 1;
    }
    foreach my $item (@Get_from_Config) {
        $Recognized_Att_Keys{uc $item} = $Config{$item};
        print "Attribute '\U$item\E' => '$Config{$item}'\n"
            if ($Verbose >= 2);
    }

    #
    # When we eval a Makefile.PL in a subdirectory, that one will ask
    # us (the parent) for the values and will prepend "..", so that
    # all files to be installed end up below OUR ./blib
    #
    @Prepend_parent = qw(
           INST_BIN INST_LIB INST_ARCHLIB INST_SCRIPT
           MAP_TARGET INST_MAN1DIR INST_MAN3DIR PERL_SRC
           PERL FULLPERL
    );
}

sub _has_cpan_meta_requirements {
    return eval {
      require CPAN::Meta::Requirements;
      CPAN::Meta::Requirements->VERSION(2.130);
      require B; # CMR requires this, for core we have to too.
    };
}

sub new {
    my($class,$self) = @_;
    my($key);

    _convert_compat_attrs($self) if defined $self && $self;

    # Store the original args passed to WriteMakefile()
    foreach my $k (keys %$self) {
        $self->{ARGS}{$k} = $self->{$k};
    }

    $self = {} unless defined $self;

    # Temporarily bless it into MM so it can be used as an
    # object.  It will be blessed into a temp package later.
    bless $self, "MM";

    # Cleanup all the module requirement bits
    my %key2cmr;
    for my $key (qw(PREREQ_PM BUILD_REQUIRES CONFIGURE_REQUIRES TEST_REQUIRES)) {
        $self->{$key}      ||= {};
        if (_has_cpan_meta_requirements) {
            my $cmr = CPAN::Meta::Requirements->from_string_hash(
                $self->{$key},
                {
                  bad_version_hook => sub {
                    #no warnings 'numeric'; # module doesn't use warnings
                    my $fallback;
                    if ( $_[0] =~ m!^[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?$! ) {
                      $fallback = sprintf "%f", $_[0];
                    } else {
                      ($fallback) = $_[0] ? ($_[0] =~ /^([0-9.]+)/) : 0;
                      $fallback += 0;
                      carp "Unparsable version '$_[0]' for prerequisite $_[1] treated as $fallback";
                    }
                    version->new($fallback);
                  },
                },
            );
            $self->{$key} = $cmr->as_string_hash;
            $key2cmr{$key} = $cmr;
        } else {
            for my $module (sort keys %{ $self->{$key} }) {
                my $version = $self->{$key}->{$module};
                my $fallback = 0;
                if (!defined($version) or !length($version)) {
                    carp "Undefined requirement for $module treated as '0' (CPAN::Meta::Requirements not available)";
                }
                elsif ($version =~ /^\d+(?:\.\d+(?:_\d+)*)?$/) {
                    next;
                }
                else {
                    if ( $version =~ m!^[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?$! ) {
                      $fallback = sprintf "%f", $version;
                    } else {
                      ($fallback) = $version ? ($version =~ /^([0-9.]+)/) : 0;
                      $fallback += 0;
                      carp "Unparsable version '$version' for prerequisite $module treated as $fallback (CPAN::Meta::Requirements not available)";
                    }
                }
                $self->{$key}->{$module} = $fallback;
            }
        }
    }

    if ("@ARGV" =~ /\bPREREQ_PRINT\b/) {
        $self->_PREREQ_PRINT;
    }

    # PRINT_PREREQ is RedHatism.
    if ("@ARGV" =~ /\bPRINT_PREREQ\b/) {
        $self->_PRINT_PREREQ;
   }

    print "MakeMaker (v$VERSION)\n" if $Verbose;
    if (-f "MANIFEST" && ! -f "Makefile" && ! $UNDER_CORE){
        check_manifest();
    }

    check_hints($self);

    if ( defined $self->{MIN_PERL_VERSION}
          && $self->{MIN_PERL_VERSION} !~ /^v?[\d_\.]+$/ ) {
      require version;
      my $normal = eval {
        local $SIG{__WARN__} = sub {
            # simulate "use warnings FATAL => 'all'" for vintage perls
            die @_;
        };
        version->new( $self->{MIN_PERL_VERSION} )
      };
      $self->{MIN_PERL_VERSION} = $normal if defined $normal && !$@;
    }

    # Translate X.Y.Z to X.00Y00Z
    if( defined $self->{MIN_PERL_VERSION} ) {
        $self->{MIN_PERL_VERSION} =~ s{ ^v? (\d+) \. (\d+) \. (\d+) $ }
                                      {sprintf "%d.%03d%03d", $1, $2, $3}ex;
    }

    my $perl_version_ok = eval {
        local $SIG{__WARN__} = sub {
            # simulate "use warnings FATAL => 'all'" for vintage perls
            die @_;
        };
        !$self->{MIN_PERL_VERSION} or $self->{MIN_PERL_VERSION} <= $]
    };
    if (!$perl_version_ok) {
        if (!defined $perl_version_ok) {
            die <<'END';
Warning: MIN_PERL_VERSION is not in a recognized format.
Recommended is a quoted numerical value like '5.005' or '5.008001'.
END
        }
        elsif ($self->{PREREQ_FATAL}) {
            die sprintf <<"END", $self->{MIN_PERL_VERSION}, $];
MakeMaker FATAL: perl version too low for this distribution.
Required is %s. We run %s.
END
        }
        else {
            warn sprintf
                "Warning: Perl version %s or higher required. We run %s.\n",
                $self->{MIN_PERL_VERSION}, $];
        }
    }

    my %configure_att;         # record &{$self->{CONFIGURE}} attributes
    my(%initial_att) = %$self; # record initial attributes

    my(%unsatisfied) = ();
    my %prereq2version;
    my $cmr;
    if (_has_cpan_meta_requirements) {
        $cmr = CPAN::Meta::Requirements->new;
        for my $key (qw(PREREQ_PM BUILD_REQUIRES CONFIGURE_REQUIRES TEST_REQUIRES)) {
            $cmr->add_requirements($key2cmr{$key}) if $key2cmr{$key};
        }
        foreach my $prereq ($cmr->required_modules) {
            $prereq2version{$prereq} = $cmr->requirements_for_module($prereq);
        }
    } else {
        for my $key (qw(PREREQ_PM BUILD_REQUIRES CONFIGURE_REQUIRES TEST_REQUIRES)) {
            next unless my $module2version = $self->{$key};
            $prereq2version{$_} = $module2version->{$_} for keys %$module2version;
        }
    }
    foreach my $prereq (sort keys %prereq2version) {
        my $required_version = $prereq2version{$prereq};

        my $pr_version = 0;
        my $installed_file;

        if ( $prereq eq 'perl' ) {
          if ( defined $required_version && $required_version =~ /^v?[\d_\.]+$/
               || $required_version !~ /^v?[\d_\.]+$/ ) {
            require version;
            my $normal = eval { version->new( $required_version ) };
            $required_version = $normal if defined $normal;
          }
          $installed_file = $prereq;
          $pr_version = $];
        }
        else {
          $installed_file = MM->_installed_file_for_module($prereq);
          $pr_version = MM->parse_version($installed_file) if $installed_file;
          $pr_version = 0 if $pr_version eq 'undef';
          if ( !eval { version->new( $pr_version ); 1 } ) {
            #no warnings 'numeric'; # module doesn't use warnings
            my $fallback;
            if ( $pr_version =~ m!^[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?$! ) {
              $fallback = sprintf '%f', $pr_version;
            } else {
              ($fallback) = $pr_version ? ($pr_version =~ /^([0-9.]+)/) : 0;
              $fallback += 0;
              carp "Unparsable version '$pr_version' for installed prerequisite $prereq treated as $fallback";
            }
            $pr_version = $fallback;
          }
        }

        # convert X.Y_Z alpha version #s to X.YZ for easier comparisons
        $pr_version =~ s/(\d+)\.(\d+)_(\d+)/$1.$2$3/;

        if (!$installed_file) {
            warn sprintf "Warning: prerequisite %s %s not found.\n",
              $prereq, $required_version
                   unless $self->{PREREQ_FATAL}
                       or $UNDER_CORE;

            $unsatisfied{$prereq} = 'not installed';
        }
        elsif (
            $cmr
                ? !$cmr->accepts_module($prereq, $pr_version)
                : $required_version > $pr_version
        ) {
            warn sprintf "Warning: prerequisite %s %s not found. We have %s.\n",
              $prereq, $required_version, ($pr_version || 'unknown version')
                  unless $self->{PREREQ_FATAL}
                       or $UNDER_CORE;

            $unsatisfied{$prereq} = $required_version || 'unknown version' ;
        }
    }

    if (%unsatisfied && $self->{PREREQ_FATAL}){
        my $failedprereqs = join "\n", map {"    $_ $unsatisfied{$_}"}
                            sort { $a cmp $b } keys %unsatisfied;
        die <<"END";
MakeMaker FATAL: prerequisites not found.
$failedprereqs

Please install these modules first and rerun 'perl Makefile.PL'.
END
    }

    if (defined $self->{CONFIGURE}) {
        if (ref $self->{CONFIGURE} eq 'CODE') {
            %configure_att = %{&{$self->{CONFIGURE}}};
            _convert_compat_attrs(\%configure_att);
            $self = { %$self, %configure_att };
        } else {
            croak "Attribute 'CONFIGURE' to WriteMakefile() not a code reference\n";
        }
    }

    my $newclass = ++$PACKNAME;
    local @Parent = @Parent;    # Protect against non-local exits
    {
        print "Blessing Object into class [$newclass]\n" if $Verbose>=2;
        mv_all_methods("MY",$newclass);
        bless $self, $newclass;
        push @Parent, $self;
        require ExtUtils::MY;

        no strict 'refs';   ## no critic;
        @{"$newclass\:\:ISA"} = 'MM';
    }

    if (defined $Parent[-2]){
        $self->{PARENT} = $Parent[-2];
        for my $key (@Prepend_parent) {
            next unless defined $self->{PARENT}{$key};

            # Don't stomp on WriteMakefile() args.
            next if defined $self->{ARGS}{$key} and
                    $self->{ARGS}{$key} eq $self->{$key};

            $self->{$key} = $self->{PARENT}{$key};

            if ($Is_VMS && $key =~ /PERL$/) {
                # PERL or FULLPERL will be a command verb or even a
                # command with an argument instead of a full file
                # specification under VMS.  So, don't turn the command
                # into a filespec, but do add a level to the path of
                # the argument if not already absolute.
                my @cmd = split /\s+/, $self->{$key};
                $cmd[1] = $self->catfile('[-]',$cmd[1])
                  unless (@cmd < 2) || $self->file_name_is_absolute($cmd[1]);
                $self->{$key} = join(' ', @cmd);
            } else {
                my $value = $self->{$key};
                # not going to test in FS so only stripping start
                $value =~ s/^"// if $key =~ /PERL$/;
                $value = $self->catdir("..", $value)
                  unless $self->file_name_is_absolute($value);
                $value = qq{"$value} if $key =~ /PERL$/;
                $self->{$key} = $value;
            }
        }
        if ($self->{PARENT}) {
            $self->{PARENT}->{CHILDREN}->{$newclass} = $self;
            foreach my $opt (qw(POLLUTE PERL_CORE LINKTYPE LD OPTIMIZE)) {
                if (exists $self->{PARENT}->{$opt}
                    and not exists $self->{$opt})
                    {
                        # inherit, but only if already unspecified
                        $self->{$opt} = $self->{PARENT}->{$opt};
                    }
            }
        }
        my @fm = grep /^FIRST_MAKEFILE=/, @ARGV;
        parse_args($self,@fm) if @fm;
    }
    else {
        parse_args($self, _shellwords($ENV{PERL_MM_OPT} || ''),@ARGV);
    }

    # RT#91540 PREREQ_FATAL not recognized on command line
    if (%unsatisfied && $self->{PREREQ_FATAL}){
        my $failedprereqs = join "\n", map {"    $_ $unsatisfied{$_}"}
                            sort { $a cmp $b } keys %unsatisfied;
        die <<"END";
MakeMaker FATAL: prerequisites not found.
$failedprereqs

Please install these modules first and rerun 'perl Makefile.PL'.
END
    }

    $self->{NAME} ||= $self->guess_name;

    warn "Warning: NAME must be a package name\n"
      unless $self->{NAME} =~ m!^[A-Z_a-z][0-9A-Z_a-z]*(?:::[0-9A-Z_a-z]+)*$!;

    ($self->{NAME_SYM} = $self->{NAME}) =~ s/\W+/_/g;

    $self->init_MAKE;
    $self->init_main;
    $self->init_VERSION;
    $self->init_dist;
    $self->init_INST;
    $self->init_INSTALL;
    $self->init_DEST;
    $self->init_dirscan;
    $self->init_PM;
    $self->init_MANPODS;
    $self->init_xs;
    $self->init_PERL;
    $self->init_DIRFILESEP;
    $self->init_linker;
    $self->init_ABSTRACT;

    $self->arch_check(
        $INC{'Config.pm'},
        $self->catfile($Config{'archlibexp'}, "Config.pm")
    );

    $self->init_tools();
    $self->init_others();
    $self->init_platform();
    $self->init_PERM();
    my @args = @ARGV;
    @args = map { Encode::decode(locale => $_) } @args if $CAN_DECODE;
    my($argv) = neatvalue(\@args);
    $argv =~ s/^\[/(/;
    $argv =~ s/\]$/)/;

    push @{$self->{RESULT}}, <<END;
# This Makefile is for the $self->{NAME} extension to perl.
#
# It was generated automatically by MakeMaker version
# $VERSION (Revision: $Revision) from the contents of
# Makefile.PL. Don't edit this file, edit Makefile.PL instead.
#
#       ANY CHANGES MADE HERE WILL BE LOST!
#
#   MakeMaker ARGV: $argv
#
END

    push @{$self->{RESULT}}, $self->_MakeMaker_Parameters_section(\%initial_att);

    if (defined $self->{CONFIGURE}) {
       push @{$self->{RESULT}}, <<END;

#   MakeMaker 'CONFIGURE' Parameters:
END
        if (scalar(keys %configure_att) > 0) {
            foreach my $key (sort keys %configure_att){
               next if $key eq 'ARGS';
               my($v) = neatvalue($configure_att{$key});
               $v =~ s/(CODE|HASH|ARRAY|SCALAR)\([\dxa-f]+\)/$1\(...\)/;
               $v =~ tr/\n/ /s;
               push @{$self->{RESULT}}, "#     $key => $v";
            }
        }
        else
        {
           push @{$self->{RESULT}}, "# no values returned";
        }
        undef %configure_att;  # free memory
    }

    # turn the SKIP array into a SKIPHASH hash
    for my $skip (@{$self->{SKIP} || []}) {
        $self->{SKIPHASH}{$skip} = 1;
    }
    delete $self->{SKIP}; # free memory

    if ($self->{PARENT}) {
        for (qw/install dist dist_basics dist_core distdir dist_test dist_ci/) {
            $self->{SKIPHASH}{$_} = 1;
        }
    }

    # We run all the subdirectories now. They don't have much to query
    # from the parent, but the parent has to query them: if they need linking!
    unless ($self->{NORECURS}) {
        $self->eval_in_subdirs if @{$self->{DIR}};
    }

    foreach my $section ( @MM_Sections ){
        # Support for new foo_target() methods.
        my $method = $section;
        $method .= '_target' unless $self->can($method);

        print "Processing Makefile '$section' section\n" if ($Verbose >= 2);
        my($skipit) = $self->skipcheck($section);
        if ($skipit){
            push @{$self->{RESULT}}, "\n# --- MakeMaker $section section $skipit.";
        } else {
            my(%a) = %{$self->{$section} || {}};
            push @{$self->{RESULT}}, "\n# --- MakeMaker $section section:";
            push @{$self->{RESULT}}, "# " . join ", ", %a if $Verbose && %a;
            push @{$self->{RESULT}}, $self->maketext_filter(
                $self->$method( %a )
            );
        }
    }

    push @{$self->{RESULT}}, "\n# End.";

    $self;
}

sub WriteEmptyMakefile {
    croak "WriteEmptyMakefile: Need an even number of args" if @_ % 2;

    my %att = @_;
    $att{DIR} = [] unless $att{DIR}; # don't recurse by default
    my $self = MM->new(\%att);

    my $new = $self->{MAKEFILE};
    my $old = $self->{MAKEFILE_OLD};
    if (-f $old) {
        _unlink($old) or warn "unlink $old: $!";
    }
    if ( -f $new ) {
        _rename($new, $old) or warn "rename $new => $old: $!"
    }
    open my $mfh, '>', $new or die "open $new for write: $!";
    print $mfh <<'EOP';
all :

manifypods :

subdirs :

dynamic :

static :

clean :

install :

makemakerdflt :

test :

test_dynamic :

test_static :

EOP
    close $mfh or die "close $new for write: $!";
}


=begin private

=head3 _installed_file_for_module

  my $file = MM->_installed_file_for_module($module);

Return the first installed .pm $file associated with the $module.  The
one which will show up when you C<use $module>.

$module is something like "strict" or "Test::More".

=end private

=cut

sub _installed_file_for_module {
    my $class  = shift;
    my $prereq = shift;

    my $file = "$prereq.pm";
    $file =~ s{::}{/}g;

    my $path;
    for my $dir (@INC) {
        my $tmp = File::Spec->catfile($dir, $file);
        if ( -r $tmp ) {
            $path = $tmp;
            last;
        }
    }

    return $path;
}


# Extracted from MakeMaker->new so we can test it
sub _MakeMaker_Parameters_section {
    my $self = shift;
    my $att  = shift;

    my @result = <<'END';
#   MakeMaker Parameters:
END

    foreach my $key (sort keys %$att){
        next if $key eq 'ARGS';
        my $v;
        if ($key eq 'PREREQ_PM') {
            # CPAN.pm takes prereqs from this field in 'Makefile'
            # and does not know about BUILD_REQUIRES
            $v = neatvalue({
                %{ $att->{PREREQ_PM} || {} },
                %{ $att->{BUILD_REQUIRES} || {} },
                %{ $att->{TEST_REQUIRES} || {} },
            });
        } else {
            $v = neatvalue($att->{$key});
        }

        $v =~ s/(CODE|HASH|ARRAY|SCALAR)\([\dxa-f]+\)/$1\(...\)/;
        $v =~ tr/\n/ /s;
        push @result, "#     $key => $v";
    }

    return @result;
}

# _shellwords and _parseline borrowed from Text::ParseWords
sub _shellwords {
    my (@lines) = @_;
    my @allwords;

    foreach my $line (@lines) {
      $line =~ s/^\s+//;
      my @words = _parse_line('\s+', 0, $line);
      pop @words if (@words and !defined $words[-1]);
      return() unless (@words || !length($line));
      push(@allwords, @words);
    }
    return(@allwords);
}

sub _parse_line {
    my($delimiter, $keep, $line) = @_;
    my($word, @pieces);

    no warnings 'uninitialized';  # we will be testing undef strings

    while (length($line)) {
        # This pattern is optimised to be stack conservative on older perls.
        # Do not refactor without being careful and testing it on very long strings.
        # See Perl bug #42980 for an example of a stack busting input.
        $line =~ s/^
                    (?:
                        # double quoted string
                        (")                             # $quote
                        ((?>[^\\"]*(?:\\.[^\\"]*)*))"   # $quoted
        | # --OR--
                        # singe quoted string
                        (')                             # $quote
                        ((?>[^\\']*(?:\\.[^\\']*)*))'   # $quoted
                    |   # --OR--
                        # unquoted string
            (                               # $unquoted
                            (?:\\.|[^\\"'])*?
                        )
                        # followed by
            (                               # $delim
                            \Z(?!\n)                    # EOL
                        |   # --OR--
                            (?-x:$delimiter)            # delimiter
                        |   # --OR--
                            (?!^)(?=["'])               # a quote
                        )
        )//xs or return;    # extended layout
        my ($quote, $quoted, $unquoted, $delim) = (($1 ? ($1,$2) : ($3,$4)), $5, $6);


  return() unless( defined($quote) || length($unquoted) || length($delim));

        if ($keep) {
      $quoted = "$quote$quoted$quote";
  }
        else {
      $unquoted =~ s/\\(.)/$1/sg;
      if (defined $quote) {
    $quoted =~ s/\\(.)/$1/sg if ($quote eq '"');
    #$quoted =~ s/\\([\\'])/$1/g if ( $PERL_SINGLE_QUOTE && $quote eq "'");
            }
  }
        $word .= substr($line, 0, 0); # leave results tainted
        $word .= defined $quote ? $quoted : $unquoted;

        if (length($delim)) {
            push(@pieces, $word);
            push(@pieces, $delim) if ($keep eq 'delimiters');
            undef $word;
        }
        if (!length($line)) {
            push(@pieces, $word);
  }
    }
    return(@pieces);
}

sub check_manifest {
    print "Checking if your kit is complete...\n";
    require ExtUtils::Manifest;
    # avoid warning
    $ExtUtils::Manifest::Quiet = $ExtUtils::Manifest::Quiet = 1;
    my(@missed) = ExtUtils::Manifest::manicheck();
    if (@missed) {
        print "Warning: the following files are missing in your kit:\n";
        print "\t", join "\n\t", @missed;
        print "\n";
        print "Please inform the author.\n";
    } else {
        print "Looks good\n";
    }
}

sub parse_args{
    my($self, @args) = @_;
    @args = map { Encode::decode(locale => $_) } @args if $CAN_DECODE;
    foreach (@args) {
        unless (m/(.*?)=(.*)/) {
            ++$Verbose if m/^verb/;
            next;
        }
        my($name, $value) = ($1, $2);
        if ($value =~ m/^~(\w+)?/) { # tilde with optional username
            $value =~ s [^~(\w*)]
                [$1 ?
                 ((getpwnam($1))[7] || "~$1") :
                 (getpwuid($>))[7]
                 ]ex;
        }

        # Remember the original args passed it.  It will be useful later.
        $self->{ARGS}{uc $name} = $self->{uc $name} = $value;
    }

    # catch old-style 'potential_libs' and inform user how to 'upgrade'
    if (defined $self->{potential_libs}){
        my($msg)="'potential_libs' => '$self->{potential_libs}' should be";
        if ($self->{potential_libs}){
            print "$msg changed to:\n\t'LIBS' => ['$self->{potential_libs}']\n";
        } else {
            print "$msg deleted.\n";
        }
        $self->{LIBS} = [$self->{potential_libs}];
        delete $self->{potential_libs};
    }
    # catch old-style 'ARMAYBE' and inform user how to 'upgrade'
    if (defined $self->{ARMAYBE}){
        my($armaybe) = $self->{ARMAYBE};
        print "ARMAYBE => '$armaybe' should be changed to:\n",
                        "\t'dynamic_lib' => {ARMAYBE => '$armaybe'}\n";
        my(%dl) = %{$self->{dynamic_lib} || {}};
        $self->{dynamic_lib} = { %dl, ARMAYBE => $armaybe};
        delete $self->{ARMAYBE};
    }
    if (defined $self->{LDTARGET}){
        print "LDTARGET should be changed to LDFROM\n";
        $self->{LDFROM} = $self->{LDTARGET};
        delete $self->{LDTARGET};
    }
    # Turn a DIR argument on the command line into an array
    if (defined $self->{DIR} && ref \$self->{DIR} eq 'SCALAR') {
        # So they can choose from the command line, which extensions they want
        # the grep enables them to have some colons too much in case they
        # have to build a list with the shell
        $self->{DIR} = [grep $_, split ":", $self->{DIR}];
    }
    # Turn a INCLUDE_EXT argument on the command line into an array
    if (defined $self->{INCLUDE_EXT} && ref \$self->{INCLUDE_EXT} eq 'SCALAR') {
        $self->{INCLUDE_EXT} = [grep $_, split '\s+', $self->{INCLUDE_EXT}];
    }
    # Turn a EXCLUDE_EXT argument on the command line into an array
    if (defined $self->{EXCLUDE_EXT} && ref \$self->{EXCLUDE_EXT} eq 'SCALAR') {
        $self->{EXCLUDE_EXT} = [grep $_, split '\s+', $self->{EXCLUDE_EXT}];
    }

    foreach my $mmkey (sort keys %$self){
        next if $mmkey eq 'ARGS';
        print "  $mmkey => ", neatvalue($self->{$mmkey}), "\n" if $Verbose;
        print "'$mmkey' is not a known MakeMaker parameter name.\n"
            unless exists $Recognized_Att_Keys{$mmkey};
    }
    $| = 1 if $Verbose;
}

sub check_hints {
    my($self) = @_;
    # We allow extension-specific hints files.

    require File::Spec;
    my $curdir = File::Spec->curdir;

    my $hint_dir = File::Spec->catdir($curdir, "hints");
    return unless -d $hint_dir;

    # First we look for the best hintsfile we have
    my($hint)="${^O}_$Config{osvers}";
    $hint =~ s/\./_/g;
    $hint =~ s/_$//;
    return unless $hint;

    # Also try without trailing minor version numbers.
    while (1) {
        last if -f File::Spec->catfile($hint_dir, "$hint.pl");  # found
    } continue {
        last unless $hint =~ s/_[^_]*$//; # nothing to cut off
    }
    my $hint_file = File::Spec->catfile($hint_dir, "$hint.pl");

    return unless -f $hint_file;    # really there

    _run_hintfile($self, $hint_file);
}

sub _run_hintfile {
    our $self;
    local($self) = shift;       # make $self available to the hint file.
    my($hint_file) = shift;

    local($@, $!);
    print "Processing hints file $hint_file\n" if $Verbose;

    # Just in case the ./ isn't on the hint file, which File::Spec can
    # often strip off, we bung the curdir into @INC
    local @INC = (File::Spec->curdir, @INC);
    my $ret = do $hint_file;
    if( !defined $ret ) {
        my $error = $@ || $!;
        warn $error;
    }
}

sub mv_all_methods {
    my($from,$to) = @_;
    local $SIG{__WARN__} = sub {
        # can't use 'no warnings redefined', 5.6 only
        warn @_ unless $_[0] =~ /^Subroutine .* redefined/
    };
    foreach my $method (@Overridable) {
        next unless defined &{"${from}::$method"};
        no strict 'refs';   ## no critic
        *{"${to}::$method"} = \&{"${from}::$method"};

        # If we delete a method, then it will be undefined and cannot
        # be called.  But as long as we have Makefile.PLs that rely on
        # %MY:: being intact, we have to fill the hole with an
        # inheriting method:

        {
            package MY;
            my $super = "SUPER::".$method;
            *{$method} = sub {
                shift->$super(@_);
            };
        }
    }
}

sub skipcheck {
    my($self) = shift;
    my($section) = @_;
    return 'skipped' if $section eq 'metafile' && $UNDER_CORE;
    if ($section eq 'dynamic') {
        print "Warning (non-fatal): Target 'dynamic' depends on targets ",
        "in skipped section 'dynamic_bs'\n"
            if $self->{SKIPHASH}{dynamic_bs} && $Verbose;
        print "Warning (non-fatal): Target 'dynamic' depends on targets ",
        "in skipped section 'dynamic_lib'\n"
            if $self->{SKIPHASH}{dynamic_lib} && $Verbose;
    }
    if ($section eq 'dynamic_lib') {
        print "Warning (non-fatal): Target '\$(INST_DYNAMIC)' depends on ",
        "targets in skipped section 'dynamic_bs'\n"
            if $self->{SKIPHASH}{dynamic_bs} && $Verbose;
    }
    if ($section eq 'static') {
        print "Warning (non-fatal): Target 'static' depends on targets ",
        "in skipped section 'static_lib'\n"
            if $self->{SKIPHASH}{static_lib} && $Verbose;
    }
    return 'skipped' if $self->{SKIPHASH}{$section};
    return '';
}

# returns filehandle, dies on fail. :raw so no :crlf
sub open_for_writing {
    my ($file) = @_;
    open my $fh ,">", $file or die "Unable to open $file: $!";
    my @layers = ':raw';
    push @layers, join ' ', ':encoding(locale)' if $CAN_DECODE;
    binmode $fh, join ' ', @layers;
    $fh;
}

sub flush {
    my $self = shift;

    my $finalname = $self->{MAKEFILE};
    printf "Generating a %s %s\n", $self->make_type, $finalname if $Verbose || !$self->{PARENT};
    print "Writing $finalname for $self->{NAME}\n" if $Verbose || !$self->{PARENT};

    unlink($finalname, "MakeMaker.tmp", $Is_VMS ? 'Descrip.MMS' : ());

    write_file_via_tmp($finalname, $self->{RESULT});

    # Write MYMETA.yml to communicate metadata up to the CPAN clients
    print "Writing MYMETA.yml and MYMETA.json\n"
      if !$self->{NO_MYMETA} and $self->write_mymeta( $self->mymeta );

    # save memory
    if ($self->{PARENT} && !$self->{_KEEP_AFTER_FLUSH}) {
        my %keep = map { ($_ => 1) } qw(NEEDS_LINKING HAS_LINK_CODE);
        delete $self->{$_} for grep !$keep{$_}, keys %$self;
    }

    system("$Config::Config{eunicefix} $finalname")
      if $Config::Config{eunicefix} ne ":";

    return;
}

sub write_file_via_tmp {
    my ($finalname, $contents) = @_;
    my $fh = open_for_writing("MakeMaker.tmp");
    die "write_file_via_tmp: 2nd arg must be ref" unless ref $contents;
    for my $chunk (@$contents) {
        my $to_write = $chunk;
        utf8::encode $to_write if !$CAN_DECODE && $] > 5.008;
        print $fh "$to_write\n" or die "Can't write to MakeMaker.tmp: $!";
    }
    close $fh or die "Can't write to MakeMaker.tmp: $!";
    _rename("MakeMaker.tmp", $finalname) or
      warn "rename MakeMaker.tmp => $finalname: $!";
    chmod 0644, $finalname if !$Is_VMS;
    return;
}

# This is a rename for OS's where the target must be unlinked first.
sub _rename {
    my($src, $dest) = @_;
    _unlink($dest);
    return rename $src, $dest;
}

# This is an unlink for OS's where the target must be writable first.
sub _unlink {
    my @files = @_;
    chmod 0666, @files;
    return unlink @files;
}


# The following mkbootstrap() is only for installations that are calling
# the pre-4.1 mkbootstrap() from their old Makefiles. This MakeMaker
# writes Makefiles, that use ExtUtils::Mkbootstrap directly.
sub mkbootstrap {
    die <<END;
!!! Your Makefile has been built such a long time ago, !!!
!!! that is unlikely to work with current MakeMaker.   !!!
!!! Please rebuild your Makefile                       !!!
END
}

# Ditto for mksymlists() as of MakeMaker 5.17
sub mksymlists {
    die <<END;
!!! Your Makefile has been built such a long time ago, !!!
!!! that is unlikely to work with current MakeMaker.   !!!
!!! Please rebuild your Makefile                       !!!
END
}

sub neatvalue {
    my($v) = @_;
    return "undef" unless defined $v;
    my($t) = ref $v;
    return "q[$v]" unless $t;
    if ($t eq 'ARRAY') {
        my(@m, @neat);
        push @m, "[";
        foreach my $elem (@$v) {
            push @neat, "q[$elem]";
        }
        push @m, join ", ", @neat;
        push @m, "]";
        return join "", @m;
    }
    return $v unless $t eq 'HASH';
    my(@m, $key, $val);
    for my $key (sort keys %$v) {
        last unless defined $key; # cautious programming in case (undef,undef) is true
        push @m,"$key=>".neatvalue($v->{$key});
    }
    return "{ ".join(', ',@m)." }";
}

sub _find_magic_vstring {
    my $value = shift;
    return $value if $UNDER_CORE;
    my $tvalue = '';
    require B;
    my $sv = B::svref_2object(\$value);
    my $magic = ref($sv) eq 'B::PVMG' ? $sv->MAGIC : undef;
    while ( $magic ) {
        if ( $magic->TYPE eq 'V' ) {
            $tvalue = $magic->PTR;
            $tvalue =~ s/^v?(.+)$/v$1/;
            last;
        }
        else {
            $magic = $magic->MOREMAGIC;
        }
    }
    return $tvalue;
}

sub selfdocument {
    my($self) = @_;
    my(@m);
    if ($Verbose){
        push @m, "\n# Full list of MakeMaker attribute values:";
        foreach my $key (sort keys %$self){
            next if $key eq 'RESULT' || $key =~ /^[A-Z][a-z]/;
            my($v) = neatvalue($self->{$key});
            $v =~ s/(CODE|HASH|ARRAY|SCALAR)\([\dxa-f]+\)/$1\(...\)/;
            $v =~ tr/\n/ /s;
            push @m, "# $key => $v";
        }
    }
    # added here as selfdocument is not overridable
    push @m, <<'EOF';

# here so even if top_targets is overridden, these will still be defined
# gmake will silently still work if any are .PHONY-ed but nmake won't
EOF
    push @m, join "\n", map "$_ ::\n\t\$(NOECHO) \$(NOOP)\n",
        # config is so manifypods won't puke if no subdirs
        grep !$self->{SKIPHASH}{$_},
        qw(static dynamic config);
    join "\n", @m;
}

1;

__END__

=head1 NAME

ExtUtils::MakeMaker - Create a module Makefile

=head1 SYNOPSIS

  use ExtUtils::MakeMaker;

  WriteMakefile(
      NAME              => "Foo::Bar",
      VERSION_FROM      => "lib/Foo/Bar.pm",
  );

=head1 DESCRIPTION

This utility is designed to write a Makefile for an extension module
from a Makefile.PL. It is based on the Makefile.SH model provided by
Andy Dougherty and the perl5-porters.

It splits the task of generating the Makefile into several subroutines
that can be individually overridden.  Each subroutine returns the text
it wishes to have written to the Makefile.

As there are various Make programs with incompatible syntax, which
use operating system shells, again with incompatible syntax, it is
important for users of this module to know which flavour of Make
a Makefile has been written for so they'll use the correct one and
won't have to face the possibly bewildering errors resulting from
using the wrong one.

On POSIX systems, that program will likely be GNU Make; on Microsoft
Windows, it will be either Microsoft NMake, DMake or GNU Make.
See the section on the L</"MAKE"> parameter for details.

ExtUtils::MakeMaker (EUMM) is object oriented. Each directory below the current
directory that contains a Makefile.PL is treated as a separate
object. This makes it possible to write an unlimited number of
Makefiles with a single invocation of WriteMakefile().

All inputs to WriteMakefile are Unicode characters, not just octets. EUMM
seeks to handle all of these correctly. It is currently still not possible
to portably use Unicode characters in module names, because this requires
Perl to handle Unicode filenames, which is not yet the case on Windows.

=head2 How To Write A Makefile.PL

See L<ExtUtils::MakeMaker::Tutorial>.

The long answer is the rest of the manpage :-)

=head2 Default Makefile Behaviour

The generated Makefile enables the user of the extension to invoke

  perl Makefile.PL # optionally "perl Makefile.PL verbose"
  make
  make test        # optionally set TEST_VERBOSE=1
  make install     # See below

The Makefile to be produced may be altered by adding arguments of the
form C<KEY=VALUE>. E.g.

  perl Makefile.PL INSTALL_BASE=~

Other interesting targets in the generated Makefile are

  make config     # to check if the Makefile is up-to-date
  make clean      # delete local temp files (Makefile gets renamed)
  make realclean  # delete derived files (including ./blib)
  make ci         # check in all the files in the MANIFEST file
  make dist       # see below the Distribution Support section

=head2 make test

MakeMaker checks for the existence of a file named F<test.pl> in the
current directory, and if it exists it executes the script with the
proper set of perl C<-I> options.

MakeMaker also checks for any files matching glob("t/*.t"). It will
execute all matching files in alphabetical order via the
L<Test::Harness> module with the C<-I> switches set correctly.

You can also organize your tests within subdirectories in the F<t/> directory.
To do so, use the F<test> directive in your I<Makefile.PL>. For example, if you
had tests in:

    t/foo
    t/foo/bar

You could tell make to run tests in both of those directories with the
following directives:

    test => {TESTS => 't/*/*.t t/*/*/*.t'}
    test => {TESTS => 't/foo/*.t t/foo/bar/*.t'}

The first will run all test files in all first-level subdirectories and all
subdirectories they contain. The second will run tests in only the F<t/foo>
and F<t/foo/bar>.

If you'd like to see the raw output of your tests, set the
C<TEST_VERBOSE> variable to true.

  make test TEST_VERBOSE=1

If you want to run particular test files, set the C<TEST_FILES> variable.
It is possible to use globbing with this mechanism.

  make test TEST_FILES='t/foobar.t t/dagobah*.t'

Windows users who are using C<nmake> should note that due to a bug in C<nmake>,
when specifying C<TEST_FILES> you must use back-slashes instead of forward-slashes.

  nmake test TEST_FILES='t\foobar.t t\dagobah*.t'

=head2 make testdb

A useful variation of the above is the target C<testdb>. It runs the
test under the Perl debugger (see L<perldebug>). If the file
F<test.pl> exists in the current directory, it is used for the test.

If you want to debug some other testfile, set the C<TEST_FILE> variable
thusly:

  make testdb TEST_FILE=t/mytest.t

By default the debugger is called using C<-d> option to perl. If you
want to specify some other option, set the C<TESTDB_SW> variable:

  make testdb TESTDB_SW=-Dx

=head2 make install

make alone puts all relevant files into directories that are named by
the macros INST_LIB, INST_ARCHLIB, INST_SCRIPT, INST_MAN1DIR and
INST_MAN3DIR.  All these default to something below ./blib if you are
I<not> building below the perl source directory. If you I<are>
building below the perl source, INST_LIB and INST_ARCHLIB default to
../../lib, and INST_SCRIPT is not defined.

The I<install> target of the generated Makefile copies the files found
below each of the INST_* directories to their INSTALL*
counterparts. Which counterparts are chosen depends on the setting of
INSTALLDIRS according to the following table:

                                 INSTALLDIRS set to
                           perl        site          vendor

                 PERLPREFIX      SITEPREFIX          VENDORPREFIX
  INST_ARCHLIB   INSTALLARCHLIB  INSTALLSITEARCH     INSTALLVENDORARCH
  INST_LIB       INSTALLPRIVLIB  INSTALLSITELIB      INSTALLVENDORLIB
  INST_BIN       INSTALLBIN      INSTALLSITEBIN      INSTALLVENDORBIN
  INST_SCRIPT    INSTALLSCRIPT   INSTALLSITESCRIPT   INSTALLVENDORSCRIPT
  INST_MAN1DIR   INSTALLMAN1DIR  INSTALLSITEMAN1DIR  INSTALLVENDORMAN1DIR
  INST_MAN3DIR   INSTALLMAN3DIR  INSTALLSITEMAN3DIR  INSTALLVENDORMAN3DIR

The INSTALL... macros in turn default to their %Config
($Config{installprivlib}, $Config{installarchlib}, etc.) counterparts.

You can check the values of these variables on your system with

    perl '-V:install.*'

And to check the sequence in which the library directories are
searched by perl, run

    perl -le 'print join $/, @INC'

Sometimes older versions of the module you're installing live in other
directories in @INC.  Because Perl loads the first version of a module it
finds, not the newest, you might accidentally get one of these older
versions even after installing a brand new version.  To delete I<all other
versions of the module you're installing> (not simply older ones) set the
C<UNINST> variable.

    make install UNINST=1


=head2 INSTALL_BASE

INSTALL_BASE can be passed into Makefile.PL to change where your
module will be installed.  INSTALL_BASE is more like what everyone
else calls "prefix" than PREFIX is.

To have everything installed in your home directory, do the following.

    # Unix users, INSTALL_BASE=~ works fine
    perl Makefile.PL INSTALL_BASE=/path/to/your/home/dir

Like PREFIX, it sets several INSTALL* attributes at once.  Unlike
PREFIX it is easy to predict where the module will end up.  The
installation pattern looks like this:

    INSTALLARCHLIB     INSTALL_BASE/lib/perl5/$Config{archname}
    INSTALLPRIVLIB     INSTALL_BASE/lib/perl5
    INSTALLBIN         INSTALL_BASE/bin
    INSTALLSCRIPT      INSTALL_BASE/bin
    INSTALLMAN1DIR     INSTALL_BASE/man/man1
    INSTALLMAN3DIR     INSTALL_BASE/man/man3

INSTALL_BASE in MakeMaker and C<--install_base> in Module::Build (as
of 0.28) install to the same location.  If you want MakeMaker and
Module::Build to install to the same location simply set INSTALL_BASE
and C<--install_base> to the same location.

INSTALL_BASE was added in 6.31.


=head2 PREFIX and LIB attribute

PREFIX and LIB can be used to set several INSTALL* attributes in one
go.  Here's an example for installing into your home directory.

    # Unix users, PREFIX=~ works fine
    perl Makefile.PL PREFIX=/path/to/your/home/dir

This will install all files in the module under your home directory,
with man pages and libraries going into an appropriate place (usually
~/man and ~/lib).  How the exact location is determined is complicated
and depends on how your Perl was configured.  INSTALL_BASE works more
like what other build systems call "prefix" than PREFIX and we
recommend you use that instead.

Another way to specify many INSTALL directories with a single
parameter is LIB.

    perl Makefile.PL LIB=~/lib

This will install the module's architecture-independent files into
~/lib, the architecture-dependent files into ~/lib/$archname.

Note, that in both cases the tilde expansion is done by MakeMaker, not
by perl by default, nor by make.

Conflicts between parameters LIB, PREFIX and the various INSTALL*
arguments are resolved so that:

=over 4

=item *

setting LIB overrides any setting of INSTALLPRIVLIB, INSTALLARCHLIB,
INSTALLSITELIB, INSTALLSITEARCH (and they are not affected by PREFIX);

=item *

without LIB, setting PREFIX replaces the initial C<$Config{prefix}>
part of those INSTALL* arguments, even if the latter are explicitly
set (but are set to still start with C<$Config{prefix}>).

=back

If the user has superuser privileges, and is not working on AFS or
relatives, then the defaults for INSTALLPRIVLIB, INSTALLARCHLIB,
INSTALLSCRIPT, etc. will be appropriate, and this incantation will be
the best:

    perl Makefile.PL;
    make;
    make test
    make install

make install by default writes some documentation of what has been
done into the file C<$(INSTALLARCHLIB)/perllocal.pod>. This feature
can be bypassed by calling make pure_install.

=head2 AFS users

will have to specify the installation directories as these most
probably have changed since perl itself has been installed. They will
have to do this by calling

    perl Makefile.PL INSTALLSITELIB=/afs/here/today \
        INSTALLSCRIPT=/afs/there/now INSTALLMAN3DIR=/afs/for/manpages
    make

Be careful to repeat this procedure every time you recompile an
extension, unless you are sure the AFS installation directories are
still valid.

=head2 Static Linking of a new Perl Binary

An extension that is built with the above steps is ready to use on
systems supporting dynamic loading. On systems that do not support
dynamic loading, any newly created extension has to be linked together
with the available resources. MakeMaker supports the linking process
by creating appropriate targets in the Makefile whenever an extension
is built. You can invoke the corresponding section of the makefile with

    make perl

That produces a new perl binary in the current directory with all
extensions linked in that can be found in INST_ARCHLIB, SITELIBEXP,
and PERL_ARCHLIB. To do that, MakeMaker writes a new Makefile, on
UNIX, this is called F<Makefile.aperl> (may be system dependent). If you
want to force the creation of a new perl, it is recommended that you
delete this F<Makefile.aperl>, so the directories are searched through
for linkable libraries again.

The binary can be installed into the directory where perl normally
resides on your machine with

    make inst_perl

To produce a perl binary with a different name than C<perl>, either say

    perl Makefile.PL MAP_TARGET=myperl
    make myperl
    make inst_perl

or say

    perl Makefile.PL
    make myperl MAP_TARGET=myperl
    make inst_perl MAP_TARGET=myperl

In any case you will be prompted with the correct invocation of the
C<inst_perl> target that installs the new binary into INSTALLBIN.

make inst_perl by default writes some documentation of what has been
done into the file C<$(INSTALLARCHLIB)/perllocal.pod>. This
can be bypassed by calling make pure_inst_perl.

Warning: the inst_perl: target will most probably overwrite your
existing perl binary. Use with care!

Sometimes you might want to build a statically linked perl although
your system supports dynamic loading. In this case you may explicitly
set the linktype with the invocation of the Makefile.PL or make:

    perl Makefile.PL LINKTYPE=static    # recommended

or

    make LINKTYPE=static                # works on most systems

=head2 Determination of Perl Library and Installation Locations

MakeMaker needs to know, or to guess, where certain things are
located.  Especially INST_LIB and INST_ARCHLIB (where to put the files
during the make(1) run), PERL_LIB and PERL_ARCHLIB (where to read
existing modules from), and PERL_INC (header files and C<libperl*.*>).

Extensions may be built either using the contents of the perl source
directory tree or from the installed perl library. The recommended way
is to build extensions after you have run 'make install' on perl
itself. You can do that in any directory on your hard disk that is not
below the perl source tree. The support for extensions below the ext
directory of the perl distribution is only good for the standard
extensions that come with perl.

If an extension is being built below the C<ext/> directory of the perl
source then MakeMaker will set PERL_SRC automatically (e.g.,
C<../..>).  If PERL_SRC is defined and the extension is recognized as
a standard extension, then other variables default to the following:

  PERL_INC     = PERL_SRC
  PERL_LIB     = PERL_SRC/lib
  PERL_ARCHLIB = PERL_SRC/lib
  INST_LIB     = PERL_LIB
  INST_ARCHLIB = PERL_ARCHLIB

If an extension is being built away from the perl source then MakeMaker
will leave PERL_SRC undefined and default to using the installed copy
of the perl library. The other variables default to the following:

  PERL_INC     = $archlibexp/CORE
  PERL_LIB     = $privlibexp
  PERL_ARCHLIB = $archlibexp
  INST_LIB     = ./blib/lib
  INST_ARCHLIB = ./blib/arch

If perl has not yet been installed then PERL_SRC can be defined on the
command line as shown in the previous section.


=head2 Which architecture dependent directory?

If you don't want to keep the defaults for the INSTALL* macros,
MakeMaker helps you to minimize the typing needed: the usual
relationship between INSTALLPRIVLIB and INSTALLARCHLIB is determined
by Configure at perl compilation time. MakeMaker supports the user who
sets INSTALLPRIVLIB. If INSTALLPRIVLIB is set, but INSTALLARCHLIB not,
then MakeMaker defaults the latter to be the same subdirectory of
INSTALLPRIVLIB as Configure decided for the counterparts in %Config,
otherwise it defaults to INSTALLPRIVLIB. The same relationship holds
for INSTALLSITELIB and INSTALLSITEARCH.

MakeMaker gives you much more freedom than needed to configure
internal variables and get different results. It is worth mentioning
that make(1) also lets you configure most of the variables that are
used in the Makefile. But in the majority of situations this will not
be necessary, and should only be done if the author of a package
recommends it (or you know what you're doing).

=head2 Using Attributes and Parameters

The following attributes may be specified as arguments to WriteMakefile()
or as NAME=VALUE pairs on the command line. Attributes that became
available with later versions of MakeMaker are indicated.

In order to maintain portability of attributes with older versions of
MakeMaker you may want to use L<App::EUMM::Upgrade> with your C<Makefile.PL>.

=over 2

=item ABSTRACT

One line description of the module. Will be included in PPD file.

=item ABSTRACT_FROM

Name of the file that contains the package description. MakeMaker looks
for a line in the POD matching /^($package\s-\s)(.*)/. This is typically
the first line in the "=head1 NAME" section. $2 becomes the abstract.

=item AUTHOR

Array of strings containing name (and email address) of package author(s).
Is used in CPAN Meta files (META.yml or META.json) and PPD
(Perl Package Description) files for PPM (Perl Package Manager).

=item BINARY_LOCATION

Used when creating PPD files for binary packages.  It can be set to a
full or relative path or URL to the binary archive for a particular
architecture.  For example:

        perl Makefile.PL BINARY_LOCATION=x86/Agent.tar.gz

builds a PPD package that references a binary of the C<Agent> package,
located in the C<x86> directory relative to the PPD itself.

=item BUILD_REQUIRES

Available in version 6.55_03 and above.

A hash of modules that are needed to build your module but not run it.

This will go into the C<build_requires> field of your F<META.yml> and the C<build> of the C<prereqs> field of your F<META.json>.

Defaults to C<<< { "ExtUtils::MakeMaker" => 0 } >>> if this attribute is not specified.

The format is the same as PREREQ_PM.

=item C

Ref to array of *.c file names. Initialised from a directory scan
and the values portion of the XS attribute hash. This is not
currently used by MakeMaker but may be handy in Makefile.PLs.

=item CCFLAGS

String that will be included in the compiler call command line between
the arguments INC and OPTIMIZE.

=item CONFIG

Arrayref. E.g. [qw(archname manext)] defines ARCHNAME & MANEXT from
config.sh. MakeMaker will add to CONFIG the following values anyway:
ar
cc
cccdlflags
ccdlflags
dlext
dlsrc
ld
lddlflags
ldflags
libc
lib_ext
obj_ext
ranlib
sitelibexp
sitearchexp
so

=item CONFIGURE

CODE reference. The subroutine should return a hash reference. The
hash may contain further attributes, e.g. {LIBS =E<gt> ...}, that have to
be determined by some evaluation method.

=item CONFIGURE_REQUIRES

Available in version 6.52 and above.

A hash of modules that are required to run Makefile.PL itself, but not
to run your distribution.

This will go into the C<configure_requires> field of your F<META.yml> and the C<configure> of the C<prereqs> field of your F<META.json>.

Defaults to C<<< { "ExtUtils::MakeMaker" => 0 } >>> if this attribute is not specified.

The format is the same as PREREQ_PM.

=item DEFINE

Something like C<"-DHAVE_UNISTD_H">

=item DESTDIR

This is the root directory into which the code will be installed.  It
I<prepends itself to the normal prefix>.  For example, if your code
would normally go into F</usr/local/lib/perl> you could set DESTDIR=~/tmp/
and installation would go into F<~/tmp/usr/local/lib/perl>.

This is primarily of use for people who repackage Perl modules.

NOTE: Due to the nature of make, it is important that you put the trailing
slash on your DESTDIR.  F<~/tmp/> not F<~/tmp>.

=item DIR

Ref to array of subdirectories containing Makefile.PLs e.g. ['sdbm']
in ext/SDBM_File

=item DISTNAME

A safe filename for the package.

Defaults to NAME below but with :: replaced with -.

For example, Foo::Bar becomes Foo-Bar.

=item DISTVNAME

Your name for distributing the package with the version number
included.  This is used by 'make dist' to name the resulting archive
file.

Defaults to DISTNAME-VERSION.

For example, version 1.04 of Foo::Bar becomes Foo-Bar-1.04.

On some OS's where . has special meaning VERSION_SYM may be used in
place of VERSION.

=item DLEXT

Specifies the extension of the module's loadable object. For example:

  DLEXT => 'unusual_ext', # Default value is $Config{so}

NOTE: When using this option to alter the extension of a module's
loadable object, it is also necessary that the module's pm file
specifies the same change:

  local $DynaLoader::dl_dlext = 'unusual_ext';

=item DL_FUNCS

Hashref of symbol names for routines to be made available as universal
symbols.  Each key/value pair consists of the package name and an
array of routine names in that package.  Used only under AIX, OS/2,
VMS and Win32 at present.  The routine names supplied will be expanded
in the same way as XSUB names are expanded by the XS() macro.
Defaults to

  {"$(NAME)" => ["boot_$(NAME)" ] }

e.g.

  {"RPC" => [qw( boot_rpcb rpcb_gettime getnetconfigent )],
   "NetconfigPtr" => [ 'DESTROY'] }

Please see the L<ExtUtils::Mksymlists> documentation for more information
about the DL_FUNCS, DL_VARS and FUNCLIST attributes.

=item DL_VARS

Array of symbol names for variables to be made available as universal symbols.
Used only under AIX, OS/2, VMS and Win32 at present.  Defaults to [].
(e.g. [ qw(Foo_version Foo_numstreams Foo_tree ) ])

=item EXCLUDE_EXT

Array of extension names to exclude when doing a static build.  This
is ignored if INCLUDE_EXT is present.  Consult INCLUDE_EXT for more
details.  (e.g.  [ qw( Socket POSIX ) ] )

This attribute may be most useful when specified as a string on the
command line:  perl Makefile.PL EXCLUDE_EXT='Socket Safe'

=item EXE_FILES

Ref to array of executable files. The files will be copied to the
INST_SCRIPT directory. Make realclean will delete them from there
again.

If your executables start with something like #!perl or
#!/usr/bin/perl MakeMaker will change this to the path of the perl
'Makefile.PL' was invoked with so the programs will be sure to run
properly even if perl is not in /usr/bin/perl.

=item FIRST_MAKEFILE

The name of the Makefile to be produced.  This is used for the second
Makefile that will be produced for the MAP_TARGET.

Defaults to 'Makefile' or 'Descrip.MMS' on VMS.

(Note: we couldn't use MAKEFILE because dmake uses this for something
else).

=item FULLPERL

Perl binary able to run this extension, load XS modules, etc...

=item FULLPERLRUN

Like PERLRUN, except it uses FULLPERL.

=item FULLPERLRUNINST

Like PERLRUNINST, except it uses FULLPERL.

=item FUNCLIST

This provides an alternate means to specify function names to be
exported from the extension.  Its value is a reference to an
array of function names to be exported by the extension.  These
names are passed through unaltered to the linker options file.

=item H

Ref to array of *.h file names. Similar to C.

=item IMPORTS

This attribute is used to specify names to be imported into the
extension. Takes a hash ref.

It is only used on OS/2 and Win32.

=item INC

Include file dirs eg: C<"-I/usr/5include -I/path/to/inc">

=item INCLUDE_EXT

Array of extension names to be included when doing a static build.
MakeMaker will normally build with all of the installed extensions when
doing a static build, and that is usually the desired behavior.  If
INCLUDE_EXT is present then MakeMaker will build only with those extensions
which are explicitly mentioned. (e.g.  [ qw( Socket POSIX ) ])

It is not necessary to mention DynaLoader or the current extension when
filling in INCLUDE_EXT.  If the INCLUDE_EXT is mentioned but is empty then
only DynaLoader and the current extension will be included in the build.

This attribute may be most useful when specified as a string on the
command line:  perl Makefile.PL INCLUDE_EXT='POSIX Socket Devel::Peek'

=item INSTALLARCHLIB

Used by 'make install', which copies files from INST_ARCHLIB to this
directory if INSTALLDIRS is set to perl.

=item INSTALLBIN

Directory to install binary files (e.g. tkperl) into if
INSTALLDIRS=perl.

=item INSTALLDIRS

Determines which of the sets of installation directories to choose:
perl, site or vendor.  Defaults to site.

=item INSTALLMAN1DIR

=item INSTALLMAN3DIR

These directories get the man pages at 'make install' time if
INSTALLDIRS=perl.  Defaults to $Config{installman*dir}.

If set to 'none', no man pages will be installed.

=item INSTALLPRIVLIB

Used by 'make install', which copies files from INST_LIB to this
directory if INSTALLDIRS is set to perl.

Defaults to $Config{installprivlib}.

=item INSTALLSCRIPT

Available in version 6.30_02 and above.

Used by 'make install' which copies files from INST_SCRIPT to this
directory if INSTALLDIRS=perl.

=item INSTALLSITEARCH

Used by 'make install', which copies files from INST_ARCHLIB to this
directory if INSTALLDIRS is set to site (default).

=item INSTALLSITEBIN

Used by 'make install', which copies files from INST_BIN to this
directory if INSTALLDIRS is set to site (default).

=item INSTALLSITELIB

Used by 'make install', which copies files from INST_LIB to this
directory if INSTALLDIRS is set to site (default).

=item INSTALLSITEMAN1DIR

=item INSTALLSITEMAN3DIR

These directories get the man pages at 'make install' time if
INSTALLDIRS=site (default).  Defaults to
$(SITEPREFIX)/man/man$(MAN*EXT).

If set to 'none', no man pages will be installed.

=item INSTALLSITESCRIPT

Used by 'make install' which copies files from INST_SCRIPT to this
directory if INSTALLDIRS is set to site (default).

=item INSTALLVENDORARCH

Used by 'make install', which copies files from INST_ARCHLIB to this
directory if INSTALLDIRS is set to vendor. Note that if you do not set
this, the value of INSTALLVENDORLIB will be used, which is probably not
what you want.

=item INSTALLVENDORBIN

Used by 'make install', which copies files from INST_BIN to this
directory if INSTALLDIRS is set to vendor.

=item INSTALLVENDORLIB

Used by 'make install', which copies files from INST_LIB to this
directory if INSTALLDIRS is set to vendor.

=item INSTALLVENDORMAN1DIR

=item INSTALLVENDORMAN3DIR

These directories get the man pages at 'make install' time if
INSTALLDIRS=vendor.  Defaults to $(VENDORPREFIX)/man/man$(MAN*EXT).

If set to 'none', no man pages will be installed.

=item INSTALLVENDORSCRIPT

Available in version 6.30_02 and above.

Used by 'make install' which copies files from INST_SCRIPT to this
directory if INSTALLDIRS is set to vendor.

=item INST_ARCHLIB

Same as INST_LIB for architecture dependent files.

=item INST_BIN

Directory to put real binary files during 'make'. These will be copied
to INSTALLBIN during 'make install'

=item INST_LIB

Directory where we put library files of this extension while building
it.

=item INST_MAN1DIR

Directory to hold the man pages at 'make' time

=item INST_MAN3DIR

Directory to hold the man pages at 'make' time

=item INST_SCRIPT

Directory where executable files should be installed during
'make'. Defaults to "./blib/script", just to have a dummy location during
testing. make install will copy the files in INST_SCRIPT to
INSTALLSCRIPT.

=item LD

Program to be used to link libraries for dynamic loading.

Defaults to $Config{ld}.

=item LDDLFLAGS

Any special flags that might need to be passed to ld to create a
shared library suitable for dynamic loading.  It is up to the makefile
to use it.  (See L<Config/lddlflags>)

Defaults to $Config{lddlflags}.

=item LDFROM

Defaults to "$(OBJECT)" and is used in the ld command to specify
what files to link/load from (also see dynamic_lib below for how to
specify ld flags)

=item LIB

LIB should only be set at C<perl Makefile.PL> time but is allowed as a
MakeMaker argument. It has the effect of setting both INSTALLPRIVLIB
and INSTALLSITELIB to that value regardless any explicit setting of
those arguments (or of PREFIX).  INSTALLARCHLIB and INSTALLSITEARCH
are set to the corresponding architecture subdirectory.

=item LIBPERL_A

The filename of the perllibrary that will be used together with this
extension. Defaults to libperl.a.

=item LIBS

An anonymous array of alternative library
specifications to be searched for (in order) until
at least one library is found. E.g.

  'LIBS' => ["-lgdbm", "-ldbm -lfoo", "-L/path -ldbm.nfs"]

Mind, that any element of the array
contains a complete set of arguments for the ld
command. So do not specify

  'LIBS' => ["-ltcl", "-ltk", "-lX11"]

See ODBM_File/Makefile.PL for an example, where an array is needed. If
you specify a scalar as in

  'LIBS' => "-ltcl -ltk -lX11"

MakeMaker will turn it into an array with one element.

=item LICENSE

Available in version 6.31 and above.

The licensing terms of your distribution.  Generally it's "perl_5" for the
same license as Perl itself.

See L<CPAN::Meta::Spec> for the list of options.

Defaults to "unknown".

=item LINKTYPE

'static' or 'dynamic' (default unless usedl=undef in
config.sh). Should only be used to force static linking (also see
linkext below).

=item MAGICXS

Available in version 6.8305 and above.

When this is set to C<1>, C<OBJECT> will be automagically derived from
C<O_FILES>.

=item MAKE

Available in version 6.30_01 and above.

Variant of make you intend to run the generated Makefile with.  This
parameter lets Makefile.PL know what make quirks to account for when
generating the Makefile.

MakeMaker also honors the MAKE environment variable.  This parameter
takes precedence.

Currently the only significant values are 'dmake' and 'nmake' for Windows
users, instructing MakeMaker to generate a Makefile in the flavour of
DMake ("Dennis Vadura's Make") or Microsoft NMake respectively.

Defaults to $Config{make}, which may go looking for a Make program
in your environment.

How are you supposed to know what flavour of Make a Makefile has
been generated for if you didn't specify a value explicitly? Search
the generated Makefile for the definition of the MAKE variable,
which is used to recursively invoke the Make utility. That will tell
you what Make you're supposed to invoke the Makefile with.

=item MAKEAPERL

Boolean which tells MakeMaker that it should include the rules to
make a perl. This is handled automatically as a switch by
MakeMaker. The user normally does not need it.

=item MAKEFILE_OLD

When 'make clean' or similar is run, the $(FIRST_MAKEFILE) will be
backed up at this location.

Defaults to $(FIRST_MAKEFILE).old or $(FIRST_MAKEFILE)_old on VMS.

=item MAN1PODS

Hashref of pod-containing files. MakeMaker will default this to all
EXE_FILES files that include POD directives. The files listed
here will be converted to man pages and installed as was requested
at Configure time.

This hash should map POD files (or scripts containing POD) to the
man file names under the C<blib/man1/> directory, as in the following
example:

  MAN1PODS            => {
    'doc/command.pod'    => 'blib/man1/command.1',
    'scripts/script.pl'  => 'blib/man1/script.1',
  }

=item MAN3PODS

Hashref that assigns to *.pm and *.pod files the files into which the
manpages are to be written. MakeMaker parses all *.pod and *.pm files
for POD directives. Files that contain POD will be the default keys of
the MAN3PODS hashref. These will then be converted to man pages during
C<make> and will be installed during C<make install>.

Example similar to MAN1PODS.

=item MAP_TARGET

If it is intended that a new perl binary be produced, this variable
may hold a name for that binary. Defaults to perl

=item META_ADD

=item META_MERGE

Available in version 6.46 and above.

A hashref of items to add to the CPAN Meta file (F<META.yml> or
F<META.json>).

They differ in how they behave if they have the same key as the
default metadata.  META_ADD will override the default value with its
own.  META_MERGE will merge its value with the default.

Unless you want to override the defaults, prefer META_MERGE so as to
get the advantage of any future defaults.

Where prereqs are concerned, if META_MERGE is used, prerequisites are merged
with their counterpart C<WriteMakefile()> argument
(PREREQ_PM is merged into {prereqs}{runtime}{requires},
BUILD_REQUIRES into C<{prereqs}{build}{requires}>,
CONFIGURE_REQUIRES into C<{prereqs}{configure}{requires}>,
and TEST_REQUIRES into C<{prereqs}{test}{requires})>.
When prereqs are specified with META_ADD, the only prerequisites added to the
file come from the metadata, not C<WriteMakefile()> arguments.

Note that these configuration options are only used for generating F<META.yml>
and F<META.json> -- they are NOT used for F<MYMETA.yml> and F<MYMETA.json>.
Therefore data in these fields should NOT be used for dynamic (user-side)
configuration.

By default CPAN Meta specification C<1.4> is used. In order to use
CPAN Meta specification C<2.0>, indicate with C<meta-spec> the version
you want to use.

  META_MERGE        => {

    "meta-spec" => { version => 2 },

    resources => {

      repository => {
          type => 'git',
          url => 'git://github.com/Perl-Toolchain-Gang/ExtUtils-MakeMaker.git',
          web => 'https://github.com/Perl-Toolchain-Gang/ExtUtils-MakeMaker',
      },

    },

  },

=item MIN_PERL_VERSION

Available in version 6.48 and above.

The minimum required version of Perl for this distribution.

Either the 5.006001 or the 5.6.1 format is acceptable.

=item MYEXTLIB

If the extension links to a library that it builds, set this to the
name of the library (see SDBM_File)

=item NAME

The package representing the distribution. For example, C<Test::More>
or C<ExtUtils::MakeMaker>. It will be used to derive information about
the distribution such as the L</DISTNAME>, installation locations
within the Perl library and where XS files will be looked for by
default (see L</XS>).

C<NAME> I<must> be a valid Perl package name and it I<must> have an
associated C<.pm> file. For example, C<Foo::Bar> is a valid C<NAME>
and there must exist F<Foo/Bar.pm>.  Any XS code should be in
F<Bar.xs> unless stated otherwise.

Your distribution B<must> have a C<NAME>.

=item NEEDS_LINKING

MakeMaker will figure out if an extension contains linkable code
anywhere down the directory tree, and will set this variable
accordingly, but you can speed it up a very little bit if you define
this boolean variable yourself.

=item NOECHO

Command so make does not print the literal commands it's running.

By setting it to an empty string you can generate a Makefile that
prints all commands. Mainly used in debugging MakeMaker itself.

Defaults to C<@>.

=item NORECURS

Boolean.  Attribute to inhibit descending into subdirectories.

=item NO_META

When true, suppresses the generation and addition to the MANIFEST of
the META.yml and META.json module meta-data files during 'make distdir'.

Defaults to false.

=item NO_MYMETA

Available in version 6.57_02 and above.

When true, suppresses the generation of MYMETA.yml and MYMETA.json module
meta-data files during 'perl Makefile.PL'.

Defaults to false.

=item NO_PACKLIST

Available in version 6.7501 and above.

When true, suppresses the writing of C<packlist> files for installs.

Defaults to false.

=item NO_PERLLOCAL

Available in version 6.7501 and above.

When true, suppresses the appending of installations to C<perllocal>.

Defaults to false.

=item NO_VC

In general, any generated Makefile checks for the current version of
MakeMaker and the version the Makefile was built under. If NO_VC is
set, the version check is neglected. Do not write this into your
Makefile.PL, use it interactively instead.

=item OBJECT

List of object files, defaults to '$(BASEEXT)$(OBJ_EXT)', but can be a long
string or an array containing all object files, e.g. "tkpBind.o
tkpButton.o tkpCanvas.o" or ["tkpBind.o", "tkpButton.o", "tkpCanvas.o"]

(Where BASEEXT is the last component of NAME, and OBJ_EXT is $Config{obj_ext}.)

=item OPTIMIZE

Defaults to C<-O>. Set it to C<-g> to turn debugging on. The flag is
passed to subdirectory makes.

=item PERL

Perl binary for tasks that can be done by miniperl. If it contains
spaces or other shell metacharacters, it needs to be quoted in a way
that protects them, since this value is intended to be inserted in a
shell command line in the Makefile. E.g.:

  # Perl executable lives in "C:/Program Files/Perl/bin"
  # Normally you don't need to set this yourself!
  $ perl Makefile.PL PERL='"C:/Program Files/Perl/bin/perl.exe" -w'

=item PERL_CORE

Set only when MakeMaker is building the extensions of the Perl core
distribution.

=item PERLMAINCC

The call to the program that is able to compile perlmain.c. Defaults
to $(CC).

=item PERL_ARCHLIB

Same as for PERL_LIB, but for architecture dependent files.

Used only when MakeMaker is building the extensions of the Perl core
distribution (because normally $(PERL_ARCHLIB) is automatically in @INC,
and adding it would get in the way of PERL5LIB).

=item PERL_LIB

Directory containing the Perl library to use.

Used only when MakeMaker is building the extensions of the Perl core
distribution (because normally $(PERL_LIB) is automatically in @INC,
and adding it would get in the way of PERL5LIB).

=item PERL_MALLOC_OK

defaults to 0.  Should be set to TRUE if the extension can work with
the memory allocation routines substituted by the Perl malloc() subsystem.
This should be applicable to most extensions with exceptions of those

=over 4

=item *

with bugs in memory allocations which are caught by Perl's malloc();

=item *

which interact with the memory allocator in other ways than via
malloc(), realloc(), free(), calloc(), sbrk() and brk();

=item *

which rely on special alignment which is not provided by Perl's malloc().

=back

B<NOTE.>  Neglecting to set this flag in I<any one> of the loaded extension
nullifies many advantages of Perl's malloc(), such as better usage of
system resources, error detection, memory usage reporting, catchable failure
of memory allocations, etc.

=item PERLPREFIX

Directory under which core modules are to be installed.

Defaults to $Config{installprefixexp}, falling back to
$Config{installprefix}, $Config{prefixexp} or $Config{prefix} should
$Config{installprefixexp} not exist.

Overridden by PREFIX.

=item PERLRUN

Use this instead of $(PERL) when you wish to run perl.  It will set up
extra necessary flags for you.

=item PERLRUNINST

Use this instead of $(PERL) when you wish to run perl to work with
modules.  It will add things like -I$(INST_ARCH) and other necessary
flags so perl can see the modules you're about to install.

=item PERL_SRC

Directory containing the Perl source code (use of this should be
avoided, it may be undefined)

=item PERM_DIR

Available in version 6.51_01 and above.

Desired permission for directories. Defaults to C<755>.

=item PERM_RW

Desired permission for read/writable files. Defaults to C<644>.

=item PERM_RWX

Desired permission for executable files. Defaults to C<755>.

=item PL_FILES

MakeMaker can run programs to generate files for you at build time.
By default any file named *.PL (except Makefile.PL and Build.PL) in
the top level directory will be assumed to be a Perl program and run
passing its own basename in as an argument.  This basename is actually a build
target, and there is an intention, but not a requirement, that the *.PL file
make the file passed to to as an argument. For example...

    perl foo.PL foo

This behavior can be overridden by supplying your own set of files to
search.  PL_FILES accepts a hash ref, the key being the file to run
and the value is passed in as the first argument when the PL file is run.

    PL_FILES => {'bin/foobar.PL' => 'bin/foobar'}

    PL_FILES => {'foo.PL' => 'foo.c'}

Would run bin/foobar.PL like this:

    perl bin/foobar.PL bin/foobar

If multiple files from one program are desired an array ref can be used.

    PL_FILES => {'bin/foobar.PL' => [qw(bin/foobar1 bin/foobar2)]}

In this case the program will be run multiple times using each target file.

    perl bin/foobar.PL bin/foobar1
    perl bin/foobar.PL bin/foobar2

PL files are normally run B<after> pm_to_blib and include INST_LIB and
INST_ARCH in their C<@INC>, so the just built modules can be
accessed... unless the PL file is making a module (or anything else in
PM) in which case it is run B<before> pm_to_blib and does not include
INST_LIB and INST_ARCH in its C<@INC>.  This apparently odd behavior
is there for backwards compatibility (and it's somewhat DWIM).  The argument
passed to the .PL is set up as a target to build in the Makefile.  In other
sections such as C<postamble> you can specify a dependency on the
filename/argument that the .PL is supposed (or will have, now that that is
is a dependency) to generate.  Note the file to be generated will still be
generated and the .PL will still run even without an explicit dependency created
by you, since the C<all> target still depends on running all eligible to run.PL
files.

=item PM

Hashref of .pm files and *.pl files to be installed.  e.g.

  {'name_of_file.pm' => '$(INST_LIB)/install_as.pm'}

By default this will include *.pm and *.pl and the files found in
the PMLIBDIRS directories.  Defining PM in the
Makefile.PL will override PMLIBDIRS.

=item PMLIBDIRS

Ref to array of subdirectories containing library files.  Defaults to
[ 'lib', $(BASEEXT) ]. The directories will be scanned and I<any> files
they contain will be installed in the corresponding location in the
library.  A libscan() method can be used to alter the behaviour.
Defining PM in the Makefile.PL will override PMLIBDIRS.

(Where BASEEXT is the last component of NAME.)

=item PM_FILTER

A filter program, in the traditional Unix sense (input from stdin, output
to stdout) that is passed on each .pm file during the build (in the
pm_to_blib() phase).  It is empty by default, meaning no filtering is done.
You could use:

  PM_FILTER => 'perl -ne "print unless /^\\#/"',

to remove all the leading comments on the fly during the build.  In order
to be as portable as possible, please consider using a Perl one-liner
rather than Unix (or other) utilities, as above.  The # is escaped for
the Makefile, since what is going to be generated will then be:

  PM_FILTER = perl -ne "print unless /^\#/"

Without the \ before the #, we'd have the start of a Makefile comment,
and the macro would be incorrectly defined.

You will almost certainly be better off using the C<PL_FILES> system,
instead. See above, or the L<ExtUtils::MakeMaker::FAQ> entry.

=item POLLUTE

Release 5.005 grandfathered old global symbol names by providing preprocessor
macros for extension source compatibility.  As of release 5.6, these
preprocessor definitions are not available by default.  The POLLUTE flag
specifies that the old names should still be defined:

  perl Makefile.PL POLLUTE=1

Please inform the module author if this is necessary to successfully install
a module under 5.6 or later.

=item PPM_INSTALL_EXEC

Name of the executable used to run C<PPM_INSTALL_SCRIPT> below. (e.g. perl)

=item PPM_INSTALL_SCRIPT

Name of the script that gets executed by the Perl Package Manager after
the installation of a package.

=item PPM_UNINSTALL_EXEC

Available in version 6.8502 and above.

Name of the executable used to run C<PPM_UNINSTALL_SCRIPT> below. (e.g. perl)

=item PPM_UNINSTALL_SCRIPT

Available in version 6.8502 and above.

Name of the script that gets executed by the Perl Package Manager before
the removal of a package.

=item PREFIX

This overrides all the default install locations.  Man pages,
libraries, scripts, etc...  MakeMaker will try to make an educated
guess about where to place things under the new PREFIX based on your
Config defaults.  Failing that, it will fall back to a structure
which should be sensible for your platform.

If you specify LIB or any INSTALL* variables they will not be affected
by the PREFIX.

=item PREREQ_FATAL

Bool. If this parameter is true, failing to have the required modules
(or the right versions thereof) will be fatal. C<perl Makefile.PL>
will C<die> instead of simply informing the user of the missing dependencies.

It is I<extremely> rare to have to use C<PREREQ_FATAL>. Its use by module
authors is I<strongly discouraged> and should never be used lightly.

For dependencies that are required in order to run C<Makefile.PL>,
see C<CONFIGURE_REQUIRES>.

Module installation tools have ways of resolving unmet dependencies but
to do that they need a F<Makefile>.  Using C<PREREQ_FATAL> breaks this.
That's bad.

Assuming you have good test coverage, your tests should fail with
missing dependencies informing the user more strongly that something
is wrong.  You can write a F<t/00compile.t> test which will simply
check that your code compiles and stop "make test" prematurely if it
doesn't.  See L<Test::More/BAIL_OUT> for more details.


=item PREREQ_PM

A hash of modules that are needed to run your module.  The keys are
the module names ie. Test::More, and the minimum version is the
value. If the required version number is 0 any version will do.
The versions given may be a Perl v-string (see L<version>) or a range
(see L<CPAN::Meta::Requirements>).

This will go into the C<requires> field of your F<META.yml> and the
C<runtime> of the C<prereqs> field of your F<META.json>.

    PREREQ_PM => {
        # Require Test::More at least 0.47
        "Test::More" => "0.47",

        # Require any version of Acme::Buffy
        "Acme::Buffy" => 0,
    }

=item PREREQ_PRINT

Bool.  If this parameter is true, the prerequisites will be printed to
stdout and MakeMaker will exit.  The output format is an evalable hash
ref.

  $PREREQ_PM = {
                 'A::B' => Vers1,
                 'C::D' => Vers2,
                 ...
               };

If a distribution defines a minimal required perl version, this is
added to the output as an additional line of the form:

  $MIN_PERL_VERSION = '5.008001';

If BUILD_REQUIRES is not empty, it will be dumped as $BUILD_REQUIRES hashref.

=item PRINT_PREREQ

RedHatism for C<PREREQ_PRINT>.  The output format is different, though:

    perl(A::B)>=Vers1 perl(C::D)>=Vers2 ...

A minimal required perl version, if present, will look like this:

    perl(perl)>=5.008001

=item SITEPREFIX

Like PERLPREFIX, but only for the site install locations.

Defaults to $Config{siteprefixexp}.  Perls prior to 5.6.0 didn't have
an explicit siteprefix in the Config.  In those cases
$Config{installprefix} will be used.

Overridable by PREFIX

=item SIGN

Available in version 6.18 and above.

When true, perform the generation and addition to the MANIFEST of the
SIGNATURE file in the distdir during 'make distdir', via 'cpansign
-s'.

Note that you need to install the Module::Signature module to
perform this operation.

Defaults to false.

=item SKIP

Arrayref. E.g. [qw(name1 name2)] skip (do not write) sections of the
Makefile. Caution! Do not use the SKIP attribute for the negligible
speedup. It may seriously damage the resulting Makefile. Only use it
if you really need it.

=item TEST_REQUIRES

Available in version 6.64 and above.

A hash of modules that are needed to test your module but not run or
build it.

This will go into the C<build_requires> field of your F<META.yml> and the C<test> of the C<prereqs> field of your F<META.json>.

The format is the same as PREREQ_PM.

=item TYPEMAPS

Ref to array of typemap file names.  Use this when the typemaps are
in some directory other than the current directory or when they are
not named B<typemap>.  The last typemap in the list takes
precedence.  A typemap in the current directory has highest
precedence, even if it isn't listed in TYPEMAPS.  The default system
typemap has lowest precedence.

=item VENDORPREFIX

Like PERLPREFIX, but only for the vendor install locations.

Defaults to $Config{vendorprefixexp}.

Overridable by PREFIX

=item VERBINST

If true, make install will be verbose

=item VERSION

Your version number for distributing the package.  This defaults to
0.1.

=item VERSION_FROM

Instead of specifying the VERSION in the Makefile.PL you can let
MakeMaker parse a file to determine the version number. The parsing
routine requires that the file named by VERSION_FROM contains one
single line to compute the version number. The first line in the file
that contains something like a $VERSION assignment or C<package Name
VERSION> will be used. The following lines will be parsed o.k.:

    # Good
    package Foo::Bar 1.23;                      # 1.23
    $VERSION   = '1.00';                        # 1.00
    *VERSION   = \'1.01';                       # 1.01
    ($VERSION) = q$Revision$ =~ /(\d+)/g;       # The digits in $Revision$
    $FOO::VERSION = '1.10';                     # 1.10
    *FOO::VERSION = \'1.11';                    # 1.11

but these will fail:

    # Bad
    my $VERSION         = '1.01';
    local $VERSION      = '1.02';
    local $FOO::VERSION = '1.30';

(Putting C<my> or C<local> on the preceding line will work o.k.)

"Version strings" are incompatible and should not be used.

    # Bad
    $VERSION = 1.2.3;
    $VERSION = v1.2.3;

L<version> objects are fine.  As of MakeMaker 6.35 version.pm will be
automatically loaded, but you must declare the dependency on version.pm.
For compatibility with older MakeMaker you should load on the same line
as $VERSION is declared.

    # All on one line
    use version; our $VERSION = qv(1.2.3);

The file named in VERSION_FROM is not added as a dependency to
Makefile. This is not really correct, but it would be a major pain
during development to have to rewrite the Makefile for any smallish
change in that file. If you want to make sure that the Makefile
contains the correct VERSION macro after any change of the file, you
would have to do something like

    depend => { Makefile => '$(VERSION_FROM)' }

See attribute C<depend> below.

=item VERSION_SYM

A sanitized VERSION with . replaced by _.  For places where . has
special meaning (some filesystems, RCS labels, etc...)

=item XS

Hashref of .xs files. MakeMaker will default this.  e.g.

  {'name_of_file.xs' => 'name_of_file.c'}

The .c files will automatically be included in the list of files
deleted by a make clean.

=item XSBUILD

Available in version 7.12 and above.

Hashref with options controlling the operation of C<XSMULTI>:

  {
    xs => {
        all => {
            # options applying to all .xs files for this distribution
        },
        'lib/Class/Name/File' => { # specifically for this file
            DEFINE => '-Dfunktastic', # defines for only this file
            INC => "-I$funkyliblocation", # include flags for only this file
            # OBJECT => 'lib/Class/Name/File$(OBJ_EXT)', # default
            LDFROM => "lib/Class/Name/File\$(OBJ_EXT) $otherfile\$(OBJ_EXT)", # what's linked
        },
    },
  }

Note C<xs> is the file-extension. More possibilities may arise in the
future. Note that object names are specified without their XS extension.

C<LDFROM> defaults to the same as C<OBJECT>. C<OBJECT> defaults to,
for C<XSMULTI>, just the XS filename with the extension replaced with
the compiler-specific object-file extension.

The distinction between C<OBJECT> and C<LDFROM>: C<OBJECT> is the make
target, so make will try to build it. However, C<LDFROM> is what will
actually be linked together to make the shared object or static library
(SO/SL), so if you override it, make sure it includes what you want to
make the final SO/SL, almost certainly including the XS basename with
C<$(OBJ_EXT)> appended.

=item XSMULTI

Available in version 7.12 and above.

When this is set to C<1>, multiple XS files may be placed under F<lib/>
next to their corresponding C<*.pm> files (this is essential for compiling
with the correct C<VERSION> values). This feature should be considered
experimental, and details of it may change.

This feature was inspired by, and small portions of code copied from,
L<ExtUtils::MakeMaker::BigHelper>. Hopefully this feature will render
that module mainly obsolete.

=item XSOPT

String of options to pass to xsubpp.  This might include C<-C++> or
C<-extern>.  Do not include typemaps here; the TYPEMAP parameter exists for
that purpose.

=item XSPROTOARG

May be set to C<-protoypes>, C<-noprototypes> or the empty string.  The
empty string is equivalent to the xsubpp default, or C<-noprototypes>.
See the xsubpp documentation for details.  MakeMaker
defaults to the empty string.

=item XS_VERSION

Your version number for the .xs file of this package.  This defaults
to the value of the VERSION attribute.

=back

=head2 Additional lowercase attributes

can be used to pass parameters to the methods which implement that
part of the Makefile.  Parameters are specified as a hash ref but are
passed to the method as a hash.

=over 2

=item clean

  {FILES => "*.xyz foo"}

=item depend

  {ANY_TARGET => ANY_DEPENDENCY, ...}

(ANY_TARGET must not be given a double-colon rule by MakeMaker.)

=item dist

  {TARFLAGS => 'cvfF', COMPRESS => 'gzip', SUFFIX => '.gz',
  SHAR => 'shar -m', DIST_CP => 'ln', ZIP => '/bin/zip',
  ZIPFLAGS => '-rl', DIST_DEFAULT => 'private tardist' }

If you specify COMPRESS, then SUFFIX should also be altered, as it is
needed to tell make the target file of the compression. Setting
DIST_CP to ln can be useful, if you need to preserve the timestamps on
your files. DIST_CP can take the values 'cp', which copies the file,
'ln', which links the file, and 'best' which copies symbolic links and
links the rest. Default is 'best'.

=item dynamic_lib

  {ARMAYBE => 'ar', OTHERLDFLAGS => '...', INST_DYNAMIC_DEP => '...'}

=item linkext

  {LINKTYPE => 'static', 'dynamic' or ''}

NB: Extensions that have nothing but *.pm files had to say

  {LINKTYPE => ''}

with Pre-5.0 MakeMakers. Since version 5.00 of MakeMaker such a line
can be deleted safely. MakeMaker recognizes when there's nothing to
be linked.

=item macro

  {ANY_MACRO => ANY_VALUE, ...}

=item postamble

Anything put here will be passed to MY::postamble() if you have one.

=item realclean

  {FILES => '$(INST_ARCHAUTODIR)/*.xyz'}

=item test

Specify the targets for testing.

  {TESTS => 't/*.t'}

C<RECURSIVE_TEST_FILES> can be used to include all directories
recursively under C<t> that contain C<.t> files. It will be ignored if
you provide your own C<TESTS> attribute, defaults to false.

  {RECURSIVE_TEST_FILES=>1}

This is supported since 6.76

=item tool_autosplit

  {MAXLEN => 8}

=back

=head2 Overriding MakeMaker Methods

If you cannot achieve the desired Makefile behaviour by specifying
attributes you may define private subroutines in the Makefile.PL.
Each subroutine returns the text it wishes to have written to
the Makefile. To override a section of the Makefile you can
either say:

        sub MY::c_o { "new literal text" }

or you can edit the default by saying something like:

        package MY; # so that "SUPER" works right
        sub c_o {
            my $inherited = shift->SUPER::c_o(@_);
            $inherited =~ s/old text/new text/;
            $inherited;
        }

If you are running experiments with embedding perl as a library into
other applications, you might find MakeMaker is not sufficient. You'd
better have a look at ExtUtils::Embed which is a collection of utilities
for embedding.

If you still need a different solution, try to develop another
subroutine that fits your needs and submit the diffs to
C<makemaker@perl.org>

For a complete description of all MakeMaker methods see
L<ExtUtils::MM_Unix>.

Here is a simple example of how to add a new target to the generated
Makefile:

    sub MY::postamble {
        return <<'MAKE_FRAG';
    $(MYEXTLIB): sdbm/Makefile
            cd sdbm && $(MAKE) all

    MAKE_FRAG
    }

=head2 The End Of Cargo Cult Programming

WriteMakefile() now does some basic sanity checks on its parameters to
protect against typos and malformatted values.  This means some things
which happened to work in the past will now throw warnings and
possibly produce internal errors.

Some of the most common mistakes:

=over 2

=item C<< MAN3PODS => ' ' >>

This is commonly used to suppress the creation of man pages.  MAN3PODS
takes a hash ref not a string, but the above worked by accident in old
versions of MakeMaker.

The correct code is C<< MAN3PODS => { } >>.

=back


=head2 Hintsfile support

MakeMaker.pm uses the architecture-specific information from
Config.pm. In addition it evaluates architecture specific hints files
in a C<hints/> directory. The hints files are expected to be named
like their counterparts in C<PERL_SRC/hints>, but with an C<.pl> file
name extension (eg. C<next_3_2.pl>). They are simply C<eval>ed by
MakeMaker within the WriteMakefile() subroutine, and can be used to
execute commands as well as to include special variables. The rules
which hintsfile is chosen are the same as in Configure.

The hintsfile is eval()ed immediately after the arguments given to
WriteMakefile are stuffed into a hash reference $self but before this
reference becomes blessed. So if you want to do the equivalent to
override or create an attribute you would say something like

    $self->{LIBS} = ['-ldbm -lucb -lc'];

=head2 Distribution Support

For authors of extensions MakeMaker provides several Makefile
targets. Most of the support comes from the ExtUtils::Manifest module,
where additional documentation can be found.

=over 4

=item    make distcheck

reports which files are below the build directory but not in the
MANIFEST file and vice versa. (See ExtUtils::Manifest::fullcheck() for
details)

=item    make skipcheck

reports which files are skipped due to the entries in the
C<MANIFEST.SKIP> file (See ExtUtils::Manifest::skipcheck() for
details)

=item    make distclean

does a realclean first and then the distcheck. Note that this is not
needed to build a new distribution as long as you are sure that the
MANIFEST file is ok.

=item    make veryclean

does a realclean first and then removes backup files such as C<*~>,
C<*.bak>, C<*.old> and C<*.orig>

=item    make manifest

rewrites the MANIFEST file, adding all remaining files found (See
ExtUtils::Manifest::mkmanifest() for details)

=item    make distdir

Copies all the files that are in the MANIFEST file to a newly created
directory with the name C<$(DISTNAME)-$(VERSION)>. If that directory
exists, it will be removed first.

Additionally, it will create META.yml and META.json module meta-data file
in the distdir and add this to the distdir's MANIFEST.  You can shut this
behavior off with the NO_META flag.

=item   make disttest

Makes a distdir first, and runs a C<perl Makefile.PL>, a make, and
a make test in that directory.

=item    make tardist

First does a distdir. Then a command $(PREOP) which defaults to a null
command, followed by $(TO_UNIX), which defaults to a null command under
UNIX, and will convert files in distribution directory to UNIX format
otherwise. Next it runs C<tar> on that directory into a tarfile and
deletes the directory. Finishes with a command $(POSTOP) which
defaults to a null command.

=item    make dist

Defaults to $(DIST_DEFAULT) which in turn defaults to tardist.

=item    make uutardist

Runs a tardist first and uuencodes the tarfile.

=item    make shdist

First does a distdir. Then a command $(PREOP) which defaults to a null
command. Next it runs C<shar> on that directory into a sharfile and
deletes the intermediate directory again. Finishes with a command
$(POSTOP) which defaults to a null command.  Note: For shdist to work
properly a C<shar> program that can handle directories is mandatory.

=item    make zipdist

First does a distdir. Then a command $(PREOP) which defaults to a null
command. Runs C<$(ZIP) $(ZIPFLAGS)> on that directory into a
zipfile. Then deletes that directory. Finishes with a command
$(POSTOP) which defaults to a null command.

=item    make ci

Does a $(CI) and a $(RCS_LABEL) on all files in the MANIFEST file.

=back

Customization of the dist targets can be done by specifying a hash
reference to the dist attribute of the WriteMakefile call. The
following parameters are recognized:

    CI           ('ci -u')
    COMPRESS     ('gzip --best')
    POSTOP       ('@ :')
    PREOP        ('@ :')
    TO_UNIX      (depends on the system)
    RCS_LABEL    ('rcs -q -Nv$(VERSION_SYM):')
    SHAR         ('shar')
    SUFFIX       ('.gz')
    TAR          ('tar')
    TARFLAGS     ('cvf')
    ZIP          ('zip')
    ZIPFLAGS     ('-r')

An example:

    WriteMakefile(
        ...other options...
        dist => {
            COMPRESS => "bzip2",
            SUFFIX   => ".bz2"
        }
    );


=head2 Module Meta-Data (META and MYMETA)

Long plaguing users of MakeMaker based modules has been the problem of
getting basic information about the module out of the sources
I<without> running the F<Makefile.PL> and doing a bunch of messy
heuristics on the resulting F<Makefile>.  Over the years, it has become
standard to keep this information in one or more CPAN Meta files
distributed with each distribution.

The original format of CPAN Meta files was L<YAML> and the corresponding
file was called F<META.yml>.  In 2010, version 2 of the L<CPAN::Meta::Spec>
was released, which mandates JSON format for the metadata in order to
overcome certain compatibility issues between YAML serializers and to
avoid breaking older clients unable to handle a new version of the spec.
The L<CPAN::Meta> library is now standard for accessing old and new-style
Meta files.

If L<CPAN::Meta> is installed, MakeMaker will automatically generate
F<META.json> and F<META.yml> files for you and add them to your F<MANIFEST> as
part of the 'distdir' target (and thus the 'dist' target).  This is intended to
seamlessly and rapidly populate CPAN with module meta-data.  If you wish to
shut this feature off, set the C<NO_META> C<WriteMakefile()> flag to true.

At the 2008 QA Hackathon in Oslo, Perl module toolchain maintainers agreed
to use the CPAN Meta format to communicate post-configuration requirements
between toolchain components.  These files, F<MYMETA.json> and F<MYMETA.yml>,
are generated when F<Makefile.PL> generates a F<Makefile> (if L<CPAN::Meta>
is installed).  Clients like L<CPAN> or L<CPANPLUS> will read these
files to see what prerequisites must be fulfilled before building or testing
the distribution.  If you wish to shut this feature off, set the C<NO_MYMETA>
C<WriteMakeFile()> flag to true.

=head2 Disabling an extension

If some events detected in F<Makefile.PL> imply that there is no way
to create the Module, but this is a normal state of things, then you
can create a F<Makefile> which does nothing, but succeeds on all the
"usual" build targets.  To do so, use

    use ExtUtils::MakeMaker qw(WriteEmptyMakefile);
    WriteEmptyMakefile();

instead of WriteMakefile().

This may be useful if other modules expect this module to be I<built>
OK, as opposed to I<work> OK (say, this system-dependent module builds
in a subdirectory of some other distribution, or is listed as a
dependency in a CPAN::Bundle, but the functionality is supported by
different means on the current architecture).

=head2 Other Handy Functions

=over 4

=item prompt

    my $value = prompt($message);
    my $value = prompt($message, $default);

The C<prompt()> function provides an easy way to request user input
used to write a makefile.  It displays the $message as a prompt for
input.  If a $default is provided it will be used as a default.  The
function returns the $value selected by the user.

If C<prompt()> detects that it is not running interactively and there
is nothing on STDIN or if the PERL_MM_USE_DEFAULT environment variable
is set to true, the $default will be used without prompting.  This
prevents automated processes from blocking on user input.

If no $default is provided an empty string will be used instead.

=item os_unsupported

  os_unsupported();
  os_unsupported if $^O eq 'MSWin32';

The C<os_unsupported()> function provides a way to correctly exit your
C<Makefile.PL> before calling C<WriteMakefile>. It is essentially a
C<die> with the message "OS unsupported".

This is supported since 7.26

=back

=head2 Supported versions of Perl

Please note that while this module works on Perl 5.6, it is no longer
being routinely tested on 5.6 - the earliest Perl version being routinely
tested, and expressly supported, is 5.8.1. However, patches to repair
any breakage on 5.6 are still being accepted.

=head1 ENVIRONMENT

=over 4

=item PERL_MM_OPT

Command line options used by C<MakeMaker-E<gt>new()>, and thus by
C<WriteMakefile()>.  The string is split as the shell would, and the result
is processed before any actual command line arguments are processed.

  PERL_MM_OPT='CCFLAGS="-Wl,-rpath -Wl,/foo/bar/lib" LIBS="-lwibble -lwobble"'

=item PERL_MM_USE_DEFAULT

If set to a true value then MakeMaker's prompt function will
always return the default without waiting for user input.

=item PERL_CORE

Same as the PERL_CORE parameter.  The parameter overrides this.

=back

=head1 SEE ALSO

L<Module::Build> is a pure-Perl alternative to MakeMaker which does
not rely on make or any other external utility.  It is easier to
extend to suit your needs.

L<Module::Install> is a wrapper around MakeMaker which adds features
not normally available.

L<ExtUtils::ModuleMaker> and L<Module::Starter> are both modules to
help you setup your distribution.

L<CPAN::Meta> and L<CPAN::Meta::Spec> explain CPAN Meta files in detail.

L<File::ShareDir::Install> makes it easy to install static, sometimes
also referred to as 'shared' files. L<File::ShareDir> helps accessing
the shared files after installation.

L<Dist::Zilla> makes it easy for the module author to create MakeMaker-based
distributions with lots of bells and whistles.

=head1 AUTHORS

Andy Dougherty C<doughera@lafayette.edu>, Andreas KE<ouml>nig
C<andreas.koenig@mind.de>, Tim Bunce C<timb@cpan.org>.  VMS
support by Charles Bailey C<bailey@newman.upenn.edu>.  OS/2 support
by Ilya Zakharevich C<ilya@math.ohio-state.edu>.

Currently maintained by Michael G Schwern C<schwern@pobox.com>

Send patches and ideas to C<makemaker@perl.org>.

Send bug reports via http://rt.cpan.org/.  Please send your
generated Makefile along with your report.

For more up-to-date information, see L<https://metacpan.org/release/ExtUtils-MakeMaker>.

Repository available at L<https://github.com/Perl-Toolchain-Gang/ExtUtils-MakeMaker>.

=head1 LICENSE

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>


=cut
