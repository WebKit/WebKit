#!/usr/bin/perl -wT
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
use Bugzilla::Hook;
use Bugzilla::WebService::Constants;

# Use an eval here so that runtests.pl accepts this script even if SOAP-Lite
# is not installed.
eval 'use XMLRPC::Transport::HTTP;
      use Bugzilla::WebService;';
$@ && ThrowCodeError('soap_not_installed');

Bugzilla->usage_mode(Bugzilla::Constants::USAGE_MODE_WEBSERVICE);
local $SOAP::Constants::FAULT_SERVER;
$SOAP::Constants::FAULT_SERVER = ERROR_UNKNOWN_FATAL;
# The line above is used, this one is ignored, but SOAP::Lite
# might start using this constant (the correct one) for XML-RPC someday.
local $XMLRPC::Constants::FAULT_SERVER;
$XMLRPC::Constants::FAULT_SERVER = ERROR_UNKNOWN_FATAL;

my %hook_dispatch;
Bugzilla::Hook::process('webservice', { dispatch => \%hook_dispatch });
local @INC = (bz_locations()->{extensionsdir}, @INC);

my $dispatch = {
    'Bugzilla' => 'Bugzilla::WebService::Bugzilla',
    'Bug'      => 'Bugzilla::WebService::Bug',
    'User'     => 'Bugzilla::WebService::User',
    'Product'  => 'Bugzilla::WebService::Product',
    %hook_dispatch
};

# The on_action sub needs to be wrapped so that we can work out which
# class to use; when the XMLRPC module calls it theres no indication
# of which dispatch class will be handling it.
# Note that using this to get code thats called before the actual routine
# is a bit of a hack; its meant to be for modifying the SOAPAction
# headers, which XMLRPC doesn't use; it relies on the XMLRPC modules
# using SOAP::Lite internally....

my $response = Bugzilla::WebService::XMLRPC::Transport::HTTP::CGI
    ->dispatch_with($dispatch)
    ->on_action(sub { Bugzilla::WebService::handle_login($dispatch, @_) } )
    ->handle;
