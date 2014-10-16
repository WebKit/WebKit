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
#                 Myk Melez <myk@mozilla.org>
#                 Frank Becker <Frank@Frank-Becker.de>

################################################################################
# Script Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

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

# If the 'requirelogin' parameter is on and the user is not
# authenticated, return empty fields.
if (Bugzilla->params->{'requirelogin'} && !$user->id) {
    display_data();
}

# Get data from the shadow DB as they don't change very often.
Bugzilla->switch_to_shadow_db;

# Pass a bunch of Bugzilla configuration to the templates.
my $vars = {};
$vars->{'priority'}  = get_legal_field_values('priority');
$vars->{'severity'}  = get_legal_field_values('bug_severity');
$vars->{'platform'}  = get_legal_field_values('rep_platform');
$vars->{'op_sys'}    = get_legal_field_values('op_sys');
$vars->{'keyword'}    = [map($_->name, Bugzilla::Keyword->get_all)];
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
if (!Bugzilla->user->is_timetracker) {
    @fields = grep { $_->name !~ /^(estimated_time|remaining_time|work_time|percentage_complete|deadline)$/ } @fields;
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

    # ETag support.
    my $if_none_match = $cgi->http('If-None-Match') || "";
    my $found304;
    my @if_none = split(/[\s,]+/, $if_none_match);
    foreach my $if_none (@if_none) {
        # remove quotes from begin and end of the string
        $if_none =~ s/^\"//g;
        $if_none =~ s/\"$//g;
        if ($if_none eq $digest or $if_none eq '*') {
            # leave the loop after the first match
            $found304 = $if_none;
            last;
        }
    }
 
   if ($found304) {
        print $cgi->header(-type => 'text/html',
                           -ETag => $found304,
                           -status => '304 Not Modified');
    }
    else {
        # Return HTTP headers.
        print $cgi->header (-ETag => $digest,
                            -type => $format->{'ctype'});
        print $output;
    }
    exit;
}
