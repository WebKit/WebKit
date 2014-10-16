#!/usr/bin/env perl -wT
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
#                 Dan Mosedale <dmose@mozilla.org>
#                 Stephan Niemz  <st.n@gmx.net>
#                 Andreas Franke <afranke@mathweb.org>
#                 Myk Melez <myk@mozilla.org>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

################################################################################
# Script Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Search;
use Bugzilla::Search::Quicksearch;
use Bugzilla::Search::Recent;
use Bugzilla::Search::Saved;
use Bugzilla::User;
use Bugzilla::Bug;
use Bugzilla::Product;
use Bugzilla::Keyword;
use Bugzilla::Field;
use Bugzilla::Status;
use Bugzilla::Token;

use Date::Parse;

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};
my $buffer = $cgi->query_string();

# We have to check the login here to get the correct footer if an error is
# thrown and to prevent a logged out user to use QuickSearch if 'requirelogin'
# is turned 'on'.
my $user = Bugzilla->login();

if (length($buffer) == 0) {
    print $cgi->header(-refresh=> '10; URL=query.cgi');
    ThrowUserError("buglist_parameters_required");
}

$cgi->redirect_search_url();

# Determine whether this is a quicksearch query.
my $searchstring = $cgi->param('quicksearch');
if (defined($searchstring)) {
    $buffer = quicksearch($searchstring);
    # Quicksearch may do a redirect, in which case it does not return.
    # If it does return, it has modified $cgi->params so we can use them here
    # as if this had been a normal query from the beginning.
}

# If configured to not allow empty words, reject empty searches from the
# Find a Specific Bug search form, including words being a single or 
# several consecutive whitespaces only.
if (!Bugzilla->params->{'search_allow_no_criteria'}
    && defined($cgi->param('content')) && $cgi->param('content') =~ /^\s*$/)
{
    ThrowUserError("buglist_parameters_required");
}

################################################################################
# Data and Security Validation
################################################################################

# Whether or not the user wants to change multiple bugs.
my $dotweak = $cgi->param('tweak') ? 1 : 0;

# Log the user in
if ($dotweak) {
    Bugzilla->login(LOGIN_REQUIRED);
}

# Hack to support legacy applications that think the RDF ctype is at format=rdf.
if (defined $cgi->param('format') && $cgi->param('format') eq "rdf"
    && !defined $cgi->param('ctype')) {
    $cgi->param('ctype', "rdf");
    $cgi->delete('format');
}

# Treat requests for ctype=rss as requests for ctype=atom
if (defined $cgi->param('ctype') && $cgi->param('ctype') eq "rss") {
    $cgi->param('ctype', "atom");
}

# An agent is a program that automatically downloads and extracts data
# on its user's behalf.  If this request comes from an agent, we turn off
# various aspects of bug list functionality so agent requests succeed
# and coexist nicely with regular user requests.  Currently the only agent
# we know about is Firefox's microsummary feature.
my $agent = ($cgi->http('X-Moz') && $cgi->http('X-Moz') =~ /\bmicrosummary\b/);

# Determine the format in which the user would like to receive the output.
# Uses the default format if the user did not specify an output format;
# otherwise validates the user's choice against the list of available formats.
my $format = $template->get_format("list/list", scalar $cgi->param('format'),
                                   scalar $cgi->param('ctype'));

# Use server push to display a "Please wait..." message for the user while
# executing their query if their browser supports it and they are viewing
# the bug list as HTML and they have not disabled it by adding &serverpush=0
# to the URL.
#
# Server push is a Netscape 3+ hack incompatible with MSIE, Lynx, and others. 
# Even Communicator 4.51 has bugs with it, especially during page reload.
# http://www.browsercaps.org used as source of compatible browsers.
# Safari (WebKit) does not support it, despite a UA that says otherwise (bug 188712)
# MSIE 5+ supports it on Mac (but not on Windows) (bug 190370)
#
my $serverpush =
  $format->{'extension'} eq "html"
    && exists $ENV{'HTTP_USER_AGENT'} 
      && $ENV{'HTTP_USER_AGENT'} =~ /Mozilla.[3-9]/ 
        && (($ENV{'HTTP_USER_AGENT'} !~ /[Cc]ompatible/) || ($ENV{'HTTP_USER_AGENT'} =~ /MSIE 5.*Mac_PowerPC/))
          && $ENV{'HTTP_USER_AGENT'} !~ /WebKit/
            && !$agent
              && !defined($cgi->param('serverpush'))
                || $cgi->param('serverpush');

my $order = $cgi->param('order') || "";

# The params object to use for the actual query itself
my $params;

# If the user is retrieving the last bug list they looked at, hack the buffer
# storing the query string so that it looks like a query retrieving those bugs.
if (my $last_list = $cgi->param('regetlastlist')) {
    my $bug_ids;

    # Logged-out users use the old cookie method for storing the last search.
    if (!$user->id or $last_list eq 'cookie') {
        $bug_ids = $cgi->cookie('BUGLIST') or ThrowUserError("missing_cookie");
        $bug_ids =~ s/[:-]/,/g;
        $order ||= "reuse last sort";
    }
    # But logged in users store the last X searches in the DB so they can
    # have multiple bug lists available.
    else {
        my $last_search = Bugzilla::Search::Recent->check(
            { id => $last_list });
        $bug_ids = join(',', @{ $last_search->bug_list });
        $order ||= $last_search->list_order;
    }
    # set up the params for this new query
    $params = new Bugzilla::CGI({ bug_id => $bug_ids, order => $order });
    $params->param('list_id', $last_list);
}

