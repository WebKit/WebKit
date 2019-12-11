#!/usr/bin/perl -T
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

################################################################################
# Script Initialization
################################################################################

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Group;
use Bugzilla::Token;
use Bugzilla::Whine::Schedule;
use Bugzilla::Whine::Query;
use Bugzilla::Whine;

use DateTime;

# require the user to have logged in
my $user = Bugzilla->login(LOGIN_REQUIRED);

###############################################################################
# Main Body Execution
###############################################################################

my $cgi      = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars     = {};
my $dbh      = Bugzilla->dbh;

my $userid   = $user->id;
my $token    = $cgi->param('token');
my $sth; # database statement handle

# $events is a hash ref of Bugzilla::Whine objects keyed by event id,
# that stores the active user's events.
#
# Eventually, it winds up with:
#  'queries'  - array ref containing hashes of:
#       'name'          - the name of the saved query
#       'title'         - The title line for the search results table
#       'sort'          - Numeric sort ID
#       'id'            - row ID for the query entry
#       'onemailperbug' - whether a single message must be sent for each
#                         result.
#  'schedule' - array ref containing hashes of:
#       'day' - Day or range of days this schedule will be run
#       'time' - time or interval to run
#       'mailto_type' - MAILTO_USER or MAILTO_GROUP
#       'mailto' - person/group who will receive the results
#       'id' - row ID for the schedule
my $events = get_events($userid);

# First see if this user may use whines
$user->in_group('bz_canusewhines')
  || ThrowUserError("auth_failure", {group  => "bz_canusewhines",
                                     action => "schedule",
                                     object => "reports"});

# May this user send mail to other users?
my $can_mail_others = $user->in_group('bz_canusewhineatothers');

# If the form was submitted, we need to look for what needs to be added or
# removed, then what was altered.

