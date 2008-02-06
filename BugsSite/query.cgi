#!/usr/bin/perl -wT
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
#                 David Gardiner <david.gardiner@unisa.edu.au>
#                 Matthias Radestock <matthias@sorted.org>
#                 Gervase Markham <gerv@gerv.net>
#                 Byron Jones <bugzilla@glob.com.au>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

use strict;
use lib ".";

require "CGI.pl";

use Bugzilla::Constants;
use Bugzilla::Search;
use Bugzilla::User;

use vars qw(
    @CheckOptionValues
    @legal_resolution
    @legal_bug_status
    @legal_components
    @legal_keywords
    @legal_opsys
    @legal_platform
    @legal_priority
    @legal_product
    @legal_severity
    @legal_target_milestone
    @legal_versions
    @log_columns
    %versions
    %components
    $template
    $vars
);

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;

if ($cgi->param("GoAheadAndLogIn")) {
    # We got here from a login page, probably from relogin.cgi.  We better
    # make sure the password is legit.
    Bugzilla->login(LOGIN_REQUIRED);
} else {
    Bugzilla->login();
}

my $userid = Bugzilla->user->id;

# Backwards compatibility hack -- if there are any of the old QUERY_*
# cookies around, and we are logged in, then move them into the database
# and nuke the cookie. This is required for Bugzilla 2.8 and earlier.
if ($userid) {
    my @oldquerycookies;
    foreach my $i ($cgi->cookie()) {
        if ($i =~ /^QUERY_(.*)$/) {
            push(@oldquerycookies, [$1, $i, $cgi->cookie($i)]);
        }
    }
    if (defined $cgi->cookie('DEFAULTQUERY')) {
        push(@oldquerycookies, [DEFAULT_QUERY_NAME, 'DEFAULTQUERY',
                                $cgi->cookie('DEFAULTQUERY')]);
    }
    if (@oldquerycookies) {
        foreach my $ref (@oldquerycookies) {
            my ($name, $cookiename, $value) = (@$ref);
            if ($value) {
                # If the query name contains invalid characters, don't import.
                $name =~ /[<>&]/ && next;
                trick_taint($name);
                $dbh->bz_lock_tables('namedqueries WRITE');
                my $query = $dbh->selectrow_array(
                    "SELECT query FROM namedqueries " .
                     "WHERE userid = ? AND name = ?",
                     undef, ($userid, $name));
                if (!$query) {
                    $dbh->do("INSERT INTO namedqueries " .
                            "(userid, name, query) VALUES " .
                            "(?, ?, ?)", undef, ($userid, $name, $value));
                }
                $dbh->bz_unlock_tables();
            }
            $cgi->remove_cookie($cookiename);
        }
    }
}

if ($cgi->param('nukedefaultquery')) {
    if ($userid) {
        $dbh->do("DELETE FROM namedqueries" .
                 " WHERE userid = ? AND name = ?", 
                 undef, ($userid, DEFAULT_QUERY_NAME));
    }
    $::buffer = "";
}

my $userdefaultquery;
if ($userid) {
    $userdefaultquery = $dbh->selectrow_array(
        "SELECT query FROM namedqueries " .
         "WHERE userid = ? AND name = ?", 
         undef, ($userid, DEFAULT_QUERY_NAME));
}

my %default;

