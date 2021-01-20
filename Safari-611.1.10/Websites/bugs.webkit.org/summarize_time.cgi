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

use Date::Parse;         # strptime

use Bugzilla;
use Bugzilla::Constants; # LOGIN_*
use Bugzilla::Bug;       # EmitDependList
use Bugzilla::Util;      # trim
use Bugzilla::Error;

#
# Date handling
#

sub date_adjust_down {
   
    my ($year, $month, $day) = @_;

    if ($day == 0) {
        $month -= 1;
        $day = 31;
        # Proper day adjustment is done later.

        if ($month == 0) {
            $year -= 1;
            $month = 12;
        }
    }

    if (($month == 2) && ($day > 28)) {
        if ($year % 4 == 0 && $year % 100 != 0) {
            $day = 29;
        } else {
            $day = 28;
        }
    }

    if (($month == 4 || $month == 6 || $month == 9 || $month == 11) &&
        ($day == 31) ) 
    {
        $day = 30;
    }
    return ($year, $month, $day);
}

sub date_adjust_up {
    my ($year, $month, $day) = @_;

    if ($day > 31) {
        $month += 1;
        $day    = 1;

        if ($month == 13) {
            $month = 1;
            $year += 1;
        }
    }

    if ($month == 2 && $day > 28) {
        if ($year % 4 != 0 || $year % 100 == 0 || $day > 29) {
            $month = 3;
            $day = 1;
        }
    }

    if (($month == 4 || $month == 6 || $month == 9 || $month == 11) &&
        ($day == 31) )
    {
        $month += 1; 
        $day    = 1;
    }

    return ($year, $month, $day);
}

sub split_by_month {
    # Takes start and end dates and splits them into a list of
    # monthly-spaced 2-lists of dates.
    my ($start_date, $end_date) = @_;

    # We assume at this point that the dates are provided and sane
    my (undef, undef, undef, $sd, $sm, $sy, undef) = strptime($start_date);
    my (undef, undef, undef, $ed, $em, $ey, undef) = strptime($end_date);

    # Find out how many months fit between the two dates so we know
    # how many times we loop.
    my $yd = $ey - $sy;
    my $md = 12 * $yd + $em - $sm;
    # If the end day is smaller than the start day, last interval is not a whole month.
    if ($sd > $ed) {
        $md -= 1;
    }

    my (@months, $sub_start, $sub_end);
    # This +1 and +1900 are a result of strptime's bizarre semantics
    my $year = $sy + 1900;
    my $month = $sm + 1;

    # Keep the original date, when the date will be changed in the adjust_date.
    my $sd_tmp = $sd;
    my $month_tmp = $month;
    my $year_tmp = $year;

    # This section handles only the whole months.
    for (my $i=0; $i < $md; $i++) {
        # Start of interval is adjusted up: 31.2. -> 1.3.
        ($year_tmp, $month_tmp, $sd_tmp) = date_adjust_up($year, $month, $sd);
        $sub_start = sprintf("%04d-%02d-%02d", $year_tmp, $month_tmp, $sd_tmp); 
        $month += 1;
        if ($month == 13) {
            $month = 1;
            $year += 1;
        }
        # End of interval is adjusted down: 31.2 -> 28.2.
        ($year_tmp, $month_tmp, $sd_tmp) = date_adjust_down($year, $month, $sd - 1);
        $sub_end = sprintf("%04d-%02d-%02d", $year_tmp, $month_tmp, $sd_tmp);
        push @months, [$sub_start, $sub_end];
    }
    
    # This section handles the last (unfinished) month. 
    $sub_end = sprintf("%04d-%02d-%02d", $ey + 1900, $em + 1, $ed);
    ($year_tmp, $month_tmp, $sd_tmp) = date_adjust_up($year, $month, $sd);
    $sub_start = sprintf("%04d-%02d-%02d", $year_tmp, $month_tmp, $sd_tmp);
    push @months, [$sub_start, $sub_end];

    return @months;
}

