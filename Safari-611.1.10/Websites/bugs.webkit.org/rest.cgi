#!/usr/bin/perl -T
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::WebService::Constants;
BEGIN {
    if (!Bugzilla->feature('rest')
        || !Bugzilla->feature('jsonrpc'))
    {
        ThrowUserError('feature_disabled', { feature => 'rest' });
    }
}
use Bugzilla::WebService::Server::REST;
Bugzilla->usage_mode(USAGE_MODE_REST);
local @INC = (bz_locations()->{extensionsdir}, @INC);
my $server = new Bugzilla::WebService::Server::REST;
$server->version('1.1');
$server->handle();