if ($cgi->param('update')) {
    check_token_data($token, 'edit_whine');

    if ($cgi->param("add_event")) {
        # we create a new event
        $sth = $dbh->prepare("INSERT INTO whine_events " .
                             "(owner_userid) " .
                             "VALUES (?)");
        $sth->execute($userid);
    }
    else {
        for my $eventid (keys %{$events}) {
            # delete an entire event
            if ($cgi->param("remove_event_$eventid")) {
                # We need to make sure these belong to the same user,
                # otherwise we could simply delete whatever matched that ID.
                #
                # schedules
                my $schedules = Bugzilla::Whine::Schedule->match({ eventid => $eventid });
                $sth = $dbh->prepare("DELETE FROM whine_schedules "
                    . "WHERE id=?");
                foreach my $schedule (@$schedules) {                    
                    $sth->execute($schedule->id);
                }

                # queries
                $sth = $dbh->prepare("SELECT whine_queries.id " .
                                     "FROM whine_queries " .
                                     "LEFT JOIN whine_events " .
                                     "ON whine_events.id = " .
                                     "whine_queries.eventid " .
                                     "WHERE whine_events.id = ? " .
                                     "AND whine_events.owner_userid = ?");
                $sth->execute($eventid, $userid);
                my @ids = @{$sth->fetchall_arrayref};
                $sth = $dbh->prepare("DELETE FROM whine_queries " .
                                     "WHERE id=?");
                for (@ids) {
                    my $delete_id = $_->[0];
                    $sth->execute($delete_id);
                }

                # events
                $sth = $dbh->prepare("DELETE FROM whine_events " .
                                     "WHERE id=? AND owner_userid=?");
                $sth->execute($eventid, $userid);
            }
            else {
                # check the subject, body and mailifnobugs for changes
                my $subject = ($cgi->param("event_${eventid}_subject") or '');
                my $body    = ($cgi->param("event_${eventid}_body")    or '');
                my $mailifnobugs = $cgi->param("event_${eventid}_mailifnobugs") ? 1 : 0;

                trick_taint($subject) if $subject;
                trick_taint($body)    if $body;

                if ( ($subject ne $events->{$eventid}->subject)
                  || ($mailifnobugs != $events->{$eventid}->mail_if_no_bugs)
                  || ($body    ne $events->{$eventid}->body) ) {

                    $sth = $dbh->prepare("UPDATE whine_events " .
                                         "SET subject=?, body=?, mailifnobugs=? " .
                                         "WHERE id=?");
                    $sth->execute($subject, $body, $mailifnobugs, $eventid);
                }

                # add a schedule
                if ($cgi->param("add_schedule_$eventid")) {
                    # the schedule table must be locked before altering
                    $sth = $dbh->prepare("INSERT INTO whine_schedules " .
                                         "(eventid, mailto_type, mailto, " .
                                         "run_day, run_time) " .
                                         "VALUES (?, ?, ?, 'Sun', 2)");
                    $sth->execute($eventid, MAILTO_USER, $userid);
                }
                # add a query
                elsif ($cgi->param("add_query_$eventid")) {
                    $sth = $dbh->prepare("INSERT INTO whine_queries "
                        . "(eventid) "
                        . "VALUES (?)");
                    $sth->execute($eventid);
                }
            }

            # now check all of the schedules and queries to see if they need
            # to be altered or deleted

            # Check schedules for changes
            my $schedules = Bugzilla::Whine::Schedule->match({ eventid => $eventid });
            my @scheduleids = ();
            foreach my $schedule (@$schedules) {
                push @scheduleids, $schedule->id;
            }

            # we need to double-check all of the user IDs in mailto to make
            # sure they exist
            my $arglist = {};   # args for match_field
            for my $sid (@scheduleids) {
                if ($cgi->param("mailto_type_$sid") == MAILTO_USER) {
                    $arglist->{"mailto_$sid"} = {
                        'type' => 'single',
                    };
                }
            }
            if (scalar %{$arglist}) {
                Bugzilla::User::match_field($arglist);
            }

            for my $sid (@scheduleids) {
                if ($cgi->param("remove_schedule_$sid")) {
                    # having the assignee id in here is a security failsafe
                    $sth = $dbh->prepare("SELECT whine_schedules.id " .
                                         "FROM whine_schedules " .
                                         "LEFT JOIN whine_events " .
                                         "ON whine_events.id = " .
                                         "whine_schedules.eventid " .
                                         "WHERE whine_events.owner_userid=? " .
                                         "AND whine_schedules.id =?");
                    $sth->execute($userid, $sid);

                    my @ids = @{$sth->fetchall_arrayref};
                    for (@ids) {
                        $sth = $dbh->prepare("DELETE FROM whine_schedules " .
                                             "WHERE id=?");
                        $sth->execute($_->[0]);
                    }
                }
                else {
                    my $o_day         = $cgi->param("orig_day_$sid") || '';
                    my $day           = $cgi->param("day_$sid") || '';
                    my $o_time        = $cgi->param("orig_time_$sid") || 0;
                    my $time          = $cgi->param("time_$sid") || 0;
                    my $o_mailto      = $cgi->param("orig_mailto_$sid") || '';
                    my $mailto        = $cgi->param("mailto_$sid") || '';
                    my $o_mailto_type = $cgi->param("orig_mailto_type_$sid") || 0;
                    my $mailto_type   = $cgi->param("mailto_type_$sid") || 0;

                    my $mailto_id = $userid;

                    # get an id for the mailto address
                    if ($can_mail_others && $mailto) {
                        if ($mailto_type == MAILTO_USER) {
                            $mailto_id = login_to_id($mailto);
                        }
                        elsif ($mailto_type == MAILTO_GROUP) {
                            # The group name is used in a placeholder.
                            trick_taint($mailto);
                            $mailto_id = Bugzilla::Group::ValidateGroupName($mailto, ($user))
                                           || ThrowUserError('invalid_group_name', { name => $mailto });
                        }
                        else {
                            # bad value, so it will just mail to the whine
                            # owner.  $mailto_id was already set above.
                            $mailto_type = MAILTO_USER;
                        }
                    }

                    detaint_natural($mailto_type);

                    if ( ($o_day  ne $day) ||
                         ($o_time ne $time) ||
                         ($o_mailto ne $mailto) ||
                         ($o_mailto_type != $mailto_type) ){

                        trick_taint($day);
                        trick_taint($time);

                        # the schedule table must be locked
                        $sth = $dbh->prepare("UPDATE whine_schedules " .
                                             "SET run_day=?, run_time=?, " .
                                             "mailto_type=?, mailto=?, " .
                                             "run_next=NULL " .
                                             "WHERE id=?");
                        $sth->execute($day, $time, $mailto_type,
                                      $mailto_id, $sid);
                    }
                }
            }

            # Check queries for changes
            my $queries = Bugzilla::Whine::Query->match({ eventid => $eventid });
            for my $query (@$queries) {
                my $qid = $query->id;
                if ($cgi->param("remove_query_$qid")) {

                    $sth = $dbh->prepare("SELECT whine_queries.id " .
                                         "FROM whine_queries " .
                                         "LEFT JOIN whine_events " .
                                         "ON whine_events.id = " .
                                         "whine_queries.eventid " .
                                         "WHERE whine_events.owner_userid=? " .
                                         "AND whine_queries.id =?");
                    $sth->execute($userid, $qid);

                    for (@{$sth->fetchall_arrayref}) {
                        $sth = $dbh->prepare("DELETE FROM whine_queries " .
                                             "WHERE id=?");
                        $sth->execute($_->[0]);
                    }
                }
                else {
                    my $o_sort      = $cgi->param("orig_query_sort_$qid") || 0;
                    my $sort        = $cgi->param("query_sort_$qid") || 0;
                    my $o_queryname = $cgi->param("orig_query_name_$qid") || '';
                    my $queryname   = $cgi->param("query_name_$qid") || '';
                    my $o_title     = $cgi->param("orig_query_title_$qid") || '';
                    my $title       = $cgi->param("query_title_$qid") || '';
                    my $o_onemailperbug =
                            $cgi->param("orig_query_onemailperbug_$qid") || 0;
                    my $onemailperbug   =
                            $cgi->param("query_onemailperbug_$qid") ? 1 : 0;

                    if ( ($o_sort != $sort) ||
                         ($o_queryname ne $queryname) ||
                         ($o_onemailperbug != $onemailperbug) ||
                         ($o_title ne $title) ){

                        detaint_natural($sort);
                        trick_taint($queryname);
                        trick_taint($title);

                        $sth = $dbh->prepare("UPDATE whine_queries " .
                                             "SET sortkey=?, " .
                                             "query_name=?, " .
                                             "title=?, " .
                                             "onemailperbug=? " .
                                             "WHERE id=?");
                        $sth->execute($sort, $queryname, $title,
                                      $onemailperbug, $qid);
                    }
                }
            }
        }
    }
    delete_token($token);
}