sub sqlize_dates {
    my ($start_date, $end_date) = @_;
    my $date_bits = "";
    my @date_values;
    if ($start_date) {
        # we've checked, trick_taint is fine
        trick_taint($start_date);
        $date_bits = " AND longdescs.bug_when > ?";
        push @date_values, $start_date;
    } 
    if ($end_date) {
        # we need to add one day to end_date to catch stuff done today
        # do not forget to adjust date if it was the last day of month
        my (undef, undef, undef, $ed, $em, $ey, undef) = strptime($end_date);
        ($ey, $em, $ed) = date_adjust_up($ey+1900, $em+1, $ed+1);
        $end_date = sprintf("%04d-%02d-%02d", $ey, $em, $ed);

        $date_bits .= " AND longdescs.bug_when < ?"; 
        push @date_values, $end_date;
    }
    return ($date_bits, \@date_values);
}

# Return all blockers of the current bug, recursively.
sub get_blocker_ids {
    my ($bug_id, $unique) = @_;
    $unique ||= {$bug_id => 1};
    my $deps = Bugzilla::Bug::EmitDependList("blocked", "dependson", $bug_id);
    my @unseen = grep { !$unique->{$_}++ } @$deps;
    foreach $bug_id (@unseen) {
        get_blocker_ids($bug_id, $unique);
    }
    return keys %$unique;
}

# Return a hashref whose key is chosen by the user (bug ID or commenter)
# and value is a hash of the form {bug ID, commenter, time spent}.
# So you can either view it as the time spent by commenters on each bug
# or the time spent in bugs by each commenter.
sub get_list {
    my ($bugids, $start_date, $end_date, $keyname) = @_;
    my $dbh = Bugzilla->dbh;

    my ($date_bits, $date_values) = sqlize_dates($start_date, $end_date);
    my $buglist = join(", ", @$bugids);

    # Returns the total time worked on each bug *per developer*.
    my $data = $dbh->selectall_arrayref(
            qq{SELECT SUM(work_time) AS total_time, login_name, longdescs.bug_id
                 FROM longdescs
           INNER JOIN profiles
                   ON longdescs.who = profiles.userid
           INNER JOIN bugs
                   ON bugs.bug_id = longdescs.bug_id
                WHERE longdescs.bug_id IN ($buglist) $date_bits } .
            $dbh->sql_group_by('longdescs.bug_id, login_name', 'longdescs.bug_when') .
           qq{ HAVING SUM(work_time) > 0}, {Slice => {}}, @$date_values);

    my %list;
    # What this loop does is to push data having the same key in an array.
    push(@{$list{ $_->{$keyname} }}, $_) foreach @$data;
    return \%list;
}

# Return bugs which had no activity (a.k.a work_time = 0) during the given time range.
sub get_inactive_bugs {
    my ($bugids, $start_date, $end_date) = @_;
    my $dbh = Bugzilla->dbh;
    my ($date_bits, $date_values) = sqlize_dates($start_date, $end_date);
    my $buglist = join(", ", @$bugids);

    my $bugs = $dbh->selectcol_arrayref(
        "SELECT bug_id
           FROM bugs
          WHERE bugs.bug_id IN ($buglist)
            AND NOT EXISTS (
                SELECT 1
                  FROM longdescs
                 WHERE bugs.bug_id = longdescs.bug_id
                   AND work_time > 0 $date_bits)",
         undef, @$date_values);

    return $bugs;
}

# Return 1st day of the month of the earliest activity date for a given list of bugs.
sub get_earliest_activity_date {
    my ($bugids) = @_;
    my $dbh = Bugzilla->dbh;

    my ($date) = $dbh->selectrow_array(
        'SELECT ' . $dbh->sql_date_format('MIN(bug_when)', '%Y-%m-01')
       . ' FROM longdescs
          WHERE ' . $dbh->sql_in('bug_id', $bugids)
                  . ' AND work_time > 0');

    return $date;
}

#
# Template code starts here
#

my $user = Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};

Bugzilla->switch_to_shadow_db();

$user->is_timetracker
    || ThrowUserError("auth_failure", {group  => "time-tracking",
                                       action => "access",
                                       object => "timetracking_summaries"});

