#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use 5.10.1;
use diagnostics;
use strict;
use warnings;

use lib qw(lib);

use Test::Harness qw(&runtests $verbose);

$verbose = 0;
my $onlytest = "";

foreach (@ARGV) {
    if (/^(?:-v|--verbose)$/) {
        $verbose = 1;
    }
    else {
        $onlytest = sprintf("%0.3d",$_);
    }
}

runtests(glob("t/$onlytest*.t"));