# Figure out whether or not the user is doing a fulltext search.  If not,
# we'll remove the relevance column from the lists of columns to display
# and order by, since relevance only exists when doing a fulltext search.
my $fulltext = 0;
if ($cgi->param('content')) { $fulltext = 1 }
my @charts = map(/^field(\d-\d-\d)$/ ? $1 : (), $cgi->param());
foreach my $chart (@charts) {
    if ($cgi->param("field$chart") eq 'content' && $cgi->param("value$chart")) {
        $fulltext = 1;
        last;
    }
}

################################################################################
# Utilities
################################################################################

sub DiffDate {
    my ($datestr) = @_;
    my $date = str2time($datestr);
    my $age = time() - $date;

    if( $age < 18*60*60 ) {
        $date = format_time($datestr, '%H:%M:%S');
    } elsif( $age < 6*24*60*60 ) {
        $date = format_time($datestr, '%a %H:%M');
    } else {
        $date = format_time($datestr, '%Y-%m-%d');
    }
    return $date;
}

sub LookupNamedQuery {
    my ($name, $sharer_id) = @_;

    Bugzilla->login(LOGIN_REQUIRED);

    my $query = Bugzilla::Search::Saved->check(
        { user => $sharer_id, name => $name });

    $query->url
       || ThrowUserError("buglist_parameters_required");

    # Detaint $sharer_id.
    $sharer_id = $query->user->id if $sharer_id;
    return wantarray ? ($query->url, $query->id, $sharer_id) : $query->url;
}

# Inserts a Named Query (a "Saved Search") into the database, or
# updates a Named Query that already exists..
# Takes four arguments:
# userid - The userid who the Named Query will belong to.
# query_name - A string that names the new Named Query, or the name
#              of an old Named Query to update. If this is blank, we
#              will throw a UserError. Leading and trailing whitespace
#              will be stripped from this value before it is inserted
#              into the DB.
# query - The query part of the buglist.cgi URL, unencoded. Must not be 
#         empty, or we will throw a UserError.
# link_in_footer (optional) - 1 if the Named Query should be 
# displayed in the user's footer, 0 otherwise.
#
# All parameters are validated before passing them into the database.
#
# Returns: A boolean true value if the query existed in the database 
# before, and we updated it. A boolean false value otherwise.
sub InsertNamedQuery {
    my ($query_name, $query, $link_in_footer) = @_;
    my $dbh = Bugzilla->dbh;

    $query_name = trim($query_name);
    my ($query_obj) = grep {lc($_->name) eq lc($query_name)} @{Bugzilla->user->queries};

    if ($query_obj) {
        $query_obj->set_name($query_name);
        $query_obj->set_url($query);
        $query_obj->update();
    } else {
        Bugzilla::Search::Saved->create({
            name           => $query_name,
            query          => $query,
            link_in_footer => $link_in_footer
        });
    }

    return $query_obj ? 1 : 0;
}

sub LookupSeries {
    my ($series_id) = @_;
    detaint_natural($series_id) || ThrowCodeError("invalid_series_id");
    
    my $dbh = Bugzilla->dbh;
    my $result = $dbh->selectrow_array("SELECT query FROM series " .
                                       "WHERE series_id = ?"
                                       , undef, ($series_id));
    $result
           || ThrowCodeError("invalid_series_id", {'series_id' => $series_id});
    return $result;
}

sub GetQuip {
    my $dbh = Bugzilla->dbh;
    # COUNT is quick because it is cached for MySQL. We may want to revisit
    # this when we support other databases.
    my $count = $dbh->selectrow_array("SELECT COUNT(quip)"
                                    . " FROM quips WHERE approved = 1");
    my $random = int(rand($count));
    my $quip = 
        $dbh->selectrow_array("SELECT quip FROM quips WHERE approved = 1 " . 
                              $dbh->sql_limit(1, $random));
    return $quip;
}

# Return groups available for at least one product of the buglist.
sub GetGroups {
    my $product_names = shift;
    my $user = Bugzilla->user;
    my %legal_groups;

    foreach my $product_name (@$product_names) {
        my $product = new Bugzilla::Product({name => $product_name});

        foreach my $gid (keys %{$product->group_controls}) {
            # The user can only edit groups he belongs to.
            next unless $user->in_group_id($gid);

            # The user has no control on groups marked as NA or MANDATORY.
            my $group = $product->group_controls->{$gid};
            next if ($group->{membercontrol} == CONTROLMAPMANDATORY
                     || $group->{membercontrol} == CONTROLMAPNA);

            # It's fine to include inactive groups. Those will be marked
            # as "remove only" when editing several bugs at once.
            $legal_groups{$gid} ||= $group->{group};
        }
    }
    # Return a list of group objects.
    return [values %legal_groups];
}

