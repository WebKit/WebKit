#!/usr/bin/perl -T
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This script is used by servertest.pl to confirm that cgi scripts
# are being run instead of shown. This script does not rely on database access
# or correct params.

use 5.10.1;
use strict;
use warnings;

say "content-type:text/plain\n";
say "OK " . ($::ENV{MOD_PERL} || "mod_cgi");
exit;
