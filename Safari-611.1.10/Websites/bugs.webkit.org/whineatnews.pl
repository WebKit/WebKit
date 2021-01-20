#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


# This is a script suitable for running once a day from a cron job.  It 
# looks at all the bugs, and sends whiny mail to anyone who has a bug 
# assigned to them that has status CONFIRMED, NEW, or REOPENED that has not
# been touched for more than the number of days specified in the whinedays
# param. (We have NEW and REOPENED in there to keep compatibility with old
# Bugzillas.)

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Mailer;
use Bugzilla::Util;
use Bugzilla::User;

# Whining is disabled if whinedays is zero
exit unless Bugzilla->params->{'whinedays'} >= 1;

my $dbh = Bugzilla->dbh;
my $query = q{SELECT bug_id, short_desc, login_name
                FROM bugs
          INNER JOIN profiles
                  ON userid = assigned_to
               WHERE bug_status IN (?,?,?)
                 AND disable_mail = 0
                 AND } . $dbh->sql_to_days('NOW()') . " - " .
                       $dbh->sql_to_days('delta_ts') . " > " .
                       Bugzilla->params->{'whinedays'} .
          " ORDER BY bug_id";

my %bugs;
my %desc;

my $slt_bugs = $dbh->selectall_arrayref($query, undef, 'CONFIRMED', 'NEW',
                                                       'REOPENED');

foreach my $bug (@$slt_bugs) {
    my ($id, $desc, $email) = @$bug;
    if (!defined $bugs{$email}) {
        $bugs{$email} = [];
    }
    if (!defined $desc{$email}) {
        $desc{$email} = [];
    }
    push @{$bugs{$email}}, $id;
    push @{$desc{$email}}, $desc;
}


foreach my $email (sort (keys %bugs)) {
    my $user = new Bugzilla::User({name => $email});
    next if $user->email_disabled;

    my $vars = {'email' => $email};

    my @bugs = ();
    foreach my $i (@{$bugs{$email}}) {
        my $bug = {};
        $bug->{'summary'} = shift(@{$desc{$email}});
        $bug->{'id'} = $i;
        push @bugs, $bug;
    }
    $vars->{'bugs'} = \@bugs;

    my $msg;
    my $template = Bugzilla->template_inner($user->setting('lang'));
    $template->process("email/whine.txt.tmpl", $vars, \$msg)
      or die($template->error());

    MessageToMTA($msg);

    say "$email      " . join(" ", @{$bugs{$email}});
}
