# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Install::CPAN;

use 5.10.1;
use strict;
use warnings;

use parent qw(Exporter);
our @EXPORT = qw(
    BZ_LIB

    check_cpan_requirements 
    set_cpan_config
    install_module 
);

use Bugzilla::Constants;
use Bugzilla::Install::Requirements qw(have_vers);
use Bugzilla::Install::Util qw(bin_loc install_string);

use Config;
use CPAN;
use Cwd qw(abs_path);
use File::Path qw(rmtree);

# These are required for install-module.pl to be able to install
# all modules properly.
use constant REQUIREMENTS => (
    {
        module  => 'CPAN',
        package => 'CPAN',
        version => '1.81',
    },
    {
        # When Module::Build isn't installed, the YAML module allows
        # CPAN to read META.yml to determine that Module::Build first
        # needs to be installed to compile a module.
        module  => 'YAML',
        package => 'YAML',
        version => 0,
    },
    {
        # Many modules on CPAN are now built with Dist::Zilla, which
        # unfortunately means they require this version of EU::MM to install.
        module  => 'ExtUtils::MakeMaker',
        package => 'ExtUtils-MakeMaker',
        version => '6.31',
    },
);

# We need the absolute path of ext_libpath, because CPAN chdirs around
# and so we can't use a relative directory.
#
# We need it often enough (and at compile time, in install-module.pl) so 
# we make it a constant.
use constant BZ_LIB => abs_path(bz_locations()->{ext_libpath});

# CPAN requires nearly all of its parameters to be set, or it will start
# asking questions to the user. We want to avoid that, so we have
# defaults here for most of the required parameters we know about, in case
# any of them aren't set. The rest are handled by set_cpan_defaults().
use constant CPAN_DEFAULTS => {
    auto_commit => 0,
    # We always force builds, so there's no reason to cache them.
    build_cache => 0,
    build_requires_install_policy => 'yes',
    cache_metadata => 1,
    colorize_output => 1,
    colorize_print => 'bold',
    index_expire => 1,
    scan_cache => 'atstart',

    inhibit_startup_message => 1,

    bzip2 => bin_loc('bzip2'),
    curl => bin_loc('curl'),
    gzip => bin_loc('gzip'),
    links => bin_loc('links'),
    lynx => bin_loc('lynx'),
    make => bin_loc('make'),
    pager => bin_loc('less'),
    tar => bin_loc('tar'),
    unzip => bin_loc('unzip'),
    wget => bin_loc('wget'),

    urllist => ['http://www.cpan.org/'],
};

sub check_cpan_requirements {
    my ($original_dir, $original_args) = @_;

    _require_compiler();

    my @install;
    foreach my $module (REQUIREMENTS) {
        my $installed = have_vers($module, 1);
        push(@install, $module) if !$installed;
    }

    return if !@install;

    my $restart_required;
    foreach my $module (@install) {
        $restart_required = 1 if $module->{module} eq 'CPAN';
        install_module($module->{module}, 1);
    }

    if ($restart_required) {
        chdir $original_dir;
        exec($^X, $0, @$original_args);
    }
}

sub _require_compiler {
    my @errors;

    my $cc_name = $Config{cc};
    my $cc_exists = bin_loc($cc_name);

    if (!$cc_exists) {
        push(@errors, install_string('install_no_compiler'));
    }

    my $make_name = $CPAN::Config->{make};
    my $make_exists = bin_loc($make_name);

    if (!$make_exists) {
        push(@errors, install_string('install_no_make'));
    }

    die @errors if @errors;
}

sub install_module {
    my ($name, $test) = @_;
    my $bzlib = BZ_LIB;

    # Make Module::AutoInstall install all dependencies and never prompt.
    local $ENV{PERL_AUTOINSTALL} = '--alldeps';
    # This makes Net::SSLeay not prompt the user, if it gets installed.
    # It also makes any other MakeMaker prompts accept their defaults.
    local $ENV{PERL_MM_USE_DEFAULT} = 1;

    # Certain modules require special stuff in order to not prompt us.
    my $original_makepl = $CPAN::Config->{makepl_arg};
    # This one's a regex in case we're doing Template::Plugin::GD and it
    # pulls in Template-Toolkit as a dependency.
    if ($name =~ /^Template/) {
        $CPAN::Config->{makepl_arg} .= " TT_ACCEPT=y TT_EXTRAS=n";
    }
    elsif ($name eq 'XML::Twig') {
        $CPAN::Config->{makepl_arg} = "-n $original_makepl";
    }
    elsif ($name eq 'SOAP::Lite') {
        $CPAN::Config->{makepl_arg} .= " --noprompt";
    }

    my $module = CPAN::Shell->expand('Module', $name);
    if (!$module) {
        die install_string('no_such_module', { module => $name }) . "\n";
    }

    print install_string('install_module', 
              { module => $name, version => $module->cpan_version }) . "\n";

    if ($test) {
        CPAN::Shell->force('install', $name);
    }
    else {
        CPAN::Shell->notest('install', $name);
    }

    # If it installed any binaries in the Bugzilla directory, delete them.
    if (-d "$bzlib/bin") {
        File::Path::rmtree("$bzlib/bin");
    }

    $CPAN::Config->{makepl_arg} = $original_makepl;
}

