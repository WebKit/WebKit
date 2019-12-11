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

use FindBin qw($RealBin);
use lib ($RealBin, "$RealBin/lib");

use Module::Build 0.36_14;

use Bugzilla::Install::Requirements qw(REQUIRED_MODULES OPTIONAL_MODULES);
use Bugzilla::Constants qw(BUGZILLA_VERSION);

sub requires {
    my $requirements = REQUIRED_MODULES();
    my $hrequires = {};
    foreach my $module (@$requirements) {
        $hrequires->{$module->{module}} = $module->{version};
    }
    return $hrequires;
};

sub build_requires {
    return requires();
}

sub recommends {
    my $recommends = OPTIONAL_MODULES();
    my @blacklist = ('Apache-SizeLimit', 'mod_perl'); # Does not compile properly on Travis
    my $hrecommends = {};
    foreach my $module (@$recommends) {
        next if grep($_ eq $module->{package}, @blacklist);
        $hrecommends->{$module->{module}} = $module->{version};
    }
    return $hrecommends;
}

my $build = Module::Build->new(
    module_name        => 'Bugzilla',
    dist_abstract      => <<END,
Bugzilla is a free bug-tracking system that is developed by an active
community of volunteers. You can install and use it without having to
pay any license fee.
END
    dist_version_from  => 'Bugzilla/Constants.pm',
    dist_version       => BUGZILLA_VERSION,
    requires           => requires(),
    recommends         => recommends(),
    license            => 'Mozilla_2_0',
    create_readme      => 0,
    create_makefile_pl => 0
);

$build->create_build_script;
