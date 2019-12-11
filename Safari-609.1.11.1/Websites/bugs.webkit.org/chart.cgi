#!/usr/bin/perl -T
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# Glossary:
# series:   An individual, defined set of data plotted over time.
# data set: What a series is called in the UI.
# line:     A set of one or more series, to be summed and drawn as a single
#           line when the series is plotted.
# chart:    A set of lines
#
# So when you select rows in the UI, you are selecting one or more lines, not
# series.

# Generic Charting TODO:
#
# JS-less chart creation - hard.
# Broken image on error or no data - need to do much better.
# Centralise permission checking, so Bugzilla->user->in_group('editbugs')
#   not scattered everywhere.
# User documentation :-)
#
# Bonus:
# Offer subscription when you get a "series already exists" error?

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::CGI;
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Chart;
use Bugzilla::Series;
use Bugzilla::Token;

# For most scripts we don't make $cgi and $template global variables. But
# when preparing Bugzilla for mod_perl, this script used these
# variables in so many subroutines that it was easier to just
# make them globals.
local our $cgi = Bugzilla->cgi;
local our $template = Bugzilla->template;
local our $vars = {};
my $dbh = Bugzilla->dbh;

my $user = Bugzilla->login(LOGIN_REQUIRED);

if (!Bugzilla->feature('new_charts')) {
    ThrowUserError('feature_disabled', { feature => 'new_charts' });
}

# Go back to query.cgi if we are adding a boolean chart parameter.
if (grep(/^cmd-/, $cgi->param())) {
    my $params = $cgi->canonicalise_query("format", "ctype", "action");
    print $cgi->redirect("query.cgi?format=" . $cgi->param('query_format') .
                                               ($params ? "&$params" : ""));
    exit;
}

my $action = $cgi->param('action');
my $series_id = $cgi->param('series_id');
$vars->{'doc_section'} = 'using/reports-and-charts.html#charts';

# Because some actions are chosen by buttons, we can't encode them as the value
# of the action param, because that value is localization-dependent. So, we
# encode it in the name, as "action-<action>". Some params even contain the
# series_id they apply to (e.g. subscribe, unsubscribe).
my @actions = grep(/^action-/, $cgi->param());
if ($actions[0] && $actions[0] =~ /^action-([^\d]+)(\d*)$/) {
    $action = $1;
    $series_id = $2 if $2;
}

$action ||= "assemble";

# Go to buglist.cgi if we are doing a search.
if ($action eq "search") {
    my $params = $cgi->canonicalise_query("format", "ctype", "action");
    print $cgi->redirect("buglist.cgi" . ($params ? "?$params" : ""));
    exit;
}

$user->in_group(Bugzilla->params->{"chartgroup"})
  || ThrowUserError("auth_failure", {group  => Bugzilla->params->{"chartgroup"},
                                     action => "use",
                                     object => "charts"});

# Only admins may create public queries
$user->in_group('admin') || $cgi->delete('public');