sub _close_standby_message {
    my ($contenttype, $disposition, $serverpush) = @_;
    my $cgi = Bugzilla->cgi;

    # Close the "please wait" page, then open the buglist page
    if ($serverpush) {
        print $cgi->multipart_end();
        print $cgi->multipart_start(-type                => $contenttype,
                                    -content_disposition => $disposition);
    }
    else {
        print $cgi->header(-type                => $contenttype,
                           -content_disposition => $disposition);
    }
}


################################################################################
# Command Execution
################################################################################

my $cmdtype   = $cgi->param('cmdtype')   || '';
my $remaction = $cgi->param('remaction') || '';
my $sharer_id;

# Backwards-compatibility - the old interface had cmdtype="runnamed" to run
# a named command, and we can't break this because it's in bookmarks.
if ($cmdtype eq "runnamed") {  
    $cmdtype = "dorem";
    $remaction = "run";
}

# Now we're going to be running, so ensure that the params object is set up,
# using ||= so that we only do so if someone hasn't overridden this 
# earlier, for example by setting up a named query search.

# This will be modified, so make a copy.
$params ||= new Bugzilla::CGI($cgi);

# Generate a reasonable filename for the user agent to suggest to the user
# when the user saves the bug list.  Uses the name of the remembered query
# if available.  We have to do this now, even though we return HTTP headers 
# at the end, because the fact that there is a remembered query gets 
# forgotten in the process of retrieving it.
my @time = localtime(time());
my $date = sprintf "%04d-%02d-%02d", 1900+$time[5],$time[4]+1,$time[3];
my $filename = "bugs-$date.$format->{extension}";
if ($cmdtype eq "dorem" && $remaction =~ /^run/) {
    $filename = $cgi->param('namedcmd') . "-$date.$format->{extension}";
    # Remove white-space from the filename so the user cannot tamper
    # with the HTTP headers.
    $filename =~ s/\s/_/g;
}
$filename =~ s/\\/\\\\/g; # escape backslashes
$filename =~ s/"/\\"/g; # escape quotes

