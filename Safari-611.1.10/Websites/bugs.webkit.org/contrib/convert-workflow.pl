#!/usr/bin/perl
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
use Bugzilla::Config qw(:admin);
use Bugzilla::Search::Saved;
use Bugzilla::Status;
use Getopt::Long;

my $confirmed   = new Bugzilla::Status({ name => 'CONFIRMED' });
my $in_progress = new Bugzilla::Status({ name => 'IN_PROGRESS' });

if ($confirmed and $in_progress) {
    print "You are already using the new workflow.\n";
    exit 1;
}
my $enable_unconfirmed  = 0;
my $result = GetOptions("enable-unconfirmed" => \$enable_unconfirmed);

print <<END;
WARNING: This will convert the status of all bugs using the following
system:

  "NEW" will become "CONFIRMED"
  "ASSIGNED" will become "IN_PROGRESS"
  "REOPENED" will become "CONFIRMED" (and the "REOPENED" status will be removed)
  "CLOSED" will become "VERIFIED" (and the "CLOSED" status will be removed)

This change will be immediate. The history of each bug will also be changed
so that it appears that these statuses were always in existence.

Emails will not be sent for the change.

END
if ($enable_unconfirmed) {
    print "UNCONFIRMED will be enabled in all products.\n";
} else {
    print <<END;
If you also want to enable the UNCONFIRMED status in every product,
restart this script with the --enable-unconfirmed option.
END
}
print "\nTo continue, press any key, or press Ctrl-C to stop this program...";
getc;

my $dbh = Bugzilla->dbh;
# This is an array instead of a hash so that we can be sure that
# the translation happens in the right order. In particular, we
# want NEW to be renamed to CONFIRMED, instead of having REOPENED
# be the one that gets renamed.
my @translation = (
    [NEW      => 'CONFIRMED'],
    [ASSIGNED => 'IN_PROGRESS'],
    [REOPENED => 'CONFIRMED'],
    [CLOSED   => 'VERIFIED'],
);

my $status_field = Bugzilla::Field->check('bug_status');
$dbh->bz_start_transaction();
foreach my $pair (@translation) {
    my ($from, $to) = @$pair;
    print "Converting $from to $to...\n";
    # There is no FK on bugs.bug_status pointing to bug_status.value,
    # so it's fine to update the bugs table first.
    $dbh->do('UPDATE bugs SET bug_status = ? WHERE bug_status = ?',
             undef, $to, $from);

    if (Bugzilla->params->{'duplicate_or_move_bug_status'} eq $from) {
        SetParam('duplicate_or_move_bug_status', $to);
        write_params();
    }

    foreach my $what (qw(added removed)) {
        $dbh->do("UPDATE bugs_activity SET $what = ?
                   WHERE fieldid = ? AND $what = ?",
                 undef, $to, $status_field->id, $from);
    }

    # Delete any transitions where it now appears that
    # a bug moved from a status to itself.
    $dbh->do('DELETE FROM bugs_activity WHERE fieldid = ? AND added = removed',
             undef, $status_field->id);

    # If the new status already exists, just delete the old one, but retain
    # the workflow items from it.
    my $new_status = new Bugzilla::Status({ name => $to });
    my $old_status = new Bugzilla::Status({ name => $from });

    if ($new_status && $old_status) {
        my $to_id = $new_status->id;
        my $from_id = $old_status->id;
        # The subselect collects existing transitions from the target bug status.
        # The main select collects existing transitions from the renamed bug status.
        # The diff tells us which transitions are missing from the target bug status.
        my $missing_transitions =
          $dbh->selectcol_arrayref('SELECT sw1.new_status
                                      FROM status_workflow sw1
                                     WHERE sw1.old_status = ?
                                       AND sw1.new_status NOT IN (SELECT sw2.new_status
                                                                    FROM status_workflow sw2
                                                                   WHERE sw2.old_status = ?)',
                                     undef, ($from_id, $to_id));

        $dbh->do('UPDATE status_workflow SET old_status = ? WHERE old_status = ? AND '
                 . $dbh->sql_in('new_status', $missing_transitions),
                 undef, ($to_id, $from_id)) if @$missing_transitions;

        # The subselect collects existing transitions to the target bug status.
        # The main select collects existing transitions to the renamed bug status.
        # The diff tells us which transitions are missing to the target bug status.
        # We have to explicitly exclude NULL from the subselect, because NOT IN
        # doesn't know what to do with it (neither true nor false) and no data is returned.
        $missing_transitions =
          $dbh->selectcol_arrayref('SELECT sw1.old_status
                                      FROM status_workflow sw1
                                     WHERE sw1.new_status = ?
                                       AND sw1.old_status NOT IN (SELECT sw2.old_status
                                                                    FROM status_workflow sw2
                                                                   WHERE sw2.new_status = ?
                                                                     AND sw2.old_status IS NOT NULL)',
                                     undef, ($from_id, $to_id));

        $dbh->do('UPDATE status_workflow SET new_status = ? WHERE new_status = ? AND '
                 . $dbh->sql_in('old_status', $missing_transitions),
                 undef, ($to_id, $from_id)) if @$missing_transitions;

        # Delete rows where old_status = new_status, and then the old status itself.
        $dbh->do('DELETE FROM status_workflow WHERE old_status = new_status');
        $dbh->do('DELETE FROM bug_status WHERE value = ?', undef, $from);
    }
    # Otherwise, rename the old status to the new one.
    elsif ($old_status) {
        $dbh->do('UPDATE bug_status SET value = ? WHERE value = ?',
                 undef, $to, $from);
    }

    Bugzilla::Search::Saved->rename_field_value('bug_status', $from, $to);
    Bugzilla::Series->Bugzilla::Search::Saved::rename_field_value('bug_status',
        $from, $to);
}
if ($enable_unconfirmed) {
    print "Enabling UNCONFIRMED in all products...\n";
    $dbh->do('UPDATE products SET allows_unconfirmed = 1');
}
$dbh->bz_commit_transaction();
Bugzilla->memcached->clear_all();

print <<END;
Done. There are some things you may want to fix, now:

  * You may want to run ./collectstats.pl --regenerate to regenerate
    data for the Old Charts system. 
  * You may have to fix the Status Workflow using the Status Workflow
    panel in "Administration".
  * You will probably want to update the "mybugstemplate" and "defaultquery"
    parameters using the Parameters panel in "Administration". (Just
    resetting them to the default will work.)
END