# All these actions relate to chart construction.
if ($action =~ /^(assemble|add|remove|sum|subscribe|unsubscribe)$/) {
    # These two need to be done before the creation of the Chart object, so
    # that the changes they make will be reflected in it.
    if ($action =~ /^subscribe|unsubscribe$/) {
        detaint_natural($series_id) || ThrowCodeError("invalid_series_id");
        my $series = new Bugzilla::Series($series_id);
        $series->$action($user->id);
    }

    my $chart = new Bugzilla::Chart($cgi);

    if ($action =~ /^remove|sum$/) {
        $chart->$action(getSelectedLines());
    }
    elsif ($action eq "add") {
        my @series_ids = getAndValidateSeriesIDs();
        $chart->add(@series_ids);
    }

    view($chart);
}
elsif ($action eq "plot") {
    plot();
}
elsif ($action eq "wrap") {
    # For CSV "wrap", we go straight to "plot".
    if ($cgi->param('ctype') && $cgi->param('ctype') eq "csv") {
        plot();
    }
    else {
        wrap();
    }
}
elsif ($action eq "create") {
    assertCanCreate($cgi);
    my $token = $cgi->param('token');
    check_hash_token($token, ['create-series']);
    
    my $series = new Bugzilla::Series($cgi);

    ThrowUserError("series_already_exists", {'series' => $series})
      if $series->existsInDatabase;

    $series->writeToDatabase();
    $vars->{'message'} = "series_created";
    $vars->{'series'} = $series;

    my $chart = new Bugzilla::Chart($cgi);
    view($chart);
}
elsif ($action eq "edit") {
    my $series = assertCanEdit($series_id);
    edit($series);
}
elsif ($action eq "alter") {
    my $series = assertCanEdit($series_id);
    my $token = $cgi->param('token');
    check_hash_token($token, [$series->id, $series->name]);
    # XXX - This should be replaced by $series->set_foo() methods.
    $series = new Bugzilla::Series($cgi);

    # We need to check if there is _another_ series in the database with
    # our (potentially new) name. So we call existsInDatabase() to see if
    # the return value is us or some other series we need to avoid stomping
    # on.
    my $id_of_series_in_db = $series->existsInDatabase();
    if (defined($id_of_series_in_db) && 
        $id_of_series_in_db != $series->{'series_id'}) 
    {
        ThrowUserError("series_already_exists", {'series' => $series});
    }
    
    $series->writeToDatabase();
    $vars->{'changes_saved'} = 1;
    
    edit($series);
}
elsif ($action eq "confirm-delete") {
    $vars->{'series'} = assertCanEdit($series_id);

    print $cgi->header();
    $template->process("reports/delete-series.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}
elsif ($action eq "delete") {
    my $series = assertCanEdit($series_id);
    my $token = $cgi->param('token');
    check_hash_token($token, [$series->id, $series->name]);

    $dbh->bz_start_transaction();

    $series->remove_from_db();
    # Remove (sub)categories which no longer have any series.
    foreach my $cat (qw(category subcategory)) {
        my $is_used = $dbh->selectrow_array("SELECT COUNT(*) FROM series WHERE $cat = ?",
                                             undef, $series->{"${cat}_id"});
        if (!$is_used) {
            $dbh->do('DELETE FROM series_categories WHERE id = ?',
                      undef, $series->{"${cat}_id"});
        }
    }
    $dbh->bz_commit_transaction();

    $vars->{'message'} = "series_deleted";
    $vars->{'series'} = $series;
    view();
}
elsif ($action eq "convert_search") {
    my $saved_search = $cgi->param('series_from_search') || '';
    my ($query) = grep { $_->name eq $saved_search } @{ $user->queries };
    my $url = '';
    if ($query) {
        my $params = new Bugzilla::CGI($query->edit_link);
        # These two parameters conflict with the one below.
        $url = $params->canonicalise_query('format', 'query_format');
        $url = '&amp;' . html_quote($url);
    }
    print $cgi->redirect(-location => correct_urlbase() . "query.cgi?format=create-series$url");
}
else {
    ThrowUserError('unknown_action', {action => $action});
}

exit;

# Find any selected series and return either the first or all of them.
sub getAndValidateSeriesIDs {
    my @series_ids = grep(/^\d+$/, $cgi->param("name"));

    return wantarray ? @series_ids : $series_ids[0];
}

# Return a list of IDs of all the lines selected in the UI.
sub getSelectedLines {
    my @ids = map { /^select(\d+)$/ ? $1 : () } $cgi->param();

    return @ids;
}

# Check if the user is the owner of series_id or is an admin. 
sub assertCanEdit {
    my $series_id = shift;
    my $user = Bugzilla->user;

    my $series = new Bugzilla::Series($series_id)
      || ThrowCodeError('invalid_series_id');

    if (!$user->in_group('admin') && $series->{creator_id} != $user->id) {
        ThrowUserError('illegal_series_edit');
    }

    return $series;
}

# Check if the user is permitted to create this series with these parameters.
sub assertCanCreate {
    my ($cgi) = shift;
    my $user = Bugzilla->user;

    $user->in_group("editbugs") || ThrowUserError("illegal_series_creation");

    # Check permission for frequency
    my $min_freq = 7;
    if ($cgi->param('frequency') < $min_freq && !$user->in_group("admin")) {
        ThrowUserError("illegal_frequency", { 'minimum' => $min_freq });
    }
}

sub validateWidthAndHeight {
    $vars->{'width'} = $cgi->param('width');
    $vars->{'height'} = $cgi->param('height');

    if (defined($vars->{'width'})) {
       (detaint_natural($vars->{'width'}) && $vars->{'width'} > 0)
         || ThrowUserError("invalid_dimensions");
    }

    if (defined($vars->{'height'})) {
       (detaint_natural($vars->{'height'}) && $vars->{'height'} > 0)
         || ThrowUserError("invalid_dimensions");
    }

    # The equivalent of 2000 square seems like a very reasonable maximum size.
    # This is merely meant to prevent accidental or deliberate DOS, and should
    # have no effect in practice.
    if ($vars->{'width'} && $vars->{'height'}) {
       (($vars->{'width'} * $vars->{'height'}) <= 4000000)
         || ThrowUserError("chart_too_large");
    }
}

sub edit {
    my $series = shift;

    $vars->{'category'} = Bugzilla::Chart::getVisibleSeries();
    $vars->{'default'} = $series;
    $vars->{'message'} = 'series_updated' if $vars->{'changes_saved'};

    print $cgi->header();
    $template->process("reports/edit-series.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub plot {
    validateWidthAndHeight();
    $vars->{'chart'} = new Bugzilla::Chart($cgi);

    my $format = $template->get_format("reports/chart", "", scalar($cgi->param('ctype')));
    $format->{'ctype'} = 'text/html' if $cgi->param('debug');

    $cgi->set_dated_content_disp('inline', 'chart', $format->{extension});
    print $cgi->header($format->{'ctype'});
    disable_utf8() if ($format->{'ctype'} =~ /^image\//);

    # Debugging PNGs is a pain; we need to be able to see the error messages
    $vars->{'chart'}->dump() if $cgi->param('debug');

    $template->process($format->{'template'}, $vars)
      || ThrowTemplateError($template->error());
}

sub wrap {
    validateWidthAndHeight();
    
    # We create a Chart object so we can validate the parameters
    my $chart = new Bugzilla::Chart($cgi);
    
    $vars->{'time'} = localtime(time());

    $vars->{'imagebase'} = $cgi->canonicalise_query(
                "action", "action-wrap", "ctype", "format", "width", "height");

    print $cgi->header();
    $template->process("reports/chart.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub view {
    my $chart = shift;

    # Set defaults
    foreach my $field ('category', 'subcategory', 'name', 'ctype') {
        $vars->{'default'}{$field} = $cgi->param($field) || 0;
    }

    # Pass the state object to the display UI.
    $vars->{'chart'} = $chart;
    $vars->{'category'} = Bugzilla::Chart::getVisibleSeries();

    print $cgi->header();

    # If we have having problems with bad data, we can set debug=1 to dump
    # the data structure.
    $chart->dump() if $cgi->param('debug');

    $template->process("reports/create-chart.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}
