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
use Bugzilla::Error;
use Bugzilla::Keyword;
use Bugzilla::Product;
use Bugzilla::Status;
use Bugzilla::Field;

use Digest::MD5 qw(md5_base64);

my $user = Bugzilla->login(LOGIN_OPTIONAL);
my $cgi  = Bugzilla->cgi;

# Get data from the shadow DB as they don't change very often.
Bugzilla->switch_to_shadow_db;

# If the 'requirelogin' parameter is on and the user is not
# authenticated, return empty fields.
if (Bugzilla->params->{'requirelogin'} && !$user->id) {
    display_data();
    exit;
}

# Pass a bunch of Bugzilla configuration to the templates.
my $vars = {};
$vars->{'priority'}  = get_legal_field_values('priority');
$vars->{'severity'}  = get_legal_field_values('bug_severity');
$vars->{'platform'}  = get_legal_field_values('rep_platform');
$vars->{'op_sys'}    = get_legal_field_values('op_sys');
$vars->{'keywords'}  = [Bugzilla::Keyword->get_all];
$vars->{'resolution'} = get_legal_field_values('resolution');
$vars->{'status'}    = get_legal_field_values('bug_status');
$vars->{'custom_fields'} =
    [ grep {$_->is_select} Bugzilla->active_custom_fields ];

# Include a list of product objects.
if ($cgi->param('product')) {
    my @products = $cgi->param('product');
    foreach my $product_name (@products) {
        # We don't use check() because config.cgi outputs mostly
        # in XML and JS and we don't want to display an HTML error
        # instead of that.
        my $product = new Bugzilla::Product({ name => $product_name });
        if ($product && $user->can_see_product($product->name)) {
            push (@{$vars->{'products'}}, $product);
        }
    }
} else {
    $vars->{'products'} = $user->get_selectable_products;
}

# We set the 2nd argument to 1 to also preload flag types.
Bugzilla::Product::preload($vars->{'products'}, 1);

if (Bugzilla->params->{'useclassification'}) {
    my $class = {};
    # Get all classifications with at least one selectable product.
    foreach my $product (@{$vars->{'products'}}) {
        $class->{$product->classification_id} ||= $product->classification;
    }
    my @classifications = sort {$a->sortkey <=> $b->sortkey
        || lc($a->name) cmp lc($b->name)} (values %$class);
    $vars->{'class_names'} = $class;
    $vars->{'classifications'} = \@classifications;
}

# Allow consumers to specify whether or not they want flag data.
if (defined $cgi->param('flags')) {
    $vars->{'show_flags'} = $cgi->param('flags');
}
else {
    # We default to sending flag data.
    $vars->{'show_flags'} = 1;
}

# Create separate lists of open versus resolved statuses.  This should really
# be made part of the configuration.
my @open_status;
my @closed_status;
foreach my $status (@{$vars->{'status'}}) {
    is_open_state($status) ? push(@open_status, $status) 
                           : push(@closed_status, $status);
}
$vars->{'open_status'} = \@open_status;
$vars->{'closed_status'} = \@closed_status;

# Generate a list of fields that can be queried.
my @fields = @{Bugzilla::Field->match({obsolete => 0})};
# Exclude fields the user cannot query.
if (!$user->is_timetracker) {
    foreach my $tt_field (TIMETRACKING_FIELDS) {
        @fields = grep { $_->name ne $tt_field } @fields;
    }
}

my %FIELD_PARAMS = (
    classification    => 'useclassification',
    target_milestone  => 'usetargetmilestone',
    qa_contact        => 'useqacontact',
    status_whiteboard => 'usestatuswhiteboard',
    see_also          => 'use_see_also',
);
foreach my $field (@fields) {
    my $param = $FIELD_PARAMS{$field->name};
    $field->{is_active} =  Bugzilla->params->{$param} if $param;
}
$vars->{'field'} = \@fields;

display_data($vars);


sub display_data {
    my $vars = shift;

    my $cgi      = Bugzilla->cgi;
    my $template = Bugzilla->template;

    # Determine how the user would like to receive the output; 
    # default is JavaScript.
    my $format = $template->get_format("config", scalar($cgi->param('format')),
                                       scalar($cgi->param('ctype')) || "js");

    # Generate the configuration data.
    my $output;
    $template->process($format->{'template'}, $vars, \$output)
      || ThrowTemplateError($template->error());

    # Wide characters cause md5_base64() to die.
    my $digest_data = $output;
    utf8::encode($digest_data) if utf8::is_utf8($digest_data);
    my $digest = md5_base64($digest_data);

    if ($cgi->check_etag($digest)) {
        print $cgi->header(-ETag   => $digest,
                           -status => '304 Not Modified');
        exit;
    }

    print $cgi->header (-ETag => $digest,
                        -type => $format->{'ctype'});
    print $output;
}
