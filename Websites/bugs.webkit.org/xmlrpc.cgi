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
# The Original Code is the Bugzilla Bug Tracking System.
#
# Contributor(s): Marc Schumann <wurblzap@gmail.com>

use strict;
use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::WebService::Constants;
BEGIN {
    if (!Bugzilla->feature('xmlrpc')) {
        ThrowCodeError('feature_disabled', { feature => 'xmlrpc' });
    }
}
use Bugzilla::WebService::Server::XMLRPC;

Bugzilla->usage_mode(USAGE_MODE_XMLRPC);

# Fix the error code that SOAP::Lite uses for Perl errors.
local $SOAP::Constants::FAULT_SERVER;
$SOAP::Constants::FAULT_SERVER = ERROR_UNKNOWN_FATAL;
# The line above is used, this one is ignored, but SOAP::Lite
# might start using this constant (the correct one) for XML-RPC someday.
local $XMLRPC::Constants::FAULT_SERVER;
$XMLRPC::Constants::FAULT_SERVER = ERROR_UNKNOWN_FATAL;

local @INC = (bz_locations()->{extensionsdir}, @INC);
my $server = new Bugzilla::WebService::Server::XMLRPC;
# We use a sub for on_action because that gets us the info about what 
# class is being called. Note that this is a hack--this is technically 
# for setting SOAPAction, which isn't used by XML-RPC.
$server->on_action(sub { $server->handle_login(WS_DISPATCH, @_) })
       ->handle();
