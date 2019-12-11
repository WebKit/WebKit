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
use Bugzilla::Field;
use Bugzilla::Search;
use Bugzilla::Report;
use Bugzilla::Token;

use List::MoreUtils qw(uniq);

my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};

# Go straight back to query.cgi if we are adding a boolean chart.
if (grep(/^cmd-/, $cgi->param())) {
    my $params = $cgi->canonicalise_query("format", "ctype");
    my $location = "query.cgi?format=" . $cgi->param('query_format') . 
      ($params ? "&$params" : "");

    print $cgi->redirect($location);
    exit;
}

Bugzilla->login();
my $action = $cgi->param('action') || 'menu';
my $token  = $cgi->param('token');

if ($action eq "menu") {
    # No need to do any searching in this case, so bail out early.
    print $cgi->header();
    $template->process("reports/menu.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;

}
elsif ($action eq 'add') {
    my $user = Bugzilla->login(LOGIN_REQUIRED);
    check_hash_token($token, ['save_report']);

    my $name = clean_text($cgi->param('name'));
    my $query = $cgi->param('query');

    if (my ($report) = grep{ lc($_->name) eq lc($name) } @{$user->reports}) {
        $report->set_query($query);
        $report->update;
        $vars->{'message'} = "report_updated";
    } else {
        my $report = Bugzilla::Report->create({name => $name, query => $query});
        $vars->{'message'} = "report_created";
    }

    $user->flush_reports_cache;

    print $cgi->header();

    $vars->{'reportname'} = $name;

    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}
elsif ($action eq 'del') {
    my $user = Bugzilla->login(LOGIN_REQUIRED);
    my $report_id = $cgi->param('saved_report_id');
    check_hash_token($token, ['delete_report', $report_id]);

    my $report = Bugzilla::Report->check({id => $report_id});
    $report->remove_from_db();

    $user->flush_reports_cache;

    print $cgi->header();

    $vars->{'message'} = 'report_deleted';
    $vars->{'reportname'} = $report->name;

    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

# Sanitize the URL, to make URLs shorter.
$cgi->clean_search_url;

my $col_field = $cgi->param('x_axis_field') || '';
my $row_field = $cgi->param('y_axis_field') || '';
my $tbl_field = $cgi->param('z_axis_field') || '';

if (!($col_field || $row_field || $tbl_field)) {
    ThrowUserError("no_axes_defined");
}

# There is no UI for these parameters anymore,
# but they are still here just in case.
my $width = $cgi->param('width') || 1024;
my $height = $cgi->param('height') || 600;

(detaint_natural($width) && $width > 0)
  || ThrowUserError("invalid_dimensions");
$width <= 2000 || ThrowUserError("chart_too_large");

(detaint_natural($height) && $height > 0)
  || ThrowUserError("invalid_dimensions");
$height <= 2000 || ThrowUserError("chart_too_large");

my $formatparam = $cgi->param('format') || '';

# These shenanigans are necessary to make sure that both vertical and 
# horizontal 1D tables convert to the correct dimension when you ask to
# display them as some sort of chart.
if ($formatparam eq "table") {
    if ($col_field && !$row_field) {    
        # 1D *tables* should be displayed vertically (with a row_field only)
        $row_field = $col_field;
        $col_field = '';
    }
}
else {
    if (!Bugzilla->feature('graphical_reports')) {
        ThrowUserError('feature_disabled', { feature => 'graphical_reports' });
    }

    if ($row_field && !$col_field) {
        # 1D *charts* should be displayed horizontally (with an col_field only)
        $col_field = $row_field;
        $row_field = '';
    }
}

# Valid bug fields that can be reported on.
my $valid_columns = Bugzilla::Search::REPORT_COLUMNS;

# Validate the values in the axis fields or throw an error.
!$row_field 
  || ($valid_columns->{$row_field} && trick_taint($row_field))
  || ThrowUserError("report_axis_invalid", {fld => "x", val => $row_field});
!$col_field 
  || ($valid_columns->{$col_field} && trick_taint($col_field))
  || ThrowUserError("report_axis_invalid", {fld => "y", val => $col_field});
!$tbl_field 
  || ($valid_columns->{$tbl_field} && trick_taint($tbl_field))
  || ThrowUserError("report_axis_invalid", {fld => "z", val => $tbl_field});

my @axis_fields = grep { $_ } ($row_field, $col_field, $tbl_field);

# Clone the params, so that Bugzilla::Search can modify them
my $params = new Bugzilla::CGI($cgi);
my $search = new Bugzilla::Search(
    fields => \@axis_fields, 
    params => scalar $params->Vars,
    allow_unlimited => 1,
);

$::SIG{TERM} = 'DEFAULT';
$::SIG{PIPE} = 'DEFAULT';

Bugzilla->switch_to_shadow_db();
my ($results, $extra_data) = $search->data;

# We have a hash of hashes for the data itself, and a hash to hold the 
# row/col/table names.
my %data;
my %names;

# Read the bug data and count the bugs for each possible value of row, column
# and table.
#
# We detect a numerical field, and sort appropriately, if all the values are
# numeric.
my $col_isnumeric = 1;
my $row_isnumeric = 1;
my $tbl_isnumeric = 1;

# define which fields are multiselect
my @multi_selects = map { $_->name } @{Bugzilla->fields(
    {
        obsolete => 0,
        type => [FIELD_TYPE_MULTI_SELECT, FIELD_TYPE_KEYWORDS]
    }
)};
my $col_ismultiselect = scalar grep {$col_field eq $_} @multi_selects;
my $row_ismultiselect = scalar grep {$row_field eq $_} @multi_selects;
my $tbl_ismultiselect = scalar grep {$tbl_field eq $_} @multi_selects;


foreach my $result (@$results) {
    # handle empty dimension member names
    
    my @rows = check_value($row_field, $result, $row_ismultiselect);
    my @cols = check_value($col_field, $result, $col_ismultiselect);
    my @tbls = check_value($tbl_field, $result, $tbl_ismultiselect);

    my %in_total_row;
    my %in_total_col;
    for my $tbl (@tbls) {
        my %in_row_total;
        for my $col (@cols) {
            for my $row (@rows) {
                $data{$tbl}{$col}{$row}++;
                $names{"row"}{$row}++;
                $row_isnumeric &&= ($row =~ /^-?\d+(\.\d+)?$/o);
                if ($formatparam eq "table") {
                    if (!$in_row_total{$row}) {
                        $data{$tbl}{'-total-'}{$row}++;
                        $in_row_total{$row} = 1;
                    }
                    if (!$in_total_row{$row}) {
                        $data{'-total-'}{'-total-'}{$row}++;
                        $in_total_row{$row} = 1;
                    }
                }
            }
            if ($formatparam eq "table") {
                $data{$tbl}{$col}{'-total-'}++;
                if (!$in_total_col{$col}) {
                    $data{'-total-'}{$col}{'-total-'}++;
                    $in_total_col{$col} = 1;
                }
            }
            $names{"col"}{$col}++;
            $col_isnumeric &&= ($col =~ /^-?\d+(\.\d+)?$/o);
        }
        $names{"tbl"}{$tbl}++;
        $tbl_isnumeric &&= ($tbl =~ /^-?\d+(\.\d+)?$/o);
        if ($formatparam eq "table") {
            $data{$tbl}{'-total-'}{'-total-'}++;
        }
    }
    if ($formatparam eq "table") {
        $data{'-total-'}{'-total-'}{'-total-'}++;
    }
}

my @col_names = get_names($names{"col"}, $col_isnumeric, $col_field);
my @row_names = get_names($names{"row"}, $row_isnumeric, $row_field);
my @tbl_names = get_names($names{"tbl"}, $tbl_isnumeric, $tbl_field);

# The GD::Graph package requires a particular format of data, so once we've
# gathered everything into the hashes and made sure we know the size of the
# data, we reformat it into an array of arrays of arrays of data.
push(@tbl_names, "-total-") if (scalar(@tbl_names) > 1);
    
my @image_data;
foreach my $tbl (@tbl_names) {
    my @tbl_data;
    push(@tbl_data, \@col_names);
    foreach my $row (@row_names) {
        my @col_data;
        foreach my $col (@col_names) {
            $data{$tbl}{$col}{$row} = $data{$tbl}{$col}{$row} || 0;
            push(@col_data, $data{$tbl}{$col}{$row});
            if ($tbl ne "-total-") {
                # This is a bit sneaky. We spend every loop except the last
                # building up the -total- data, and then last time round,
                # we process it as another tbl, and push() the total values 
                # into the image_data array.
                $data{"-total-"}{$col}{$row} += $data{$tbl}{$col}{$row};
            }
        }

        push(@tbl_data, \@col_data);
    }
    
    unshift(@image_data, \@tbl_data);
}

$vars->{'col_field'} = $col_field;
$vars->{'row_field'} = $row_field;
$vars->{'tbl_field'} = $tbl_field;
$vars->{'time'} = localtime(time());

$vars->{'col_names'} = \@col_names;
$vars->{'row_names'} = \@row_names;
$vars->{'tbl_names'} = \@tbl_names;
$vars->{'note_multi_select'} = $row_ismultiselect || $col_ismultiselect;

# Below a certain width, we don't see any bars, so there needs to be a minimum.
if ($formatparam eq "bar") {
    my $min_width = (scalar(@col_names) || 1) * 20;

    if (!$cgi->param('cumulate')) {
        $min_width *= (scalar(@row_names) || 1);
    }

    $vars->{'min_width'} = $min_width;
}

$vars->{'width'} = $width;
$vars->{'height'} = $height;
$vars->{'queries'} = $extra_data;
$vars->{'saved_report_id'} = $cgi->param('saved_report_id');

if ($cgi->param('debug')
    && Bugzilla->params->{debug_group}
    && Bugzilla->user->in_group(Bugzilla->params->{debug_group})
) {
    $vars->{'debug'} = 1;
}

if ($action eq "wrap") {
    # So which template are we using? If action is "wrap", we will be using
    # no format (it gets passed through to be the format of the actual data),
    # and either report.csv.tmpl (CSV), or report.html.tmpl (everything else).
    # report.html.tmpl produces an HTML framework for either tables of HTML
    # data, or images generated by calling report.cgi again with action as
    # "plot".
    $formatparam =~ s/[^a-zA-Z\-]//g;
    $vars->{'format'} = $formatparam;
    $formatparam = '';

    # We need to keep track of the defined restrictions on each of the 
    # axes, because buglistbase, below, throws them away. Without this, we
    # get buglistlinks wrong if there is a restriction on an axis field.
    $vars->{'col_vals'} = get_field_restrictions($col_field);
    $vars->{'row_vals'} = get_field_restrictions($row_field);
    $vars->{'tbl_vals'} = get_field_restrictions($tbl_field);

    # We need a number of different variants of the base URL for different
    # URLs in the HTML.
    $vars->{'buglistbase'} = $cgi->canonicalise_query(
                                 "x_axis_field", "y_axis_field", "z_axis_field",
                               "ctype", "format", "query_format", @axis_fields);
    $vars->{'imagebase'}   = $cgi->canonicalise_query( 
                    $tbl_field, "action", "ctype", "format", "width", "height");
    $vars->{'switchbase'}  = $cgi->canonicalise_query( 
                "query_format", "action", "ctype", "format", "width", "height");
    $vars->{'data'} = \%data;
}
elsif ($action eq "plot") {
    # If action is "plot", we will be using a format as normal (pie, bar etc.)
    # and a ctype as normal (currently only png.)
    $vars->{'cumulate'} = $cgi->param('cumulate') ? 1 : 0;
    $vars->{'x_labels_vertical'} = $cgi->param('x_labels_vertical') ? 1 : 0;
    $vars->{'data'} = \@image_data;
}
else {
    ThrowUserError('unknown_action', {action => $action});
}

my $format = $template->get_format("reports/report", $formatparam,
                                   scalar($cgi->param('ctype')));

# If we get a template or CGI error, it comes out as HTML, which isn't valid
# PNG data, and the browser just displays a "corrupt PNG" message. So, you can
# set debug=1 to always get an HTML content-type, and view the error.
$format->{'ctype'} = "text/html" if $cgi->param('debug');

$cgi->set_dated_content_disp("inline", "report", $format->{extension});
print $cgi->header($format->{'ctype'});

# Problems with this CGI are often due to malformed data. Setting debug=1
# prints out both data structures.
if ($cgi->param('debug')) {
    require Data::Dumper;
    say "<pre>data hash:";
    say html_quote(Data::Dumper::Dumper(%data));
    say "\ndata array:";
    say html_quote(Data::Dumper::Dumper(@image_data)) . "\n\n</pre>";
}

# All formats point to the same section of the documentation.
$vars->{'doc_section'} = 'using/reports-and-charts.html#reports';

disable_utf8() if ($format->{'ctype'} =~ /^image\//);

$template->process("$format->{'template'}", $vars)
  || ThrowTemplateError($template->error());


sub get_names {
    my ($names, $isnumeric, $field_name) = @_;
    my ($field, @sorted);
    # XXX - This is a hack to handle the actual_time/work_time field,
    # because it's named 'actual_time' in Search.pm but 'work_time' in Field.pm.
    $_[2] = $field_name = 'work_time' if $field_name eq 'actual_time';

    # _realname fields aren't real Bugzilla::Field objects, but they are a
    # valid axis, so we don't vailidate them as Bugzilla::Field objects.
    $field = Bugzilla::Field->check($field_name) 
        if ($field_name && $field_name !~ /_realname$/);
    
    if ($field && $field->is_select) {
        foreach my $value (@{$field->legal_values}) {
            push(@sorted, $value->name) if $names->{$value->name};
        }
        unshift(@sorted, '---') if ($field_name eq 'resolution'
                                    || $field->type == FIELD_TYPE_MULTI_SELECT);
        @sorted = uniq @sorted;
    }  
    elsif ($isnumeric) {
        # It's not a field we are preserving the order of, so sort it 
        # numerically...
        @sorted = sort { $a <=> $b } keys %$names;
    }
    else {
        # ...or alphabetically, as appropriate.
        @sorted = sort keys %$names;
    }
    
    return @sorted;
}

sub check_value {
    my ($field, $result, $ismultiselect) = @_;

    my $value;
    if (!defined $field) {
        $value = '';
    }
    elsif ($field eq '') {
        $value = ' ';
    }
    else {
        $value = shift @$result;
        $value = ' ' if (!defined $value || $value eq '');
        $value = '---' if (($field eq 'resolution' || $ismultiselect ) &&
                           $value eq ' ');
    }
    if ($ismultiselect) {
        # Some DB servers have a space after the comma, some others don't.
        return split(/, ?/, $value);
    } else {
        return ($value);
    }
}

sub get_field_restrictions {
    my $field = shift;
    my $cgi = Bugzilla->cgi;

    return join('&amp;', map {url_quote($field) . '=' . url_quote($_)} $cgi->param($field));
}
