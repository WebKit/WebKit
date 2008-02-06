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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Holger
# Schurig. Portions created by Holger Schurig are
# Copyright (C) 1999 Holger Schurig. All
# Rights Reserved.
#
# Contributor(s): Holger Schurig <holgerschurig@nikocity.de>
#                 Terry Weissman <terry@mozilla.org>
#                 Gavin Shelley <bugzilla@chimpychompy.org>
#                 Frédéric Buclin <LpSolit@gmail.com>
#
#
# Direct any questions on this source code to
#
# Holger Schurig <holgerschurig@nikocity.de>

use strict;
use lib ".";

require "CGI.pl";
require "globals.pl";

use Bugzilla::Constants;
use Bugzilla::Config qw(:DEFAULT $datadir);
use Bugzilla::User;

use vars qw($template $vars);

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;

# TestProduct:  just returns if the specified product does exists
# CheckProduct: same check, optionally  emit an error text
# TestVersion:  just returns if the specified product/version combination exists
# CheckVersion: same check, optionally emit an error text

sub TestProduct ($)
{
    my $prod = shift;

    # does the product exist?
    SendSQL("SELECT name
             FROM products
             WHERE name = " . SqlQuote($prod));
    return FetchOneColumn();
}

sub CheckProduct ($)
{
    my $prod = shift;

    # do we have a product?
    unless ($prod) {
        ThrowUserError('product_not_specified');    
    }

    unless (TestProduct $prod) {
        ThrowUserError('product_doesnt_exist',
                       {'product' => $prod});
    }
}

sub TestVersion ($$)
{
    my ($prod,$ver) = @_;

    # does the product exist?
    SendSQL("SELECT products.name, value
             FROM versions, products
             WHERE versions.product_id = products.id
               AND products.name = " . SqlQuote($prod) . "
               AND value = " . SqlQuote($ver));
    return FetchOneColumn();
}

sub CheckVersion ($$)
{
    my ($prod, $ver) = @_;

    # do we have the version?
    unless ($ver) {
        ThrowUserError('version_not_specified');
    }

    CheckProduct($prod);

    unless (TestVersion $prod, $ver) {
        ThrowUserError('version_not_valid',
                       {'product' => $prod,
                        'version' => $ver});
    }
}


#
# Preliminary checks:
#

Bugzilla->login(LOGIN_REQUIRED);

print Bugzilla->cgi->header();

UserInGroup("editcomponents")
  || ThrowUserError("auth_failure", {group  => "editcomponents",
                                     action => "edit",
                                     object => "versions"});

#
# often used variables
#
my $product = trim($cgi->param('product') || '');
my $version = trim($cgi->param('version') || '');
my $action  = trim($cgi->param('action')  || '');


#
# product = '' -> Show nice list of versions
#

unless ($product) {

    my @products = ();

    SendSQL("SELECT products.name, products.description
             FROM products 
             ORDER BY products.name");

    while ( MoreSQLData() ) {
        my ($product, $description) = FetchSQLData();

        my $prod = {};

        $prod->{'name'} = $product;
        $prod->{'description'} = $description;

        push(@products, $prod);
    }

    $vars->{'products'} = \@products;
    $template->process("admin/versions/select-product.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# action='' -> Show nice list of versions
#

unless ($action) {

    CheckProduct($product);
    my $product_id = get_product_id($product);
    my @versions = ();

    SendSQL("SELECT value
             FROM versions
             WHERE product_id = $product_id
             ORDER BY value");

    while ( MoreSQLData() ) {
        my $name = FetchOneColumn();

        my $version = {};

        $version->{'name'} = $name;

        push(@versions, $version);

    }

    $vars->{'product'} = $product;
    $vars->{'versions'} = \@versions;
    $template->process("admin/versions/list.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}




#
# action='add' -> present form for parameters for new version
#
# (next action will be 'new')
#

if ($action eq 'add') {

    CheckProduct($product);
    my $product_id = get_product_id($product);

    $vars->{'product'} = $product;
    $template->process("admin/versions/create.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='new' -> add version entered in the 'action=add' screen
#

if ($action eq 'new') {

    CheckProduct($product);
    my $product_id = get_product_id($product);

    # Cleanups and valididy checks

    unless ($version) {
        ThrowUserError('version_blank_name',
                       {'name' => $version});
    }

    if (TestVersion($product,$version)) {
        ThrowUserError('version_already_exists',
                       {'name' => $version,
                        'product' => $product});
    }

    # Add the new version
    SendSQL("INSERT INTO versions ( " .
            "value, product_id" .
            " ) VALUES ( " .
            SqlQuote($version) . ", $product_id)");

    # Make versioncache flush
    unlink "$datadir/versioncache";

    $vars->{'name'} = $version;
    $vars->{'product'} = $product;
    $template->process("admin/versions/created.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}




#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {

    CheckVersion($product, $version);
    my $product_id = get_product_id($product);

    SendSQL("SELECT count(bug_id)
             FROM bugs
             WHERE product_id = $product_id
               AND version = " . SqlQuote($version));
    my $bugs = FetchOneColumn() || 0;

    $vars->{'bug_count'} = $bugs;
    $vars->{'name'} = $version;
    $vars->{'product'} = $product;
    $template->process("admin/versions/confirm-delete.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='delete' -> really delete the version
#

if ($action eq 'delete') {
    CheckVersion($product, $version);
    my $product_id = get_product_id($product);

    trick_taint($version);

    my $nb_bugs =
      $dbh->selectrow_array("SELECT COUNT(bug_id) FROM bugs
                             WHERE product_id = ? AND version = ?",
                             undef, ($product_id, $version));

    # The version cannot be removed if there are bugs
    # associated with it.
    if ($nb_bugs) {
        ThrowUserError("version_has_bugs", { nb => $nb_bugs });
    }

    $dbh->do("DELETE FROM versions WHERE product_id = ? AND value = ?",
              undef, ($product_id, $version));

    unlink "$datadir/versioncache";

    $vars->{'name'} = $version;
    $vars->{'product'} = $product;

    $template->process("admin/versions/deleted.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}



#
# action='edit' -> present the edit version form
#
# (next action would be 'update')
#

if ($action eq 'edit') {

    CheckVersion($product,$version);
    my $product_id = get_product_id($product);

    $vars->{'name'} = $version;
    $vars->{'product'} = $product;

    $template->process("admin/versions/edit.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='update' -> update the version
#

if ($action eq 'update') {

    my $versionold = trim($cgi->param('versionold') || '');

    CheckVersion($product,$versionold);
    my $product_id = get_product_id($product);

    # Note that the order of this tests is important. If you change
    # them, be sure to test for WHERE='$version' or WHERE='$versionold'

    $dbh->bz_lock_tables('bugs WRITE',
                         'versions WRITE',
                         'products READ');

    if ($version ne $versionold) {
        unless ($version) {
            ThrowUserError('version_blank_name');
        }
        if (TestVersion($product,$version)) {
            ThrowUserError('version_already_exists',
                           {'name' => $version,
                            'product' => $product});
        }
        SendSQL("UPDATE bugs
                 SET version=" . SqlQuote($version) . "
                 WHERE version=" . SqlQuote($versionold) . "
                   AND product_id = $product_id");
        SendSQL("UPDATE versions
                 SET value = " . SqlQuote($version) . "
                 WHERE product_id = $product_id
                   AND value = " . SqlQuote($versionold));
        unlink "$datadir/versioncache";

        $vars->{'updated_name'} = 1;
    }

    $dbh->bz_unlock_tables(); 

    $vars->{'name'} = $version;
    $vars->{'product'} = $product;
    $template->process("admin/versions/updated.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# No valid action found
#
ThrowUserError('no_valid_action', {'field' => "version"});
