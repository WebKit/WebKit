#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use 5.10.1;
use strict;
use warnings;

# Have to abs_path('.') or calls to Bugzilla modules won't work once
# CPAN has chdir'ed around. We do all of this in this funny order to
# make sure that we use the lib/ modules instead of the base Perl modules,
# in case the lib/ modules are newer.
use Cwd qw(abs_path cwd);
use lib abs_path('.');
use Bugzilla::Constants;
use lib abs_path(bz_locations()->{ext_libpath});

use Bugzilla::Install::CPAN;

use Bugzilla::Constants;
use Bugzilla::Install::Requirements;
use Bugzilla::Install::Util qw(bin_loc init_console);

use Data::Dumper;
use Getopt::Long;
use Pod::Usage;

init_console();

my @original_args = @ARGV;
my $original_dir = cwd();
our %switch;
GetOptions(\%switch, 'all|a', 'upgrade-all|u', 'show-config|s', 'global|g',
                     'shell', 'help|h');

pod2usage({ -verbose => 1 }) if $switch{'help'};

if (ON_ACTIVESTATE) {
    print <<END;
You cannot run this script when using ActiveState Perl. Please follow
the instructions given by checksetup.pl to install missing Perl modules.

END
    exit;
}

pod2usage({ -verbose => 0 }) if (!%switch && !@ARGV);

set_cpan_config($switch{'global'});

if ($switch{'show-config'}) {
    print Dumper($CPAN::Config);
    exit;
}

check_cpan_requirements($original_dir, \@original_args);

if ($switch{'shell'}) {
    CPAN::shell();
    exit;
}

if ($switch{'all'} || $switch{'upgrade-all'}) {
    my @modules;
    if ($switch{'upgrade-all'}) {
        @modules = (@{REQUIRED_MODULES()}, @{OPTIONAL_MODULES()});
        push(@modules, DB_MODULE->{$_}->{dbd}) foreach (keys %{DB_MODULE()});
    }
    else {
        # This is the only time we need a Bugzilla-related module, so
        # we require them down here. Otherwise this script can be run from
        # any directory, even outside of Bugzilla itself.
        my $reqs = check_requirements(0);
        @modules = (@{$reqs->{missing}}, @{$reqs->{optional}});
        my $dbs = DB_MODULE;
        foreach my $db (keys %$dbs) {
            push(@modules, $dbs->{$db}->{dbd})
                if !have_vers($dbs->{$db}->{dbd}, 0);
        }
    }
    foreach my $module (@modules) {
        my $cpan_name = $module->{module};
        # --all shouldn't include mod_perl2, because it can have some complex
        # configuration, and really should be installed on its own.
        next if $cpan_name eq 'mod_perl2';
        next if $cpan_name eq 'DBD::Oracle' and !$ENV{ORACLE_HOME};
        next if $cpan_name eq 'DBD::Pg' and !bin_loc('pg_config');
        install_module($cpan_name);
    }
}

foreach my $module (@ARGV) {
    install_module($module);
}

__END__

=head1 NAME

install-module.pl - Installs or upgrades modules from CPAN.
This script does not run on Windows.

=head1 SYNOPSIS

  ./install-module.pl Module::Name [--global]
  ./install-module.pl --all [--global]
  ./install-module.pl --upgrade-all [--global]
  ./install-module.pl --show-config
  ./install-module.pl --shell

  Do "./install-module.pl --help" for more information.

=head1 OPTIONS

=over

=item B<Module::Name>

The name of a module that you want to install from CPAN. This is the
same thing that you'd give to the C<install> command in the CPAN shell.

You can specify multiple module names separated by a space to install
multiple modules.

=item B<--global>

This makes install-module install modules globally for all applications,
instead of just for Bugzilla.

On most systems, you have to be root for C<--global> to work.

=item B<--all>

This will make install-module do its best to install every required
and optional module that is not installed that Bugzilla can use.

Some modules may fail to install. You can run checksetup.pl to see
which installed properly.

=item B<--upgrade-all>

This is like C<--all>, except it forcibly installs the very latest
version of every Bugzilla prerequisite, whether or not you already
have them installed.

=item B<--show-config>

Prints out the CPAN configuration in raw Perl format. Useful for debugging.

=item B<--shell>

Starts a CPAN shell using the configuration of F<install-module.pl>.

=item B<--help>

Shows this help.

=back
