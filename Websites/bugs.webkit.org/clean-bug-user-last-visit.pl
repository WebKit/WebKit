#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

=head1 NAME

clean-bug-user-last-visit.pl

=head1 DESCRIPTION

This utility script cleans out entries from the bug_user_last_visit table that
are older than (a configurable) number of days.

It takes no arguments and produces no output except in the case of errors.

=cut

use 5.10.1;
use strict;
use warnings;
use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;

Bugzilla->usage_mode(USAGE_MODE_CMDLINE);

my $dbh = Bugzilla->dbh;
my $sql = 'DELETE FROM bug_user_last_visit WHERE last_visit_ts < '
  . $dbh->sql_date_math('NOW()',
                        '-',
                        Bugzilla->params->{last_visit_keep_days},
                        'DAY');
$dbh->do($sql);