# Take appropriate action based on user's request.
if ($cmdtype eq "dorem") {  
    if ($remaction eq "run") {
        my $query_id;
        ($buffer, $query_id, $sharer_id) =
          LookupNamedQuery(scalar $cgi->param("namedcmd"),
                           scalar $cgi->param('sharer_id'));
        # If this is the user's own query, remember information about it
        # so that it can be modified easily.
        $vars->{'searchname'} = $cgi->param('namedcmd');
        if (!$cgi->param('sharer_id') ||
            $cgi->param('sharer_id') == Bugzilla->user->id) {
            $vars->{'searchtype'} = "saved";
            $vars->{'search_id'} = $query_id;
        }
        $params = new Bugzilla::CGI($buffer);
        $order = $params->param('order') || $order;

    }
    elsif ($remaction eq "runseries") {
        $buffer = LookupSeries(scalar $cgi->param("series_id"));
        $vars->{'searchname'} = $cgi->param('namedcmd');
        $vars->{'searchtype'} = "series";
        $params = new Bugzilla::CGI($buffer);
        $order = $params->param('order') || $order;
    }
    elsif ($remaction eq "forget") {
        $user = Bugzilla->login(LOGIN_REQUIRED);
        # Copy the name into a variable, so that we can trick_taint it for
        # the DB. We know it's safe, because we're using placeholders in 
        # the SQL, and the SQL is only a DELETE.
        my $qname = $cgi->param('namedcmd');
        trick_taint($qname);

        # Do not forget the saved search if it is being used in a whine
        my $whines_in_use = 
            $dbh->selectcol_arrayref('SELECT DISTINCT whine_events.subject
                                                 FROM whine_events
                                           INNER JOIN whine_queries
                                                   ON whine_queries.eventid
                                                      = whine_events.id
                                                WHERE whine_events.owner_userid
                                                      = ?
                                                  AND whine_queries.query_name
                                                      = ?
                                      ', undef, $user->id, $qname);
        if (scalar(@$whines_in_use)) {
            ThrowUserError('saved_search_used_by_whines', 
                           { subjects    => join(',', @$whines_in_use),
                             search_name => $qname                      }
            );
        }

        # If we are here, then we can safely remove the saved search
        my $query_id;
        ($buffer, $query_id) = LookupNamedQuery(scalar $cgi->param("namedcmd"),
                                                $user->id);
        if ($query_id) {
            # Make sure the user really wants to delete his saved search.
            my $token = $cgi->param('token');
            check_hash_token($token, [$query_id, $qname]);

            $dbh->do('DELETE FROM namedqueries
                            WHERE id = ?',
                     undef, $query_id);
            $dbh->do('DELETE FROM namedqueries_link_in_footer
                            WHERE namedquery_id = ?',
                     undef, $query_id);
            $dbh->do('DELETE FROM namedquery_group_map
                            WHERE namedquery_id = ?',
                     undef, $query_id);
        }

        # Now reset the cached queries
        $user->flush_queries_cache();

        print $cgi->header();
        # Generate and return the UI (HTML page) from the appropriate template.
        $vars->{'message'} = "buglist_query_gone";
        $vars->{'namedcmd'} = $qname;
        $vars->{'url'} = "buglist.cgi?newquery=" . url_quote($buffer) . "&cmdtype=doit&remtype=asnamed&newqueryname=" . url_quote($qname);
        $template->process("global/message.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        exit;
    }
}
elsif (($cmdtype eq "doit") && defined $cgi->param('remtype')) {
    if ($cgi->param('remtype') eq "asdefault") {
        $user = Bugzilla->login(LOGIN_REQUIRED);
        InsertNamedQuery(DEFAULT_QUERY_NAME, $buffer);
        $vars->{'message'} = "buglist_new_default_query";
    }
    elsif ($cgi->param('remtype') eq "asnamed") {
        $user = Bugzilla->login(LOGIN_REQUIRED);
        my $query_name = $cgi->param('newqueryname');
        my $new_query = $cgi->param('newquery');
        my $token = $cgi->param('token');
        check_hash_token($token, ['savedsearch']);
        # If list_of_bugs is true, we are adding/removing tags to/from
        # individual bugs.
        if ($cgi->param('list_of_bugs')) {
            # We add/remove tags based on the action choosen.
            my $action = trim($cgi->param('action') || '');
            $action =~ /^(add|remove)$/
              || ThrowUserError('unknown_action', {action => $action});

            my $method = "${action}_tag";

            # If no new tag name has been given, use the selected one.
            $query_name ||= $cgi->param('oldqueryname')
              or ThrowUserError('no_tag_to_edit', {action => $action});

            my @buglist;
            # Validate all bug IDs before editing tags in any of them.
            foreach my $bug_id (split(/[\s,]+/, $cgi->param('bug_ids'))) {
                next unless $bug_id;
                push(@buglist, Bugzilla::Bug->check($bug_id));
            }

            foreach my $bug (@buglist) {
                $bug->$method($query_name);
            }

            $vars->{'message'} = 'tag_updated';
            $vars->{'action'} = $action;
            $vars->{'tag'} = $query_name;
            $vars->{'buglist'} = [map { $_->id } @buglist];
        }
        else {
            my $existed_before = InsertNamedQuery($query_name, $new_query, 1);
            if ($existed_before) {
                $vars->{'message'} = "buglist_updated_named_query";
            }
            else {
                $vars->{'message'} = "buglist_new_named_query";
            }

            # Make sure to invalidate any cached query data, so that the footer is
            # correctly displayed
            $user->flush_queries_cache();

            $vars->{'queryname'} = $query_name;
        }

        print $cgi->header();
        $template->process("global/message.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        exit;
    }
}

# backward compatibility hack: if the saved query doesn't say which
# form was used to create it, assume it was on the advanced query
# form - see bug 252295
if (!$params->param('query_format')) {
    $params->param('query_format', 'advanced');
    $buffer = $params->query_string;
}

################################################################################
# Column Definition
################################################################################

my $columns = Bugzilla::Search::COLUMNS;

################################################################################
# Display Column Determination
################################################################################

# Determine the columns that will be displayed in the bug list via the 
# columnlist CGI parameter, the user's preferences, or the default.
my @displaycolumns = ();
if (defined $params->param('columnlist')) {
    if ($params->param('columnlist') eq "all") {
        # If the value of the CGI parameter is "all", display all columns,
        # but remove the redundant "short_desc" column.
        @displaycolumns = grep($_ ne 'short_desc', keys(%$columns));
    }
    else {
        @displaycolumns = split(/[ ,]+/, $params->param('columnlist'));
    }
}
elsif (defined $cgi->cookie('COLUMNLIST')) {
    # 2002-10-31 Rename column names (see bug 176461)
    my $columnlist = $cgi->cookie('COLUMNLIST');
    $columnlist =~ s/\bowner\b/assigned_to/;
    $columnlist =~ s/\bowner_realname\b/assigned_to_realname/;
    $columnlist =~ s/\bplatform\b/rep_platform/;
    $columnlist =~ s/\bseverity\b/bug_severity/;
    $columnlist =~ s/\bstatus\b/bug_status/;
    $columnlist =~ s/\bsummaryfull\b/short_desc/;
    $columnlist =~ s/\bsummary\b/short_short_desc/;

    # Use the columns listed in the user's preferences.
    @displaycolumns = split(/ /, $columnlist);
}
else {
    # Use the default list of columns.
    @displaycolumns = DEFAULT_COLUMN_LIST;
}

# Weed out columns that don't actually exist to prevent the user 
# from hacking their column list cookie to grab data to which they 
# should not have access.  Detaint the data along the way.
@displaycolumns = grep($columns->{$_} && trick_taint($_), @displaycolumns);

# Remove the "ID" column from the list because bug IDs are always displayed
# and are hard-coded into the display templates.
@displaycolumns = grep($_ ne 'bug_id', @displaycolumns);

# Remove the timetracking columns if they are not a part of the group
# (happens if a user had access to time tracking and it was revoked/disabled)
if (!Bugzilla->user->is_timetracker) {
   @displaycolumns = grep($_ ne 'estimated_time', @displaycolumns);
   @displaycolumns = grep($_ ne 'remaining_time', @displaycolumns);
   @displaycolumns = grep($_ ne 'actual_time', @displaycolumns);
   @displaycolumns = grep($_ ne 'percentage_complete', @displaycolumns);
   @displaycolumns = grep($_ ne 'deadline', @displaycolumns);
}

# Remove the relevance column if the user is not doing a fulltext search.
if (grep('relevance', @displaycolumns) && !$fulltext) {
    @displaycolumns = grep($_ ne 'relevance', @displaycolumns);
}


################################################################################
# Select Column Determination
################################################################################

# Generate the list of columns that will be selected in the SQL query.

# The bug ID is always selected because bug IDs are always displayed.
# Severity, priority, resolution and status are required for buglist
# CSS classes.
my @selectcolumns = ("bug_id", "bug_severity", "priority", "bug_status",
                     "resolution", "product");

# remaining and actual_time are required for percentage_complete calculation:
if (grep { $_ eq "percentage_complete" } @displaycolumns) {
    push (@selectcolumns, "remaining_time");
    push (@selectcolumns, "actual_time");
}

# Make sure that the login_name version of a field is always also
# requested if the realname version is requested, so that we can
# display the login name when the realname is empty.
my @realname_fields = grep(/_realname$/, @displaycolumns);
foreach my $item (@realname_fields) {
    my $login_field = $item;
    $login_field =~ s/_realname$//;
    if (!grep($_ eq $login_field, @selectcolumns)) {
        push(@selectcolumns, $login_field);
    }
}

# Display columns are selected because otherwise we could not display them.
foreach my $col (@displaycolumns) {
    push (@selectcolumns, $col) if !grep($_ eq $col, @selectcolumns);
}

# If the user is editing multiple bugs, we also make sure to select the 
# status, because the values of that field determines what options the user
# has for modifying the bugs.
if ($dotweak) {
    push(@selectcolumns, "bug_status") if !grep($_ eq 'bug_status', @selectcolumns);
}

if ($format->{'extension'} eq 'ics') {
    push(@selectcolumns, "opendate") if !grep($_ eq 'opendate', @selectcolumns);
}

if ($format->{'extension'} eq 'atom') {
    # The title of the Atom feed will be the same one as for the bug list.
    $vars->{'title'} = $cgi->param('title');

    # This is the list of fields that are needed by the Atom filter.
    my @required_atom_columns = (
      'short_desc',
      'opendate',
      'changeddate',
      'reporter',
      'reporter_realname',
      'priority',
      'bug_severity',
      'assigned_to',
      'assigned_to_realname',
      'bug_status',
      'product',
      'component',
      'resolution'
    );
    push(@required_atom_columns, 'target_milestone') if Bugzilla->params->{'usetargetmilestone'};

    foreach my $required (@required_atom_columns) {
        push(@selectcolumns, $required) if !grep($_ eq $required,@selectcolumns);
    }
}

################################################################################
# Sort Order Determination
################################################################################

# Add to the query some instructions for sorting the bug list.

# First check if we'll want to reuse the last sorting order; that happens if
# the order is not defined or its value is "reuse last sort"
if (!$order || $order =~ /^reuse/i) {
    if ($cgi->cookie('LASTORDER')) {
        $order = $cgi->cookie('LASTORDER');
       
        # Cookies from early versions of Specific Search included this text,
        # which is now invalid.
        $order =~ s/ LIMIT 200//;
    }
    else {
        $order = '';  # Remove possible "reuse" identifier as unnecessary
    }
}

if ($order) {
    # Convert the value of the "order" form field into a list of columns
    # by which to sort the results.
    ORDER: for ($order) {
        /^Bug Number$/ && do {
            $order = "bug_id";
            last ORDER;
        };
        /^Importance$/ && do {
            $order = "priority,bug_severity";
            last ORDER;
        };
        /^Assignee$/ && do {
            $order = "assigned_to,bug_status,priority,bug_id";
            last ORDER;
        };
        /^Last Changed$/ && do {
            $order = "changeddate,bug_status,priority,assigned_to,bug_id";
            last ORDER;
        };
        do {
            my (@order, @invalid_fragments);

            # A custom list of columns.  Make sure each column is valid.
            foreach my $fragment (split(/,/, $order)) {
                $fragment = trim($fragment);
                next unless $fragment;
                my ($column_name, $direction) = split_order_term($fragment);
                $column_name = translate_old_column($column_name);

                # Special handlings for certain columns
                next if $column_name eq 'relevance' && !$fulltext;
                                
                if (exists $columns->{$column_name}) {
                    $direction = " $direction" if $direction;
                    push(@order, "$column_name$direction");
                }
                else {
                    push(@invalid_fragments, $fragment);
                }
            }
            if (scalar @invalid_fragments) {
                $vars->{'message'} = 'invalid_column_name';
                $vars->{'invalid_fragments'} = \@invalid_fragments;
            }

            $order = join(",", @order);
            # Now that we have checked that all columns in the order are valid,
            # detaint the order string.
            trick_taint($order) if $order;
        };
    }
}

if (!$order) {
    # DEFAULT
    $order = "bug_status,priority,assigned_to,bug_id";
}

my @orderstrings = split(/,\s*/, $order);

if ($fulltext and grep { /^relevance/ } @orderstrings) {
    $vars->{'message'} = 'buglist_sorted_by_relevance'
}

# In the HTML interface, by default, we limit the returned results,
# which speeds up quite a few searches where people are really only looking
# for the top results.
if ($format->{'extension'} eq 'html' && !defined $params->param('limit')) {
    $params->param('limit', Bugzilla->params->{'default_search_limit'});
    $vars->{'default_limited'} = 1;
}

# Generate the basic SQL query that will be used to generate the bug list.
my $search = new Bugzilla::Search('fields' => \@selectcolumns, 
                                  'params' => scalar $params->Vars,
                                  'order'  => \@orderstrings,
                                  'sharer' => $sharer_id);
my $query = $search->sql;
$vars->{'search_description'} = $search->search_description;

# We don't want saved searches and other buglist things to save
# our default limit.
$params->delete('limit') if $vars->{'default_limited'};

################################################################################
# Query Execution
################################################################################

if ($cgi->param('debug')
    && Bugzilla->params->{debug_group}
    && Bugzilla->user->in_group(Bugzilla->params->{debug_group})
) {
    $vars->{'debug'} = 1;
    $vars->{'query'} = $query;
    # Explains are limited to admins because you could use them to figure
    # out how many hidden bugs are in a particular product (by doing
    # searches and looking at the number of rows the explain says it's
    # examining).
    if (Bugzilla->user->in_group('admin')) {
        $vars->{'query_explain'} = $dbh->bz_explain($query);
    }
}

# Time to use server push to display an interim message to the user until
# the query completes and we can display the bug list.
if ($serverpush) {
    print $cgi->multipart_init();
    print $cgi->multipart_start(-type => 'text/html');

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("list/server-push.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    # Under mod_perl, flush stdout so that the page actually shows up.
    if ($ENV{MOD_PERL}) {
        require Apache2::RequestUtil;
        Apache2::RequestUtil->request->rflush();
    }

    # Don't do multipart_end() until we're ready to display the replacement
    # page, otherwise any errors that happen before then (like SQL errors)
    # will result in a blank page being shown to the user instead of the error.
}

# Connect to the shadow database if this installation is using one to improve
# query performance.
$dbh = Bugzilla->switch_to_shadow_db();

# Normally, we ignore SIGTERM and SIGPIPE, but we need to
# respond to them here to prevent someone DOSing us by reloading a query
# a large number of times.
$::SIG{TERM} = 'DEFAULT';
$::SIG{PIPE} = 'DEFAULT';

# Execute the query.
my $buglist_sth = $dbh->prepare($query);
$buglist_sth->execute();


################################################################################
# Results Retrieval
################################################################################

# Retrieve the query results one row at a time and write the data into a list
# of Perl records.

# If we're doing time tracking, then keep totals for all bugs.
my $percentage_complete = grep($_ eq 'percentage_complete', @displaycolumns);
my $estimated_time      = grep($_ eq 'estimated_time', @displaycolumns);
my $remaining_time      = grep($_ eq 'remaining_time', @displaycolumns)
                            || $percentage_complete;
my $actual_time         = grep($_ eq 'actual_time', @displaycolumns)
                            || $percentage_complete;

my $time_info = { 'estimated_time' => 0,
                  'remaining_time' => 0,
                  'actual_time' => 0,
                  'percentage_complete' => 0,
                  'time_present' => ($estimated_time || $remaining_time ||
                                     $actual_time || $percentage_complete),
                };
    
my $bugowners = {};
my $bugproducts = {};
my $bugstatuses = {};
my @bugidlist;

my @bugs; # the list of records

while (my @row = $buglist_sth->fetchrow_array()) {
    my $bug = {}; # a record

    # Slurp the row of data into the record.
    # The second from last column in the record is the number of groups
    # to which the bug is restricted.
    foreach my $column (@selectcolumns) {
        $bug->{$column} = shift @row;
    }

    # Process certain values further (i.e. date format conversion).
    if ($bug->{'changeddate'}) {
        $bug->{'changeddate'} =~ 
            s/^(\d{4})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})$/$1-$2-$3 $4:$5:$6/;

        $bug->{'changedtime'} = $bug->{'changeddate'}; # for iCalendar and Atom
        $bug->{'changeddate'} = DiffDate($bug->{'changeddate'});
    }

    if ($bug->{'opendate'}) {
        $bug->{'opentime'} = $bug->{'opendate'}; # for iCalendar
        $bug->{'opendate'} = DiffDate($bug->{'opendate'});
    }

    # Record the assignee, product, and status in the big hashes of those things.
    $bugowners->{$bug->{'assigned_to'}} = 1 if $bug->{'assigned_to'};
    $bugproducts->{$bug->{'product'}} = 1 if $bug->{'product'};
    $bugstatuses->{$bug->{'bug_status'}} = 1 if $bug->{'bug_status'};

    $bug->{'secure_mode'} = undef;

    # Add the record to the list.
    push(@bugs, $bug);

    # Add id to list for checking for bug privacy later
    push(@bugidlist, $bug->{'bug_id'});

    # Compute time tracking info.
    $time_info->{'estimated_time'} += $bug->{'estimated_time'} if ($estimated_time);
    $time_info->{'remaining_time'} += $bug->{'remaining_time'} if ($remaining_time);
    $time_info->{'actual_time'}    += $bug->{'actual_time'}    if ($actual_time);
}

# Check for bug privacy and set $bug->{'secure_mode'} to 'implied' or 'manual'
# based on whether the privacy is simply product implied (by mandatory groups)
# or because of human choice
my %min_membercontrol;
if (@bugidlist) {
    my $sth = $dbh->prepare(
        "SELECT DISTINCT bugs.bug_id, MIN(group_control_map.membercontrol) " .
          "FROM bugs " .
    "INNER JOIN bug_group_map " .
            "ON bugs.bug_id = bug_group_map.bug_id " .
     "LEFT JOIN group_control_map " .
            "ON group_control_map.product_id = bugs.product_id " .
           "AND group_control_map.group_id = bug_group_map.group_id " .
         "WHERE " . $dbh->sql_in('bugs.bug_id', \@bugidlist) . 
            $dbh->sql_group_by('bugs.bug_id'));
    $sth->execute();
    while (my ($bug_id, $min_membercontrol) = $sth->fetchrow_array()) {
        $min_membercontrol{$bug_id} = $min_membercontrol || CONTROLMAPNA;
    }
    foreach my $bug (@bugs) {
        next unless defined($min_membercontrol{$bug->{'bug_id'}});
        if ($min_membercontrol{$bug->{'bug_id'}} == CONTROLMAPMANDATORY) {
            $bug->{'secure_mode'} = 'implied';
        }
        else {
            $bug->{'secure_mode'} = 'manual';
        }
    }
}

# Compute percentage complete without rounding.
my $sum = $time_info->{'actual_time'}+$time_info->{'remaining_time'};
if ($sum > 0) {
    $time_info->{'percentage_complete'} = 100*$time_info->{'actual_time'}/$sum;
}
else { # remaining_time <= 0 
    $time_info->{'percentage_complete'} = 0
}                             

################################################################################
# Template Variable Definition
################################################################################

# Define the variables and functions that will be passed to the UI template.

$vars->{'bugs'} = \@bugs;
$vars->{'buglist'} = \@bugidlist;
$vars->{'buglist_joined'} = join(',', @bugidlist);
$vars->{'columns'} = $columns;
$vars->{'displaycolumns'} = \@displaycolumns;

$vars->{'openstates'} = [BUG_STATE_OPEN];
$vars->{'closedstates'} = [map {$_->name} closed_bug_statuses()];

# The iCal file needs priorities ordered from 1 to 9 (highest to lowest)
# If there are more than 9 values, just make all the lower ones 9
if ($format->{'extension'} eq 'ics') {
    my $n = 1;
    $vars->{'ics_priorities'} = {};
    my $priorities = get_legal_field_values('priority');
    foreach my $p (@$priorities) {
        $vars->{'ics_priorities'}->{$p} = ($n > 9) ? 9 : $n++;
    }
}

$vars->{'order'} = $order;
$vars->{'caneditbugs'} = 1;
$vars->{'time_info'} = $time_info;

if (!Bugzilla->user->in_group('editbugs')) {
    foreach my $product (keys %$bugproducts) {
        my $prod = new Bugzilla::Product({name => $product});
        if (!Bugzilla->user->in_group('editbugs', $prod->id)) {
            $vars->{'caneditbugs'} = 0;
            last;
        }
    }
}

my @bugowners = keys %$bugowners;
if (scalar(@bugowners) > 1 && Bugzilla->user->in_group('editbugs')) {
    my $suffix = Bugzilla->params->{'emailsuffix'};
    map(s/$/$suffix/, @bugowners) if $suffix;
    my $bugowners = join(",", @bugowners);
    $vars->{'bugowners'} = $bugowners;
}

# Whether or not to split the column titles across two rows to make
# the list more compact.
$vars->{'splitheader'} = $cgi->cookie('SPLITHEADER') ? 1 : 0;

$vars->{'quip'} = GetQuip();
$vars->{'currenttime'} = localtime(time());

# See if there's only one product in all the results (or only one product
# that we searched for), which allows us to provide more helpful links.
my @products = keys %$bugproducts;
my $one_product;
if (scalar(@products) == 1) {
    $one_product = new Bugzilla::Product({ name => $products[0] });
}
# This is used in the "Zarroo Boogs" case.
elsif (my @product_input = $cgi->param('product')) {
    if (scalar(@product_input) == 1 and $product_input[0] ne '') {
        $one_product = new Bugzilla::Product({ name => $cgi->param('product') });
    }
}
# We only want the template to use it if the user can actually 
# enter bugs against it.
if ($one_product && Bugzilla->user->can_enter_product($one_product)) {
    $vars->{'one_product'} = $one_product;
}

# The following variables are used when the user is making changes to multiple bugs.
if ($dotweak && scalar @bugs) {
    if (!$vars->{'caneditbugs'}) {
        _close_standby_message('text/html', 'inline', $serverpush);
        ThrowUserError('auth_failure', {group  => 'editbugs',
                                        action => 'modify',
                                        object => 'multiple_bugs'});
    }
    $vars->{'dotweak'} = 1;
  
    # issue_session_token needs to write to the master DB.
    Bugzilla->switch_to_main_db();
    $vars->{'token'} = issue_session_token('buglist_mass_change');
    Bugzilla->switch_to_shadow_db();

    $vars->{'products'} = Bugzilla->user->get_enterable_products;
    $vars->{'platforms'} = get_legal_field_values('rep_platform');
    $vars->{'op_sys'} = get_legal_field_values('op_sys');
    $vars->{'priorities'} = get_legal_field_values('priority');
    $vars->{'severities'} = get_legal_field_values('bug_severity');
    $vars->{'resolutions'} = get_legal_field_values('resolution');

    # Convert bug statuses to their ID.
    my @bug_statuses = map {$dbh->quote($_)} keys %$bugstatuses;
    my $bug_status_ids =
      $dbh->selectcol_arrayref('SELECT id FROM bug_status
                               WHERE ' . $dbh->sql_in('value', \@bug_statuses));

    # This query collects new statuses which are common to all current bug statuses.
    # It also accepts transitions where the bug status doesn't change.
    $bug_status_ids =
      $dbh->selectcol_arrayref(
            'SELECT DISTINCT sw1.new_status
               FROM status_workflow sw1
         INNER JOIN bug_status
                 ON bug_status.id = sw1.new_status
              WHERE bug_status.isactive = 1
                AND NOT EXISTS 
                   (SELECT * FROM status_workflow sw2
                     WHERE sw2.old_status != sw1.new_status 
                           AND '
                         . $dbh->sql_in('sw2.old_status', $bug_status_ids)
                         . ' AND NOT EXISTS 
                           (SELECT * FROM status_workflow sw3
                             WHERE sw3.new_status = sw1.new_status
                                   AND sw3.old_status = sw2.old_status))');

    $vars->{'current_bug_statuses'} = [keys %$bugstatuses];
    $vars->{'new_bug_statuses'} = Bugzilla::Status->new_from_list($bug_status_ids);

    # The groups the user belongs to and which are editable for the given buglist.
    $vars->{'groups'} = GetGroups(\@products);

    # If all bugs being changed are in the same product, the user can change
    # their version and component, so generate a list of products, a list of
    # versions for the product (if there is only one product on the list of
    # products), and a list of components for the product.
    if ($one_product) {
        $vars->{'versions'} = [map($_->name, grep($_->is_active, @{ $one_product->versions }))];
        $vars->{'components'} = [map($_->name, grep($_->is_active, @{ $one_product->components }))];
        if (Bugzilla->params->{'usetargetmilestone'}) {
            $vars->{'targetmilestones'} = [map($_->name, grep($_->is_active,  
                                               @{ $one_product->milestones }))];
        }
    }
}

# If we're editing a stored query, use the existing query name as default for
# the "Remember search as" field.
$vars->{'defaultsavename'} = $cgi->param('query_based_on');

# If we did a quick search then redisplay the previously entered search 
# string in the text field.
$vars->{'quicksearch'} = $searchstring;

################################################################################
# HTTP Header Generation
################################################################################

# Generate HTTP headers

my $contenttype;
my $disposition = "inline";

if ($format->{'extension'} eq "html" && !$agent) {
    my $list_id = $cgi->param('list_id') || $cgi->param('regetlastlist');
    my $search = $user->save_last_search(
        { bugs => \@bugidlist, order => $order, vars => $vars, list_id => $list_id });
    $cgi->param('list_id', $search->id) if $search;
    $contenttype = "text/html";
}
else {
    $contenttype = $format->{'ctype'};
}

# Set 'urlquerypart' once the buglist ID is known.
$vars->{'urlquerypart'} = $params->canonicalise_query('order', 'cmdtype',
                                                      'query_based_on');

if ($format->{'extension'} eq "csv") {
    # We set CSV files to be downloaded, as they are designed for importing
    # into other programs.
    $disposition = "attachment";

    # If the user clicked the CSV link in the search results,
    # They should get the Field Description, not the column name in the db
    $vars->{'human'} = $cgi->param('human');
}

# Suggest a name for the bug list if the user wants to save it as a file.
$disposition .= "; filename=\"$filename\"";

_close_standby_message($contenttype, $disposition, $serverpush);

################################################################################
# Content Generation
################################################################################

# Generate and return the UI (HTML page) from the appropriate template.
$template->process($format->{'template'}, $vars)
  || ThrowTemplateError($template->error());


################################################################################
# Script Conclusion
################################################################################

print $cgi->multipart_final() if $serverpush;
