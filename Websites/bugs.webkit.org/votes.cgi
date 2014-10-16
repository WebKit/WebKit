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
# The Initial Developer of the Original Code is Everything Solved, Inc.
# Portions created by the Initial Developer are Copyright (C) 2010 the
# Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Max Kanat-Alexander <mkanat@bugzilla.org>

# This script remains as a backwards-compatibility URL for before
# the time that Voting was an extension.

use strict;
use lib qw(. lib);
use Bugzilla;
use Bugzilla::Error;

my $is_enabled = grep { $_->NAME eq 'Voting' } @{ Bugzilla->extensions };
$is_enabled || ThrowCodeError('extension_disabled', { name => 'Voting' });

my $cgi = Bugzilla->cgi;
my $action = $cgi->param('action') || 'show_user';

if ($action eq "show_bug") {
    $cgi->delete('action');
    $cgi->param('id', 'voting/bug.html');
} 
elsif ($action eq "show_user" or $action eq 'vote') {
    $cgi->delete('action') unless $action eq 'vote';
    $cgi->param('id', 'voting/user.html');
}
else {
    ThrowUserError('unknown_action', {action => $action});
}

print $cgi->redirect('page.cgi?' . $cgi->query_string);
exit;
