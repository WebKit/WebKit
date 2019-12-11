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
use Bugzilla::BugMail;

my $dbh = Bugzilla->dbh;

my $list = $dbh->selectcol_arrayref(
        'SELECT bug_id FROM bugs
          WHERE (lastdiffed IS NULL OR lastdiffed < delta_ts)
            AND delta_ts < '
                . $dbh->sql_date_math('NOW()', '-', 30, 'MINUTE') .
     ' ORDER BY bug_id');

if (scalar(@$list) > 0) {
    say "OK, now attempting to send unsent mail";
    say scalar(@$list) . " bugs found with possibly unsent mail.\n";
    foreach my $bugid (@$list) {
        my $start_time = time;
        say "Sending mail for bug $bugid...";
        my $outputref = Bugzilla::BugMail::Send($bugid);
        if ($ARGV[0] && $ARGV[0] eq "--report") {
          say "Mail sent to:";
          say $_ foreach (sort @{$outputref->{sent}});
        }
        else {
            my $sent = scalar @{$outputref->{sent}};
            say "$sent mails sent.";
            say "Took " . (time - $start_time) . " seconds.\n";
        }
    }
    say "Unsent mail has been sent.";
}
