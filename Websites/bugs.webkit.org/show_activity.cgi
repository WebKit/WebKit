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
use Bugzilla::Error;
use Bugzilla::Bug;

my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};

###############################################################################
# Begin Data/Security Validation
###############################################################################

# Check whether or not the user is currently logged in. 
Bugzilla->login();

# Make sure the bug ID is a positive integer representing an existing
# bug that the user is authorized to access.
my $id = $cgi->param('id');
my $bug = Bugzilla::Bug->check($id);

###############################################################################
# End Data/Security Validation
###############################################################################

# Run queries against the shadow DB. In the worst case, new changes are not
# visible immediately due to replication lag.
Bugzilla->switch_to_shadow_db;

($vars->{'operations'}, $vars->{'incomplete_data'}) = $bug->get_activity(undef, undef, 1);

$vars->{'bug'} = $bug;

print $cgi->header();

$template->process("bug/activity/show.html.tmpl", $vars)
  || ThrowTemplateError($template->error());