sub set_cpan_config {
    my $do_global = shift;
    my $bzlib = BZ_LIB;

    # We set defaults before we do anything, otherwise CPAN will
    # start asking us questions as soon as we load its configuration.
    eval { require CPAN::Config; };
    _set_cpan_defaults();

    # Calling a senseless autoload that does nothing makes us
    # automatically load any existing configuration.
    # We want to avoid the "invalid command" message.
    open(my $saveout, ">&", "STDOUT");
    open(STDOUT, '>', '/dev/null');
    eval { CPAN->ignore_this_error_message_from_bugzilla; };
    undef $@;
    close(STDOUT);
    open(STDOUT, '>&', $saveout);

    my $dir = $CPAN::Config->{cpan_home};
    if (!defined $dir || !-w $dir) {
        # If we can't use the standard CPAN build dir, we try to make one.
        $dir = "$ENV{HOME}/.cpan";
        mkdir $dir;

        # If we can't make one, we finally try to use the Bugzilla directory.
        if (!-w $dir) {
            print STDERR install_string('cpan_bugzilla_home'), "\n";
            $dir = "$bzlib/.cpan";
        }
    }
    $CPAN::Config->{cpan_home} = $dir;
    $CPAN::Config->{build_dir} = "$dir/build";
    # We always force builds, so there's no reason to cache them.
    $CPAN::Config->{keep_source_where} = "$dir/source";
    # This is set both here and in defaults so that it's always true.
    $CPAN::Config->{inhibit_startup_message} = 1;
    # Automatically install dependencies.
    $CPAN::Config->{prerequisites_policy} = 'follow';
    
    # Unless specified, we install the modules into the Bugzilla directory.
    if (!$do_global) {
        require Config;

        $CPAN::Config->{makepl_arg} .= " LIB=\"$bzlib\""
            . " INSTALLMAN1DIR=\"$bzlib/man/man1\""
            . " INSTALLMAN3DIR=\"$bzlib/man/man3\""
            # The bindirs are here because otherwise we'll try to write to
            # the system binary dirs, and that will cause CPAN to die.
            . " INSTALLBIN=\"$bzlib/bin\""
            . " INSTALLSCRIPT=\"$bzlib/bin\""
            # INSTALLDIRS=perl is set because that makes sure that MakeMaker
            # always uses the directories we've specified here.
            . " INSTALLDIRS=perl";
        $CPAN::Config->{mbuild_arg} = " --install_base \"$bzlib\""
            . " --install_path lib=\"$bzlib\""
            . " --install_path arch=\"$bzlib/$Config::Config{archname}\"";
        $CPAN::Config->{mbuild_install_arg} = $CPAN::Config->{mbuild_arg};

        # When we're not root, sometimes newer versions of CPAN will
        # try to read/modify things that belong to root, unless we set
        # certain config variables.
        $CPAN::Config->{histfile} = "$dir/histfile";
        $CPAN::Config->{use_sqlite} = 0;
        $CPAN::Config->{prefs_dir} = "$dir/prefs";

        # Unless we actually set PERL5LIB, some modules can't install
        # themselves, like DBD::mysql, DBD::Pg, and XML::Twig.
        my $current_lib = $ENV{PERL5LIB} ? $ENV{PERL5LIB} . ':' : '';
        $ENV{PERL5LIB} = $current_lib . $bzlib;
    }
}

sub _set_cpan_defaults {
    # If CPAN hasn't been configured, we try to use some reasonable defaults.
    foreach my $key (keys %{CPAN_DEFAULTS()}) {
        $CPAN::Config->{$key} = CPAN_DEFAULTS->{$key}
            if !defined $CPAN::Config->{$key};
    }

    my @missing;
    # In newer CPANs, this is in HandleConfig. In older CPANs, it's in
    # Config.
    if (eval { require CPAN::HandleConfig }) {
        @missing = CPAN::HandleConfig->missing_config_data;
    }
    else {
        @missing = CPAN::Config->missing_config_data;
    }

    foreach my $key (@missing) {
        $CPAN::Config->{$key} = '';
    }
}

1;

__END__

=head1 NAME

Bugzilla::Install::CPAN - Routines to install Perl modules from CPAN.

=head1 SYNOPSIS

 use Bugzilla::Install::CPAN;

 set_cpan_config();
 install_module('Module::Name');

=head1 DESCRIPTION

This is primarily used by L<install-module> to do the "hard work" of
installing CPAN modules.

=head1 SUBROUTINES

=over

=item C<set_cpan_config>

Sets up the configuration of CPAN for this session. Must be called
before L</install_module>. Takes one boolean parameter. If true,
L</install_module> will install modules globally instead of to the
local F<lib/> directory. On most systems, you have to be root to do that.

=item C<install_module>

Installs a module from CPAN. Takes two arguments:

=over

=item C<$name> - The name of the module, just like you'd pass to the
C<install> command in the CPAN shell.

=item C<$test> - If true, we run tests on this module before installing,
but we still force the install if the tests fail. This is only used
when we internally install a newer CPAN module.

=back

Note that calling this function prints a B<lot> of information to
STDOUT and STDERR.

=back

=head1 B<Methods in need of POD>

=over

=item check_cpan_requirements

=back