my @ids = split(",", $cgi->param('id') || '');
@ids = map { Bugzilla::Bug->check($_)->id } @ids;
scalar(@ids) || ThrowUserError('no_bugs_chosen', {action => 'summarize'});

my $group_by = $cgi->param('group_by') || "number";
my $monthly = $cgi->param('monthly');
my $detailed = $cgi->param('detailed');
my $do_report = $cgi->param('do_report');
my $inactive = $cgi->param('inactive');
my $do_depends = $cgi->param('do_depends');
my $ctype = scalar($cgi->param("ctype"));

my ($start_date, $end_date);
if ($do_report) {
    my @bugs = @ids;

    # Dependency mode requires a single bug and grabs dependents.
    if ($do_depends) {
        if (scalar(@bugs) != 1) {
            ThrowCodeError("bad_arg", { argument=>"id",
                                        function=>"summarize_time"});
        }
        @bugs = get_blocker_ids($bugs[0]);
        @bugs = @{ $user->visible_bugs(\@bugs) };
    }

    $start_date = trim $cgi->param('start_date');
    $end_date = trim $cgi->param('end_date');

    foreach my $date ($start_date, $end_date) {
        next unless $date;
        validate_date($date)
          || ThrowUserError('illegal_date', {date => $date, format => 'YYYY-MM-DD'});
    }
    # Swap dates in case the user put an end_date before the start_date
    if ($start_date && $end_date && 
        str2time($start_date) > str2time($end_date)) {
        $vars->{'warn_swap_dates'} = 1;
        ($start_date, $end_date) = ($end_date, $start_date);
    }

    # Store dates in a session cookie so re-visiting the page
    # for other bugs keeps them around.
    $cgi->send_cookie(-name => 'time-summary-dates',
                      -value => join ";", ($start_date, $end_date));

    my (@parts, $part_data, @part_list);

    # Break dates apart into months if necessary; if not, we use the
    # same @parts list to allow us to use a common codepath.
    if ($monthly) {
        # Calculate the earliest activity date if the user doesn't
        # specify a start date.
        if (!$start_date) {
            $start_date = get_earliest_activity_date(\@bugs);
        }
        # Provide a default end date. Note that this differs in semantics
        # from the open-ended queries we use when start/end_date aren't
        # provided -- and clock skews will make this evident!
        @parts = split_by_month($start_date, 
                                $end_date || format_time(scalar localtime(time()), '%Y-%m-%d'));
    } else {
        @parts = ([$start_date, $end_date]);
    }

    # For each of the separate divisions, grab the relevant data.
    my $keyname = ($group_by eq 'owner') ? 'login_name' : 'bug_id';
    foreach my $part (@parts) {
        my ($sub_start, $sub_end) = @$part;
        $part_data = get_list(\@bugs, $sub_start, $sub_end, $keyname);
        push(@part_list, $part_data);
    }

    # Do we want to see inactive bugs?
    if ($inactive) {
        $vars->{'null'} = get_inactive_bugs(\@bugs, $start_date, $end_date);
    } else {
        $vars->{'null'} = {};
    }

    # Convert bug IDs to bug objects.
    @bugs = map {new Bugzilla::Bug($_)} @bugs;

    $vars->{'part_list'} = \@part_list;
    $vars->{'parts'} = \@parts;
    # We pass the list of bugs as a hashref.
    $vars->{'bugs'} = {map { $_->id => $_ } @bugs};
}
elsif ($cgi->cookie("time-summary-dates")) {
    ($start_date, $end_date) = split ";", $cgi->cookie('time-summary-dates');
}

$vars->{'ids'} = \@ids;
$vars->{'start_date'} = $start_date;
$vars->{'end_date'} = $end_date;
$vars->{'group_by'} = $group_by;
$vars->{'monthly'} = $monthly;
$vars->{'detailed'} = $detailed;
$vars->{'inactive'} = $inactive;
$vars->{'do_report'} = $do_report;
$vars->{'do_depends'} = $do_depends;

my $format = $template->get_format("bug/summarize-time", undef, $ctype);

# Get the proper content-type
print $cgi->header(-type=> $format->{'ctype'});
$template->process("$format->{'template'}", $vars)
  || ThrowTemplateError($template->error());
