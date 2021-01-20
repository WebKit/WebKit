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
use Bugzilla::Constants;

Bugzilla->login();

my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};

# Return the appropriate HTTP response headers.
print $cgi->header('application/xml');

# Get the contents of favicon.ico
my $filename = bz_locations()->{'libpath'} . "/images/favicon.ico";
if (open(IN, '<', $filename)) {
    local $/;
    binmode IN;
    $vars->{'favicon'} = <IN>;
    close IN;
}
$template->process("search/search-plugin.xml.tmpl", $vars)
  || ThrowTemplateError($template->error());