# We pass the defaults as a hash of references to arrays. For those
# Items which are single-valued, the template should only reference [0]
# and ignore any multiple values.
sub PrefillForm {
    my ($buf) = (@_);
    my $foundone = 0;

    # Nothing must be undef, otherwise the template complains.
    foreach my $name ("bug_status", "resolution", "assigned_to",
                      "rep_platform", "priority", "bug_severity",
                      "classification", "product", "reporter", "op_sys",
                      "component", "version", "chfield", "chfieldfrom",
                      "chfieldto", "chfieldvalue", "target_milestone",
                      "email", "emailtype", "emailreporter",
                      "emailassigned_to", "emailcc", "emailqa_contact",
                      "emaillongdesc", "content",
                      "changedin", "votes", "short_desc", "short_desc_type",
                      "long_desc", "long_desc_type", "bug_file_loc",
                      "bug_file_loc_type", "status_whiteboard",
                      "status_whiteboard_type", "bug_id",
                      "bugidtype", "keywords", "keywords_type",
                      "x_axis_field", "y_axis_field", "z_axis_field",
                      "chart_format", "cumulate", "x_labels_vertical",
                      "category", "subcategory", "name", "newcategory",
                      "newsubcategory", "public", "frequency") 
    {
        # This is a bit of a hack. The default, empty list has 
        # three entries to accommodate the needs of the email fields -
        # we use each position to denote the relevant field. Array
        # position 0 is unused for email fields because the form 
        # parameters historically started at 1.
        $default{$name} = ["", "", ""];
    }
 
 
    # Iterate over the URL parameters
    foreach my $item (split(/\&/, $buf)) {
        my @el = split(/=/, $item);
        my $name = $el[0];
        my $value;
        if ($#el > 0) {
            $value = url_decode($el[1]);
        } else {
            $value = "";
        }
        
        # If the name begins with field, type, or value, then it is part of
        # the boolean charts. Because these are built different than the rest
        # of the form, we don't need to save a default value. We do, however,
        # need to indicate that we found something so the default query isn't
        # added in if all we have are boolean chart items.
        if ($name =~ m/^(?:field|type|value)/) {
            $foundone = 1;
        }
        # If the name ends in a number (which it does for the fields which
        # are part of the email searching), we use the array
        # positions to show the defaults for that number field.
        elsif ($name =~ m/^(.+)(\d)$/ && defined($default{$1})) {
            $foundone = 1;
            $default{$1}->[$2] = $value;
        }
        # If there's no default yet, we replace the blank string.
        elsif (defined($default{$name}) && $default{$name}->[0] eq "") {
            $foundone = 1;
            $default{$name} = [$value]; 
        } 
        # If there's already a default, we push on the new value.
        elsif (defined($default{$name})) {
            push (@{$default{$name}}, $value);
        }        
    }        
    return $foundone;
}


if (!PrefillForm($::buffer)) {
    # Ah-hah, there was no form stuff specified.  Do it again with the
    # default query.
    if ($userdefaultquery) {
        PrefillForm($userdefaultquery);
    } else {
        PrefillForm(Param("defaultquery"));
    }
}

if ($default{'chfieldto'}->[0] eq "") {
    $default{'chfieldto'} = ["Now"];
}

GetVersionTable();

# if using groups for entry, then we don't want people to see products they 
# don't have access to. Remove them from the list.

my @products = ();
my %component_set;
my %version_set;
my %milestone_set;
foreach my $p (GetSelectableProducts()) {
    # We build up boolean hashes in the "-set" hashes for each of these things 
    # before making a list because there may be duplicates names across products.
    push @products, $p;
    if ($::components{$p}) {
        foreach my $c (@{$::components{$p}}) {
            $component_set{$c} = 1;
        }
    }
    foreach my $v (@{$::versions{$p}}) {
        $version_set{$v} = 1;
    }
    foreach my $m (@{$::target_milestone{$p}}) {
        $milestone_set{$m} = 1;
    }
}

# @products is now all the products we are ever concerned with, as a list
# %x_set is now a unique "list" of the relevant components/versions/tms
@products = sort { lc($a) cmp lc($b) } @products;

# Create the component, version and milestone lists.
my @components = ();
my @versions = ();
my @milestones = ();
foreach my $c (@::legal_components) {
    if ($component_set{$c}) {
        push @components, $c;
    }
}
foreach my $v (@::legal_versions) {
    if ($version_set{$v}) {
        push @versions, $v;
    }
}
foreach my $m (@::legal_target_milestone) {
    if ($milestone_set{$m}) {
        push @milestones, $m;
    }
}

# Create data structures representing each product.
for (my $i = 0; $i < @products; ++$i) {
    my $p = $products[$i];
    
    # Bug 190611: band-aid to avoid crashing with no versions defined
    if (!defined ($::components{$p})) {
        $::components{$p} = [];
    }
    
    # Create hash to hold attributes for each product.
    my %product = (
        'name'       => $p,
        'components' => [ sort { lc($a) cmp lc($b) } @{$::components{$p}} ],
        'versions'   => [ sort { lc($a) cmp lc($b) } @{$::versions{$p}}   ]
    );
    
    if (Param('usetargetmilestone')) {
        # Sorting here is required for ordering multiple selections 
        # correctly; see bug 97736 for discussion on how to fix this
        $product{'milestones'} =  
                      [ sort { lc($a) cmp lc($b) } @{$::target_milestone{$p}} ];
    }
    
    # Assign hash back to product array.
    $products[$i] = \%product;
}
$vars->{'product'} = \@products;

