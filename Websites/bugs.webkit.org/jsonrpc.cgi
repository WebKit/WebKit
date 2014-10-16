#!/usr/bin/env perl -wT
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
# The Original Code is the Bugzilla JSON Webservices Interface.
#
# The Initial Developer of the Original Code is the San Jose State
# University Foundation. Portions created by the Initial Developer 
# are Copyright (C) 2008 the Initial Developer. All Rights Reserved.
#
# Contributor(s): 
#   Max Kanat-Alexander <mkanat@bugzilla.org>

use strict;
use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::WebService::Constants;
BEGIN {
    if (!Bugzilla->feature('jsonrpc')) {
        ThrowCodeError('feature_disabled', { feature => 'jsonrpc' });
    }
}
use Bugzilla::WebService::Server::JSONRPC;

Bugzilla->usage_mode(USAGE_MODE_JSON);

local @INC = (bz_locations()->{extensionsdir}, @INC);
my $server = new Bugzilla::WebService::Server::JSONRPC;
$server->dispatch(WS_DISPATCH)->handle();
