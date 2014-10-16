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
# Contributor(s): Harrison Page <harrison@netscape.com>
#                 Terry Weissman <terry@mozilla.org>
#                 Dawn Endico <endico@mozilla.org>
#                 Bryce Nesbitt <bryce@nextbus.com>
#                 Joe Robins <jmrobins@tgix.com>
#                 Gervase Markham <gerv@gerv.net>
#                 Adam Spiers <adam@spiers.net>
#                 Myk Melez <myk@mozilla.org>
#                 Frédéric Buclin <LpSolit@gmail.com>

use strict;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Status;

use File::Basename;
use Digest::MD5 qw(md5_hex);

# If we're using bug groups for products, we should apply those restrictions
# to viewing reports, as well.  Time to check the login in that case.
my $user = Bugzilla->login();
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};

if (!Bugzilla->feature('old_charts')) {
    ThrowCodeError('feature_disabled', { feature => 'old_charts' });
}

my $dir       = bz_locations()->{'datadir'} . "/mining";
my $graph_dir = bz_locations()->{'graphsdir'};
my $graph_url = basename($graph_dir);
my $product_name = $cgi->param('product') || '';

Bugzilla->switch_to_shadow_db();

if (!$product_name) {
    # Can we do bug charts?
    (-d $dir && -d $graph_dir) 
      || ThrowCodeError('chart_dir_nonexistent',
                        {dir => $dir, graph_dir => $graph_dir});

    my %default_sel = map { $_ => 1 } BUG_STATE_OPEN;

    my @datasets;
    my @data = get_data($dir);

    foreach my $dataset (@data) {
        my $datasets = {};
        $datasets->{'value'} = $dataset;
        $datasets->{'selected'} = $default_sel{$dataset} ? 1 : 0;
        push(@datasets, $datasets);
    }

    # We only want those products that the user has permissions for.
    my @myproducts = ('-All-');
    # Extract product names from objects and add them to the list.
    push( @myproducts, map { $_->name } @{$user->get_selectable_products} );

    $vars->{'datasets'} = \@datasets;
    $vars->{'products'} = \@myproducts;

    print $cgi->header();
}
else {
    # For security and correctness, validate the value of the "product" form variable.
    # Valid values are those products for which the user has permissions which appear
    # in the "product" drop-down menu on the report generation form.
    my ($product) = grep { $_->name eq $product_name } @{$user->get_selectable_products};
    ($product || $product_name eq '-All-')
      || ThrowUserError('invalid_product_name', {product => $product_name});

    # Product names can change over time. Their ID cannot; so use the ID
    # to generate the filename.
    my $prod_id = $product ? $product->id : 0;

    # Make sure there is something to plot.
    my @datasets = $cgi->param('datasets');
    scalar(@datasets) || ThrowUserError('missing_datasets');

    if (grep { $_ !~ /^[A-Za-z0-9:_-]+$/ } @datasets) {
        ThrowUserError('invalid_datasets', {'datasets' => \@datasets});
    }

    # Filenames must not be guessable as they can point to products
    # you are not allowed to see. Also, different projects can have
    # the same product names.
    my $key = Bugzilla->localconfig->{'site_wide_secret'};
    my $project = bz_locations()->{'project'} || '';
    my $image_file =  join(':', ($key, $project, $prod_id, @datasets));
    # Wide characters cause md5_hex() to die.
    if (Bugzilla->params->{'utf8'}) {
        utf8::encode($image_file) if utf8::is_utf8($image_file);
    }
    $image_file = md5_hex($image_file) . '.png';
    trick_taint($image_file);

    if (! -e "$graph_dir/$image_file") {
        generate_chart($dir, "$graph_dir/$image_file", $product, \@datasets);
    }

    $vars->{'url_image'} = "$graph_url/$image_file";

    print $cgi->header(-Content_Disposition=>'inline; filename=bugzilla_report.html');
}

$template->process('reports/old-charts.html.tmpl', $vars)
  || ThrowTemplateError($template->error());

#####################
#    Subroutines    #
#####################

sub get_data {
    my $dir = shift;

    my @datasets;
    open(DATA, '<', "$dir/-All-")
      || ThrowCodeError('chart_file_open_fail', {filename => "$dir/-All-"});

    while (<DATA>) {
        if (/^# fields?: (.+)\s*$/) {
            @datasets = grep ! /date/i, (split /\|/, $1);
            last;
        }
    }
    close(DATA);
    return @datasets;
}

sub generate_chart {
    my ($dir, $image_file, $product, $datasets) = @_;
    $product = $product ? $product->name : '-All-';
    my $data_file = $product;
    $data_file =~ s/\//-/gs;
    $data_file = $dir . '/' . $data_file;

    if (! open FILE, $data_file) {
        if ($product eq '-All-') {
            $product = '';
        }
        ThrowCodeError('chart_data_not_generated', {'product' => $product});
    }

    my @fields;
    my @labels = qw(DATE);
    my %datasets = map { $_ => 1 } @$datasets;

    my %data = ();
    while (<FILE>) {
        chomp;
        next unless $_;
        if (/^#/) {
            if (/^# fields?: (.*)\s*$/) {
                @fields = split /\||\r/, $1;
                $data{$_} ||= [] foreach @fields;
                unless ($fields[0] =~ /date/i) {
                    ThrowCodeError('chart_datafile_corrupt', {'file' => $data_file});
                }
                push @labels, grep($datasets{$_}, @fields);
            }
            next;
        }

        unless (@fields) {
            ThrowCodeError('chart_datafile_corrupt', {'file' => $data_file});
        }

        my @line = split /\|/;
        my $date = $line[0];
        my ($yy, $mm, $dd) = $date =~ /^\d{2}(\d{2})(\d{2})(\d{2})$/;
        push @{$data{DATE}}, "$mm/$dd/$yy";
        
        for my $i (1 .. $#fields) {
            my $field = $fields[$i];
            if (! defined $line[$i] or $line[$i] eq '') {
                # no data point given, don't plot (this will probably
                # generate loads of Chart::Base warnings, but that's not
                # our fault.)
                push @{$data{$field}}, undef;
            }
            else {
                push @{$data{$field}}, $line[$i];
            }
        }
    }
    
    shift @labels;

    close FILE;

    if (! @{$data{DATE}}) {
        ThrowUserError('insufficient_data_points');
    }

    my $img = Chart::Lines->new (800, 600);
    my $i = 0;

    my $MAXTICKS = 20;      # Try not to show any more x ticks than this.
    my $skip = 1;
    if (@{$data{DATE}} > $MAXTICKS) {
        $skip = int((@{$data{DATE}} + $MAXTICKS - 1) / $MAXTICKS);
    }

    my %settings =
        (
         "title" => "Status Counts for $product",
         "x_label" => "Dates",
         "y_label" => "Bug Counts",
         "legend_labels" => \@labels,
         "skip_x_ticks" => $skip,
         "y_grid_lines" => "true",
         "grey_background" => "false",
         "colors" => {
                      # default dataset colours are too alike
                      dataset4 => [0, 0, 0], # black
                     },
        );
    
    $img->set (%settings);
    $img->png($image_file, [ @data{('DATE', @labels)} ]);
}
