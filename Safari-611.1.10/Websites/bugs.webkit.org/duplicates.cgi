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
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Bug;
use Bugzilla::Field;
use Bugzilla::Product;

use constant DEFAULTS => {
    # We want to show bugs which:
    # a) Aren't CLOSED; and
    # b)  i) Aren't VERIFIED; OR
    #    ii) Were resolved INVALID/WONTFIX
    #
    # The rationale behind this is that people will eventually stop
    # reporting fixed bugs when they get newer versions of the software,
    # but if the bug is determined to be erroneous, people will still
    # keep reporting it, so we do need to show it here.
    fully_exclude_status  => ['CLOSED'],
    partly_exclude_status => ['VERIFIED'],
    except_resolution => ['INVALID', 'WONTFIX'],
    changedsince => 7,
    maxrows      => 20,
    sortby       => 'count',
};

###############
# Subroutines #
###############

# $counts is a count of exactly how many direct duplicates there are for
# each bug we're considering. $dups is a map of duplicates, from one
# bug_id to another. We go through the duplicates map ($dups) and if one bug
# in $count is a duplicate of another bug in $count, we add their counts
# together under the target bug.
sub add_indirect_dups {
    my ($counts, $dups) = @_;

    foreach my $add_from (keys %$dups) {
        my $add_to     = walk_dup_chain($dups, $add_from);
        my $add_amount = delete $counts->{$add_from} || 0;
        $counts->{$add_to} += $add_amount;
    }
}

sub walk_dup_chain {
    my ($dups, $from_id) = @_;
    my $to_id = $dups->{$from_id};
    my %seen;
    while (my $bug_id = $dups->{$to_id}) {
        if ($seen{$bug_id}) {
            warn "Duplicate loop: $to_id -> $bug_id\n";
            last;
        }
        $seen{$bug_id} = 1;
        $to_id = $bug_id;
    }
    # Optimize for future calls to add_indirect_dups.
    $dups->{$from_id} = $to_id;
    return $to_id;
}

# Get params from URL
sub formvalue {
    my ($name) = (@_);
    my $cgi = Bugzilla->cgi;
    if (defined $cgi->param($name)) {
        return $cgi->param($name);
    }
    elsif (exists DEFAULTS->{$name}) {
        return ref DEFAULTS->{$name} ? @{ DEFAULTS->{$name} } 
                                     : DEFAULTS->{$name};
    }
    return undef;
}

sub sort_duplicates {
    my ($a, $b, $sort_by) = @_;
    if ($sort_by eq 'count' or $sort_by eq 'delta') {
        return $a->{$sort_by} <=> $b->{$sort_by};
    }
    if ($sort_by =~ /^(bug_)?id$/) {
        return $a->{'bug'}->$sort_by <=> $b->{'bug'}->$sort_by;
    }
    return $a->{'bug'}->$sort_by cmp $b->{'bug'}->$sort_by;
    
}

###############
# Main Script #
###############

my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $user = Bugzilla->login();

my $dbh = Bugzilla->switch_to_shadow_db();

my $changedsince = formvalue("changedsince");
my $maxrows = formvalue("maxrows");
my $openonly = formvalue("openonly");
my $sortby = formvalue("sortby");
if (!grep(lc($_) eq lc($sortby), qw(count delta id))) {
    Bugzilla::Field->check($sortby);
}
my $reverse = formvalue("reverse");
# Reverse count and delta by default.
if (!defined $reverse) {
    if ($sortby eq 'count' or $sortby eq 'delta') {
        $reverse = 1;
    }
    else {
        $reverse = 0;
    }
}
my @query_products = $cgi->param('product');
my $sortvisible = formvalue("sortvisible");
my @bugs;
if ($sortvisible) {
    my @limit_to_ids = (split(/[:,]/, formvalue("bug_id") || ''));
    @bugs = @{ Bugzilla::Bug->new_from_list(\@limit_to_ids) };
    @bugs = @{ $user->visible_bugs(\@bugs) };
}

# Make sure all products are valid.
@query_products = map { Bugzilla::Product->check($_) } @query_products;

# Small backwards-compatibility hack, dated 2002-04-10.
$sortby = "count" if $sortby eq "dup_count";