$vars->{'mail_others'} = $can_mail_others;

# Return the appropriate HTTP response headers.
print $cgi->header();

# Get events again, to cover any updates that were made
$events = get_events($userid);

# Here is the data layout as sent to the template:
#
#   events
#       event_id #
#           schedule
#               day
#               time
#               mailto
#           queries
#               name
#               title
#               sort
#
# build the whine list by event id
for my $event_id (keys %{$events}) {
    $events->{$event_id}->{'schedule'} = [];
    $events->{$event_id}->{'queries'} = [];

    # schedules
    my $schedules = Bugzilla::Whine::Schedule->match({ eventid => $event_id });
    foreach my $schedule (@$schedules) {
        my $mailto_type = $schedule->mailto_is_group ? MAILTO_GROUP 
                                                     : MAILTO_USER;
        my $mailto = '';
        if ($mailto_type == MAILTO_USER) {
            $mailto = $schedule->mailto->login;
        }
        elsif ($mailto_type == MAILTO_GROUP) {
            $mailto = $schedule->mailto->name;
        }

        push @{$events->{$event_id}->{'schedule'}},
             {
                 'day'         => $schedule->run_day,
                 'time'        => $schedule->run_time,
                 'mailto_type' => $mailto_type,
                 'mailto'      => $mailto,
                 'id'          => $schedule->id,
             };
    }

    # queries
    my $queries = Bugzilla::Whine::Query->match({ eventid => $event_id });
    for my $query (@$queries) {
        push @{$events->{$event_id}->{'queries'}}, 
             {
                 'name'          => $query->name,
                 'title'         => $query->title,
                 'sort'          => $query->sortkey,
                 'id'            => $query->id,
                 'onemailperbug' => $query->one_email_per_bug,
             };
    }
}

$vars->{'events'} = $events;

# get the available queries
$sth = $dbh->prepare("SELECT name FROM namedqueries WHERE userid=?");
$sth->execute($userid);

$vars->{'available_queries'} = [];
while (my ($query) = $sth->fetchrow_array) {
    push @{$vars->{'available_queries'}}, $query;
}
$vars->{'token'} = issue_session_token('edit_whine');
$vars->{'local_timezone'} = Bugzilla->local_timezone->short_name_for_datetime(DateTime->now());

$template->process("whine/schedule.html.tmpl", $vars)
  || ThrowTemplateError($template->error());

# get_events takes a userid and returns a hash of
# Bugzilla::Whine objects keyed by event ID.
sub get_events {
    my $userid = shift;
    my $event_rows = Bugzilla::Whine->match({ owner_userid => $userid });
    my %events = map { $_->{id} => $_ } @$event_rows;

    return \%events;
}
