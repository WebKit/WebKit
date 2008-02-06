#!/usr/bin/perl -w
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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Joseph Heenan <joseph@heenan.me.uk>


# This is a script suitable for running once a day from a cron job.  It 
# looks at all the bugs, and sends whiny mail to anyone who has a bug 
# assigned to them that has status NEW or REOPENED that has not been 
# touched for more than the number of days specified in the whinedays param.

use strict;
use lib '.';

require "globals.pl";

use Bugzilla::BugMail;

# Whining is disabled if whinedays is zero
exit unless Param('whinedays') >= 1;

my $dbh = Bugzilla->dbh;
SendSQL("SELECT bug_id, short_desc, login_name " .
        "FROM bugs INNER JOIN profiles ON userid = assigned_to " .
        "WHERE (bug_status = 'NEW' OR bug_status = 'REOPENED') " .
        "AND " . $dbh->sql_to_days('NOW()') . " - " .
                 $dbh->sql_to_days('delta_ts') . " > " .
                 Param('whinedays') . " " .
        "ORDER BY bug_id");

my %bugs;
my %desc;
my @row;

while (@row = FetchSQLData()) {
    my ($id, $desc, $email) = (@row);
    if (!defined $bugs{$email}) {
        $bugs{$email} = [];
    }
    if (!defined $desc{$email}) {
        $desc{$email} = [];
    }
    push @{$bugs{$email}}, $id;
    push @{$desc{$email}}, $desc;
}


my $template = Param('whinemail');
my $urlbase = Param('urlbase');
my $emailsuffix = Param('emailsuffix');

foreach my $email (sort (keys %bugs)) {
    my %substs;
    $substs{'email'} = $email . $emailsuffix;
    $substs{'userid'} = $email;
    my $msg = PerformSubsts($template, \%substs);

    foreach my $i (@{$bugs{$email}}) {
        $msg .= "  " . shift(@{$desc{$email}}) . "\n";
        $msg .= "    -> ${urlbase}show_bug.cgi?id=$i\n";
    }

    Bugzilla::BugMail::MessageToMTA($msg);

    print "$email      " . join(" ", @{$bugs{$email}}) . "\n";
}
