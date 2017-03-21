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
use Bugzilla::Classification;
use Bugzilla::Product;

my $user = Bugzilla->login();
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};

print $cgi->header();

# This script does nothing but displaying mostly static data.
Bugzilla->switch_to_shadow_db;

my $product_name = trim($cgi->param('product') || '');
my $product = new Bugzilla::Product({'name' => $product_name});

unless ($product && $user->can_access_product($product->name)) {
    # Products which the user is allowed to see.
    my @products = @{$user->get_accessible_products};

    if (scalar(@products) == 0) {
        ThrowUserError("no_products");
    }
    # If there is only one product available but the user entered
    # another product name, we display a list with this single
    # product only, to not confuse the user with components of a
    # product they didn't request.
    elsif (scalar(@products) > 1 || $product_name) {
        $vars->{'classifications'} = sort_products_by_classification(\@products);
        $vars->{'target'} = "describecomponents.cgi";
        # If an invalid product name is given, or the user is not
        # allowed to access that product, a message is displayed
        # with a list of the products the user can choose from.
        if ($product_name) {
            $vars->{'message'} = "product_invalid";
            # Do not use $product->name here, else you could use
            # this way to determine whether the product exists or not.
            $vars->{'product'} = $product_name;
        }

        $template->process("global/choose-product.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        exit;
    }

    # If there is only one product available and the user didn't specify
    # any product name, we show this product.
    $product = $products[0];
}

######################################################################
# End Data/Security Validation
######################################################################

$vars->{'product'} = $product;

$template->process("reports/components.html.tmpl", $vars)
  || ThrowTemplateError($template->error());
