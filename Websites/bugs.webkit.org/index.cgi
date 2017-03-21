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

if ($user->id) {
    my $dbh = Bugzilla->dbh;
    $vars->{assignee_count} =
      $dbh->selectrow_array("SELECT COUNT(*) FROM bugs WHERE assigned_to = ?
                             AND resolution = ''", undef, $user->id);
    $vars->{reporter_count} =
      $dbh->selectrow_array("SELECT COUNT(*) FROM bugs WHERE reporter = ?
                             AND resolution = ''", undef, $user->id);
    $vars->{requestee_count} =
      $dbh->selectrow_array('SELECT COUNT(DISTINCT bug_id) FROM flags
                             WHERE requestee_id = ?', undef, $user->id);
}

# Generate and return the UI (HTML page) from the appropriate template.
$template->process("index.html.tmpl", $vars)
  || ThrowTemplateError($template->error());
