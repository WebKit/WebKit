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
use Bugzilla::Bug;
use Bugzilla::Constants;
use Bugzilla::Search;
use Bugzilla::Search::Saved;
use Bugzilla::User;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Product;
use Bugzilla::Version;
use Bugzilla::Keyword;
use Bugzilla::Field;
use Bugzilla::Token;

###############
# Subroutines #
###############

sub get_product_values {
    my ($products, $field, $vars) = @_;
    my @all_values = map { @{ $_->$field } } @$products;

    my (@unique, %duplicates, %duplicate_count, %seen);
    foreach my $value (@all_values) {
        my $lc_name = lc($value->name);
        if ($seen{$lc_name}) {
            $duplicate_count{$seen{$lc_name}->id}++;
            $duplicates{$value->id} = $seen{$lc_name};
            next;
        }
        push(@unique, $value);
        $seen{$lc_name} = $value;
    }

    $field =~ s/s$//;
    if ($field eq 'version') {
        @unique = sort { vers_cmp(lc($a->name), lc($b->name)) } @unique;
    }
    else {
        @unique = sort { lc($a->name) cmp lc($b->name) } @unique;
    }

    $field = 'target_milestone' if $field eq 'milestone';
    $vars->{duplicates}->{$field} = \%duplicates;
    $vars->{duplicate_count}->{$field} = \%duplicate_count;
    # "component" is a reserved word in Template Toolkit.
    $field = 'component_' if $field eq 'component';
    $vars->{$field} = \@unique;
}

###############
# Main Script #
###############

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};
my $buffer = $cgi->query_string();

my $user = Bugzilla->login();
my $userid = $user->id;

if ($cgi->param('nukedefaultquery')) {
    if ($userid) {
        my $token = $cgi->param('token');
        check_hash_token($token, ['nukedefaultquery']);
        my $named_queries = Bugzilla::Search::Saved->match(
            { userid => $userid, name => DEFAULT_QUERY_NAME });
        if (@$named_queries) {
            $named_queries->[0]->remove_from_db();
        }
    }
    $buffer = "";
}

# We are done with changes committed to the DB.
$dbh = Bugzilla->switch_to_shadow_db;

my $userdefaultquery;
if ($userid) {
    $userdefaultquery = $dbh->selectrow_array(
        "SELECT query FROM namedqueries " .
         "WHERE userid = ? AND name = ?", 
         undef, ($userid, DEFAULT_QUERY_NAME));
}

local our %default;

# We pass the defaults as a hash of references to arrays. For those
# Items which are single-valued, the template should only reference [0]
# and ignore any multiple values.
sub PrefillForm {
    my ($buf) = @_;
    my $cgi = Bugzilla->cgi;
    $buf = new Bugzilla::CGI($buf);
    my $foundone = 0;

    # If there are old-style boolean charts in the URL (from an old saved
    # search or from an old link on the web somewhere) then convert them
    # to the new "custom search" format so that the form is populated
    # properly.
    my $any_boolean_charts = grep { /^field-?\d+/ } $buf->param();
    if ($any_boolean_charts) {
        my $search = new Bugzilla::Search(params => scalar $buf->Vars);
        $search->boolean_charts_to_custom_search($buf);
    }

    # Query parameters that don't represent form fields on this page.
    my @skip = qw(format query_format list_id columnlist);

    # Iterate over the URL parameters
    foreach my $name ($buf->param()) {
        next if grep { $_ eq $name } @skip;
        $foundone = 1;
        my @values = $buf->param($name);
        
        # If the name is a single letter followed by numbers, it's part
        # of Custom Search. We store these as an array of hashes.
        if ($name =~ /^([[:lower:]])(\d+)$/) {
            $default{'custom_search'}->[$2]->{$1} = $values[0];
        }
        # If the name ends in a number (which it does for the fields which
        # are part of the email searching), we use the array
        # positions to show the defaults for that number field.
        elsif ($name =~ /^(\w+)(\d)$/) {
            $default{$1}->[$2] = $values[0];
        }
        else {
            push (@{ $default{$name} }, @values);
        }
    }

    return $foundone;
}

if (!PrefillForm($buffer)) {
    # Ah-hah, there was no form stuff specified.  Do it again with the
    # default query.
    if ($userdefaultquery) {
        PrefillForm($userdefaultquery);
    } else {
        PrefillForm(Bugzilla->params->{"defaultquery"});
    }
}

# if using groups for entry, then we don't want people to see products they 
# don't have access to. Remove them from the list.
my @selectable_products = sort {lc($a->name) cmp lc($b->name)} 
                               @{$user->get_selectable_products};
Bugzilla::Product::preload(\@selectable_products);
$vars->{'product'} = \@selectable_products;

# Create the component, version and milestone lists.
foreach my $field (qw(components versions milestones)) {
    get_product_values(\@selectable_products, $field, $vars);
}

# Create data structures representing each classification
if (Bugzilla->params->{'useclassification'}) {
    $vars->{'classification'} = $user->get_selectable_classifications;
}