my $origmaxrows = $maxrows;
detaint_natural($maxrows)
  || ThrowUserError("invalid_maxrows", { maxrows => $origmaxrows});

my $origchangedsince = $changedsince;
detaint_natural($changedsince)
  || ThrowUserError("invalid_changedsince", 
                    { changedsince => $origchangedsince });

my %total_dups = @{$dbh->selectcol_arrayref(
    "SELECT dupe_of, COUNT(dupe)
       FROM duplicates
   GROUP BY dupe_of", {Columns => [1,2]})};

my %dupe_relation = @{$dbh->selectcol_arrayref(
    "SELECT dupe, dupe_of FROM duplicates
      WHERE dupe IN (SELECT dupe_of FROM duplicates)",
    {Columns => [1,2]})};
add_indirect_dups(\%total_dups, \%dupe_relation);

my $reso_field_id = get_field_id('resolution');
my %since_dups = @{$dbh->selectcol_arrayref(
    "SELECT dupe_of, COUNT(dupe)
       FROM duplicates INNER JOIN bugs_activity 
                       ON bugs_activity.bug_id = duplicates.dupe 
      WHERE added = 'DUPLICATE' AND fieldid = ? 
            AND bug_when >= "
                . $dbh->sql_date_math('LOCALTIMESTAMP(0)', '-', '?', 'DAY') .
 " GROUP BY dupe_of", {Columns=>[1,2]},
    $reso_field_id, $changedsince)};
add_indirect_dups(\%since_dups, \%dupe_relation);

# Enforce the MOST_FREQUENT_THRESHOLD constant and the "bug_id" cgi param.
foreach my $id (keys %total_dups) {
    if ($total_dups{$id} < MOST_FREQUENT_THRESHOLD) {
        delete $total_dups{$id};
        next;
    }
    if ($sortvisible and !grep($_->id == $id, @bugs)) {
        delete $total_dups{$id};
    }
}

if (!@bugs) {
    @bugs = @{ Bugzilla::Bug->new_from_list([keys %total_dups]) };
    @bugs = @{ $user->visible_bugs(\@bugs) };
}

my @fully_exclude_status = formvalue('fully_exclude_status');
my @partly_exclude_status = formvalue('partly_exclude_status');
my @except_resolution = formvalue('except_resolution');

# Filter bugs by criteria
my @result_bugs;
foreach my $bug (@bugs) {
    # It's possible, if somebody specified a bug ID that wasn't a dup
    # in the "buglist" parameter and specified $sortvisible that there
    # would be bugs in the list with 0 dups, so we want to avoid that.
    next if !$total_dups{$bug->id};

    next if ($openonly and !$bug->isopened);
    # If the bug has a status in @fully_exclude_status, we skip it,
    # no question.
    next if grep($_ eq $bug->bug_status, @fully_exclude_status);
    # If the bug has a status in @partly_exclude_status, we skip it...
    if (grep($_ eq $bug->bug_status, @partly_exclude_status)) {
        # ...unless it has a resolution in @except_resolution.
        next if !grep($_ eq $bug->resolution, @except_resolution);
    }

    if (scalar @query_products) {
        next if !grep($_->id == $bug->product_id, @query_products);
    }

    # Note: maximum row count is dealt with later.
    push (@result_bugs, { bug => $bug,
                          count => $total_dups{$bug->id},
                          delta => $since_dups{$bug->id} || 0 });
}
@bugs = @result_bugs;
@bugs = sort { sort_duplicates($a, $b, $sortby) } @bugs;
if ($reverse) {
    @bugs = reverse @bugs;
}
@bugs = @bugs[0..$maxrows-1] if scalar(@bugs) > $maxrows;

my %vars = (
    bugs     => \@bugs,
    bug_ids  => [map { $_->{'bug'}->id } @bugs],
    sortby   => $sortby,
    openonly => $openonly,
    maxrows  => $maxrows,
    reverse  => $reverse,
    format   => scalar $cgi->param('format'),
    product  => [map { $_->name } @query_products],
    sortvisible  => $sortvisible,
    changedsince => $changedsince,
);

my $format = $template->get_format("reports/duplicates", $vars{'format'});
print $cgi->header;

# Generate and return the UI (HTML page) from the appropriate template.
$template->process($format->{'template'}, \%vars)
  || ThrowTemplateError($template->error());
