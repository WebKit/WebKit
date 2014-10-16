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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Jacob Steenhagen <jake@bugzilla.org>
#                 Frédéric Buclin <LpSolit@gmail.com>

###############################################################################
# Script Initialization
###############################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

# Include the Bugzilla CGI and general utility library.
use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Update;

# Check whether or not the user is logged in
my $user = Bugzilla->login(LOGIN_OPTIONAL);
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};

# And log out the user if requested. We do this first so that nothing
# else accidentally relies on the current login.
if ($cgi->param('logout')) {
    Bugzilla->logout();
    $user = Bugzilla->user;
    $vars->{'message'} = "logged_out";
    # Make sure that templates or other code doesn't get confused about this.
    $cgi->delete('logout');
}

###############################################################################
# Main Body Execution
###############################################################################

# Return the appropriate HTTP response headers.
print $cgi->header();

if ($user->in_group('admin')) {
    # If 'urlbase' is not set, display the Welcome page.
    unless (Bugzilla->params->{'urlbase'}) {
        $template->process('welcome-admin.html.tmpl')
          || ThrowTemplateError($template->error());
        exit;
    }
    # Inform the administrator about new releases, if any.
    $vars->{'release'} = Bugzilla::Update::get_notifications();
}

# Generate and return the UI (HTML page) from the appropriate template.
$template->process("index.html.tmpl", $vars)
  || ThrowTemplateError($template->error());
