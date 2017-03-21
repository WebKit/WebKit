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
use Bugzilla::Status;

use File::Basename;
use Digest::SHA qw(hmac_sha256_base64);

# If we're using bug groups for products, we should apply those restrictions
# to viewing reports, as well.  Time to check the login in that case.
my $user = Bugzilla->login();
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};

# We use a dummy product instance with ID 0, representing all products
my $product_all = {id => 0};
bless($product_all, 'Bugzilla::Product');

if (!Bugzilla->feature('old_charts')) {
    ThrowUserError('feature_disabled', { feature => 'old_charts' });
}

my $dir       = bz_locations()->{'datadir'} . "/mining";
my $graph_dir = bz_locations()->{'graphsdir'};
my $graph_url = basename($graph_dir);
my $product_id = $cgi->param('product_id');

Bugzilla->switch_to_shadow_db();

if (! defined($product_id)) {
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

    $vars->{'datasets'} = \@datasets;

    print $cgi->header();
}
else {
    my $product;
    # For security and correctness, validate the value of the "product_id" form
    # variable. Valid values are IDs of those products for which the user has
    # permissions which appear in the "product_id" drop-down menu on the report
    # generation form. The product_id 0 is a special case, meaning "All
    # Products".
    if ($product_id) {
        $product = Bugzilla::Product->new($product_id);
        $product && $user->can_see_product($product->name)
            || ThrowUserError('product_access_denied',
                              {id => $product_id});
    }
    else {
        $product = $product_all;
    }

    # Make sure there is something to plot.
    my @datasets = $cgi->param('datasets');
    scalar(@datasets) || ThrowUserError('missing_datasets');

    if (grep { $_ !~ /^[A-Za-z0-9:_-]+$/ } @datasets) {
        ThrowUserError('invalid_datasets', {'datasets' => \@datasets});
    }

    # Filenames must not be guessable as they can point to products
    # you are not allowed to see. Also, different projects can have
    # the same product IDs.
    my $project = bz_locations()->{'project'} || '';
    my $image_file =  join(':', ($project, $product->id, @datasets));
    my $key = Bugzilla->localconfig->{'site_wide_secret'};
    $image_file = hmac_sha256_base64($image_file, $key) . '.png';
    $image_file =~ s/\+/-/g;
    $image_file =~ s/\//_/g;
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
    open(DATA, '<', "$dir/0")
      || ThrowCodeError('chart_file_open_fail', {filename => "$dir/0"});

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
    my $data_file = $dir . '/' . $product->id;

    if (!open(FILE, '<', $data_file)) {
        ThrowCodeError('chart_data_not_generated', {'product' => $product});
    }

    my $product_in_title = $product->id ? $product->name : 'All Products';
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
         "title" => "Status Counts for $product_in_title",
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
