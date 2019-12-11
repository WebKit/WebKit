#!/usr/bin/perl -T
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This script remains as a backwards-compatibility URL for before
# the time that Voting was an extension.

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Error;

my $is_enabled = grep { $_->NAME eq 'Voting' } @{ Bugzilla->extensions };
$is_enabled || ThrowUserError('extension_disabled', { name => 'Voting' });

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
