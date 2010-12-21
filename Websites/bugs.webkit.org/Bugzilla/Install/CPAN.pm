# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Everything Solved, Inc.
# Portions created by Everything Solved are Copyright (C) 2007
# Everything Solved, Inc. All Rights Reserved.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::Install::CPAN;
use strict;
use base qw(Exporter);
our @EXPORT = qw(set_cpan_config install_module BZ_LIB);

use Bugzilla::Constants;
use Bugzilla::Install::Util qw(bin_loc install_string);

use CPAN;
use Cwd qw(abs_path);
use File::Path qw(rmtree);
use List::Util qw(shuffle);

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
    cache_metadata => 1,
    index_expire => 1,
    scan_cache => 'atstart',

    inhibit_startup_message => 1,
    mbuild_install_build_command => './Build',

    curl => bin_loc('curl'),
    gzip => bin_loc('gzip'),
    links => bin_loc('links'),
    lynx => bin_loc('lynx'),
    make => bin_loc('make'),
    pager => bin_loc('less'),
    tar => bin_loc('tar'),
    unzip => bin_loc('unzip'),
    wget => bin_loc('wget'),

    urllist => [shuffle qw(
        http://cpan.pair.com/
        http://mirror.hiwaay.net/CPAN/
        ftp://ftp.dc.aleron.net/pub/CPAN/
        http://perl.secsup.org/
        http://mirrors.kernel.org/cpan/)],
};

sub install_module {
    my ($name, $notest) = @_;
    my $bzlib = BZ_LIB;

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
    elsif ($name eq 'Net::LDAP') {
        $CPAN::Config->{makepl_arg} .= " --skipdeps";
    }
    elsif ($name eq 'SOAP::Lite') {
        $CPAN::Config->{makepl_arg} .= " --noprompt";
    }

    my $module = CPAN::Shell->expand('Module', $name);
    print install_string('install_module', 
              { module => $name, version => $module->cpan_version }) . "\n";
    if ($notest) {
        CPAN::Shell->notest('install', $name);
    }
    else {
        CPAN::Shell->force('install', $name);
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
    open(my $saveout, ">&STDOUT");
    open(STDOUT, '>/dev/null');
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
            print "WARNING: Using the Bugzilla directory as the CPAN home.\n";
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
        $CPAN::Config->{mbuild_arg} = "--install_base \"$bzlib\"";

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
 install_module('Module::Name', 1);

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

=item C<$notest> - If true, we skip running tests on this module. This
can greatly speed up the installation time.

=back

Note that calling this function prints a B<lot> of information to
STDOUT and STDERR.

=back