# Create data structures representing each classification
if (Param('useclassification')) {
    my @classifications = ();

    foreach my $c (GetSelectableClassifications()) {
        # Create hash to hold attributes for each classification.
        my %classification = (
            'name'       => $c,
            'products'   => [ GetSelectableProducts(0,$c) ]
        );
        # Assign hash back to classification array.
        push @classifications, \%classification;
    }
    $vars->{'classification'} = \@classifications;
}

# We use 'component_' because 'component' is a Template Toolkit reserved word.
$vars->{'component_'} = \@components;

$vars->{'version'} = \@versions;

if (Param('usetargetmilestone')) {
    $vars->{'target_milestone'} = \@milestones;
}

$vars->{'have_keywords'} = scalar(@::legal_keywords);

push @::legal_resolution, "---"; # Oy, what a hack.
shift @::legal_resolution; 
      # Another hack - this array contains "" for some reason. See bug 106589.
$vars->{'resolution'} = \@::legal_resolution;

my @chfields;

push @chfields, "[Bug creation]";

# This is what happens when you have variables whose definition depends
# on the DB schema, and then the underlying schema changes...
foreach my $val (@::log_columns) {
    if ($val eq 'classification_id') {
        $val = 'classification';
    } elsif ($val eq 'product_id') {
        $val = 'product';
    } elsif ($val eq 'component_id') {
        $val = 'component';
    }
    push @chfields, $val;
}

if (UserInGroup(Param('timetrackinggroup'))) {
    push @chfields, "work_time";
} else {
    @chfields = grep($_ ne "estimated_time", @chfields);
    @chfields = grep($_ ne "remaining_time", @chfields);
}
@chfields = (sort(@chfields));
$vars->{'chfield'} = \@chfields;
$vars->{'bug_status'} = \@::legal_bug_status;
$vars->{'rep_platform'} = \@::legal_platform;
$vars->{'op_sys'} = \@::legal_opsys;
$vars->{'priority'} = \@::legal_priority;
$vars->{'bug_severity'} = \@::legal_severity;

# Boolean charts
my @fields;
push(@fields, { name => "noop", description => "---" });
push(@fields, $dbh->bz_get_field_defs());
$vars->{'fields'} = \@fields;

# Creating new charts - if the cmd-add value is there, we define the field
# value so the code sees it and creates the chart. It will attempt to select
# "xyzzy" as the default, and fail. This is the correct behaviour.
foreach my $cmd (grep(/^cmd-/, $cgi->param)) {
    if ($cmd =~ /^cmd-add(\d+)-(\d+)-(\d+)$/) {
        $cgi->param(-name => "field$1-$2-$3", -value => "xyzzy");
    }
}

if (!$cgi->param('field0-0-0')) {
    $cgi->param(-name => 'field0-0-0', -value => "xyzzy");
}

# Create data structure of boolean chart info. It's an array of arrays of
# arrays - with the inner arrays having three members - field, type and
# value.
my @charts;
for (my $chart = 0; $cgi->param("field$chart-0-0"); $chart++) {
    my @rows;
    for (my $row = 0; $cgi->param("field$chart-$row-0"); $row++) {
        my @cols;
        for (my $col = 0; $cgi->param("field$chart-$row-$col"); $col++) {
            push(@cols, { field => $cgi->param("field$chart-$row-$col"),
                          type => $cgi->param("type$chart-$row-$col") || 'noop',
                          value => $cgi->param("value$chart-$row-$col") || '' });
        }
        push(@rows, \@cols);
    }
    push(@charts, {'rows' => \@rows, 'negate' => scalar($cgi->param("negate$chart")) });
}

$default{'charts'} = \@charts;

# Named queries
if ($userid) {
     $vars->{'namedqueries'} = $dbh->selectcol_arrayref(
           "SELECT name FROM namedqueries " .
            "WHERE userid = ? AND name != ?" .
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
$default{'querytype'} = $deforder || 'Importance';

if (($cgi->param('query_format') || $cgi->param('format') || "")
    eq "create-series") {
    require Bugzilla::Chart;
    $vars->{'category'} = Bugzilla::Chart::getVisibleSeries();
}

$vars->{'known_name'} = $cgi->param('known_name');


# Add in the defaults.
$vars->{'default'} = \%default;

$vars->{'format'} = $cgi->param('format');
$vars->{'query_format'} = $cgi->param('query_format');

# Set default page to "specific" if none proviced
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
my $format = GetFormat("search/search", 
                       $vars->{'query_format'} || $vars->{'format'}, 
                       scalar $cgi->param('ctype'));

print $cgi->header($format->{'ctype'});

$template->process($format->{'template'}, $vars)
  || ThrowTemplateError($template->error());
