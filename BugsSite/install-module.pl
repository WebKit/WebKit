#!/usr/bin/perl -w
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

use strict;
use warnings;

# Have to abs_path('.') or calls to Bugzilla modules won't work once
# CPAN has chdir'ed around. We do all of this in this funny order to
# make sure that we use the lib/ modules instead of the base Perl modules,
# in case the lib/ modules are newer.
use Cwd qw(abs_path);
use lib abs_path('.');
use Bugzilla::Constants;
use lib abs_path(bz_locations()->{ext_libpath});

use Bugzilla::Install::CPAN;

use Bugzilla::Constants;
use Bugzilla::Install::Requirements;

use Data::Dumper;
use Getopt::Long;
use Pod::Usage;

our %switch;

GetOptions(\%switch, 'all|a', 'upgrade-all|u', 'show-config|s', 'global|g',
                     'help|h');

pod2usage({ -verbose => 1 }) if $switch{'help'};

if (ON_WINDOWS) {
    print "\nYou cannot run this script on Windows. Please follow instructions\n";
    print "given by checksetup.pl to install missing Perl modules.\n\n";
    exit;
}

pod2usage({ -verbose => 0 }) if (!%switch && !@ARGV);

set_cpan_config($switch{'global'});

if ($switch{'show-config'}) {
  print Dumper($CPAN::Config);
  exit;
}

my $can_notest = 1;
if (substr(CPAN->VERSION, 0, 3) < 1.8) {
    $can_notest = 0;
    print "* Note: If you upgrade your CPAN module, installs will be faster.\n";
    print "* You can upgrade CPAN by doing: $^X install-module.pl CPAN\n";
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
        install_module($cpan_name, $can_notest);
    }
}

foreach my $module (@ARGV) {
    install_module($module, $can_notest);
}

__END__

=head1 NAME

install-module.pl - Installs or upgrades modules from CPAN.
This script does not run on Windows.

=head1 SYNOPSIS

  ./install-module.pl Module::Name [--global]
  ./install-module.pl --all [--global]
  ./install-module.pl --all-upgrade [--global]
  ./install-module.pl --show-config

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

=item B<--help>

Shows this help.

=back