my @chfields;

push @chfields, "[Bug creation]";

# This is what happens when you have variables whose definition depends
# on the DB schema, and then the underlying schema changes...
foreach my $val (editable_bug_fields()) {
    if ($val eq 'classification_id') {
        $val = 'classification';
    } elsif ($val eq 'product_id') {
        $val = 'product';
    } elsif ($val eq 'component_id') {
        $val = 'component';
    }
    push @chfields, $val;
}

if ($user->is_timetracker) {
    push @chfields, "work_time";
} else {
    foreach my $tt_field (TIMETRACKING_FIELDS) {
        @chfields = grep($_ ne $tt_field, @chfields);
    }
}
@chfields = (sort(@chfields));
$vars->{'chfield'} = \@chfields;
$vars->{'bug_status'} = Bugzilla::Field->new({name => 'bug_status'})->legal_values;
$vars->{'rep_platform'} = Bugzilla::Field->new({name => 'rep_platform'})->legal_values;
$vars->{'op_sys'} = Bugzilla::Field->new({name => 'op_sys'})->legal_values;
$vars->{'priority'} = Bugzilla::Field->new({name => 'priority'})->legal_values;
$vars->{'bug_severity'} = Bugzilla::Field->new({name => 'bug_severity'})->legal_values;
$vars->{'resolution'} = Bugzilla::Field->new({name => 'resolution'})->legal_values;

# Boolean charts
my @fields = @{ Bugzilla->fields({ obsolete => 0 }) };

my %exclude_fields = ();

# If we're not in the time-tracking group, exclude time-tracking fields.
if (!$user->is_timetracker) {
    foreach my $tt_field (TIMETRACKING_FIELDS) {
        $exclude_fields{$tt_field} = 1;
    }
}

# Exclude fields turned off by params
my %param_controlled_fields = ('useqacontact'        => 'qa_contact',
                               'usetargetmilestone'  => 'target_milestone',
                               'useclassification'   => 'classification',
                               'usestatuswhiteboard' => 'status_whiteboard');

while (my ($param, $field) = each %param_controlled_fields) {
    $exclude_fields{$field} = 1 unless Bugzilla->params->{$param};
}

@fields = grep(!$exclude_fields{$_->name}, @fields);

@fields = sort {lc($a->description) cmp lc($b->description)} @fields;
unshift(@fields, { name => "noop", description => "---" });
$vars->{'fields'} = \@fields;

# Named queries
if ($userid) {
     $vars->{'namedqueries'} = $dbh->selectcol_arrayref(
           "SELECT name FROM namedqueries " .
            "WHERE userid = ? AND name != ? " .
         "ORDER BY name",
         undef, ($userid, DEFAULT_QUERY_NAME));
}

# Sort order
my $deforder;
my @orders = ('Bug Number', 'Importance', 'Assignee', 'Last Changed');

if ($cgi->cookie('LASTORDER')) {
    $deforder = "Reuse same sort as last time";
    unshift(@orders, $deforder);
}

if ($cgi->param('order')) { $deforder = $cgi->param('order') }

$vars->{'userdefaultquery'} = $userdefaultquery;
$vars->{'orders'} = \@orders;
$default{'order'} = [$deforder || 'Importance'];

if (($cgi->param('query_format') || $cgi->param('format') || "")
    eq "create-series") {
    require Bugzilla::Chart;
    $vars->{'category'} = Bugzilla::Chart::getVisibleSeries();
}

$vars->{'known_name'} = $cgi->param('known_name');
$vars->{'columnlist'} = $cgi->param('columnlist');


# Add in the defaults.
$vars->{'default'} = \%default;

$vars->{'format'} = $cgi->param('format');
$vars->{'query_format'} = $cgi->param('query_format');

# Set default page to "specific" if none provided
if (!($cgi->param('query_format') || $cgi->param('format'))) {
    if (defined $cgi->cookie('DEFAULTFORMAT')) {
        $vars->{'format'} = $cgi->cookie('DEFAULTFORMAT');
    } else {
        $vars->{'format'} = 'specific';
    }
}

# Set cookie to current format as default, but only if the format
# one that we should remember.
if (defined($vars->{'format'}) && IsValidQueryType($vars->{'format'})) {
    $cgi->send_cookie(-name => 'DEFAULTFORMAT',
                      -value => $vars->{'format'},
                      -expires => "Fri, 01-Jan-2038 00:00:00 GMT");
}

# Generate and return the UI (HTML page) from the appropriate template.
# If we submit back to ourselves (for e.g. boolean charts), we need to
# preserve format information; hence query_format taking priority over
# format.
my $format = $template->get_format("search/search", 
                                   $vars->{'query_format'} || $vars->{'format'}, 
                                   scalar $cgi->param('ctype'));

print $cgi->header($format->{'ctype'});

$template->process($format->{'template'}, $vars)
  || ThrowTemplateError($template->error());
