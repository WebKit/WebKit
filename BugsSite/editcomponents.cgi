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
use Bugzilla::Series;
use Bugzilla::Util;
use Bugzilla::User;

use vars qw($template $vars);

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;

my $showbugcounts = (defined $cgi->param('showbugcounts'));

# TestProduct:    just returns if the specified product does exists
# CheckProduct:   same check, optionally  emit an error text
# TestComponent:  just returns if the specified product/component combination exists
# CheckComponent: same check, optionally emit an error text

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

sub TestComponent ($$)
{
    my ($prod, $comp) = @_;

    # does the product/component combination exist?
    SendSQL("SELECT components.name
             FROM components
             INNER JOIN products
                ON products.id = components.product_id
             WHERE products.name = " . SqlQuote($prod) . "
             AND components.name = " . SqlQuote($comp));
    return FetchOneColumn();
}

sub CheckComponent ($$)
{
    my ($prod, $comp) = @_;

    # do we have the component?
    unless ($comp) {
        ThrowUserError('component_not_specified');
    }

    CheckProduct($prod);

    unless (TestComponent $prod, $comp) {
        ThrowUserError('component_not_valid',
                       {'product' => $prod,
                        'name' => $comp});
    }
}


#
# Preliminary checks:
#

my $user = Bugzilla->login(LOGIN_REQUIRED);
my $whoid = $user->id;

print Bugzilla->cgi->header();

UserInGroup("editcomponents")
  || ThrowUserError("auth_failure", {group  => "editcomponents",
                                     action => "edit",
                                     object => "components"});

#
# often used variables
#
my $product   = trim($cgi->param('product')   || '');
my $component = trim($cgi->param('component') || '');
my $action    = trim($cgi->param('action')    || '');



#
# product = '' -> Show nice list of products
#

unless ($product) {

    my @products = ();

    if ($showbugcounts){
        SendSQL("SELECT products.name, products.description, COUNT(bug_id)
                 FROM products LEFT JOIN bugs
                   ON products.id = bugs.product_id " .
                $dbh->sql_group_by('products.name', 'products.description') . "
                 ORDER BY products.name");
    } else {
        SendSQL("SELECT products.name, products.description
                 FROM products 
                 ORDER BY products.name");
    }

    while ( MoreSQLData() ) {

        my $prod = {};

        my ($name, $description, $bug_count) = FetchSQLData();

        $prod->{'name'} = $name;
        $prod->{'description'} = $description;
        $prod->{'bug_count'} = $bug_count;

        push(@products, $prod);
    }

    $vars->{'showbugcounts'} = $showbugcounts;
    $vars->{'products'} = \@products;
    $template->process("admin/components/select-product.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());


    exit;
}



#
# action='' -> Show nice list of components
#

unless ($action) {

    CheckProduct($product);
    my $product_id = get_product_id($product);
    my @components = ();

    if ($showbugcounts) {
        SendSQL("SELECT name, description, initialowner,
                        initialqacontact, COUNT(bug_id)
                 FROM components LEFT JOIN bugs
                   ON components.id = bugs.component_id
                 WHERE components.product_id = $product_id " .
                $dbh->sql_group_by('name',
                    'description, initialowner, initialqacontact'));
    } else {
        SendSQL("SELECT name, description, initialowner, initialqacontact
                 FROM components 
                 WHERE product_id = $product_id " .
                $dbh->sql_group_by('name',
                    'description, initialowner, initialqacontact'));
    }        

    while (MoreSQLData()) {

        my $component = {};
        my ($name, $desc, $initialownerid, $initialqacontactid, $bug_count)
            = FetchSQLData();

        $component->{'name'} = $name;
        $component->{'description'} = $desc;
        $component->{'initialowner'} = DBID_to_name($initialownerid)
            if ($initialownerid);
        $component->{'initialqacontact'} = DBID_to_name($initialqacontactid)
            if ($initialqacontactid);
        $component->{'bug_count'} = $bug_count;

        push(@components, $component);

    }

    
    $vars->{'showbugcounts'} = $showbugcounts;
    $vars->{'product'} = $product;
    $vars->{'components'} = \@components;
    $template->process("admin/components/list.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}


#
# action='add' -> present form for parameters for new component
#
# (next action will be 'new')
#

if ($action eq 'add') {

    CheckProduct($product);

    $vars->{'product'} = $product;
    $template->process("admin/components/create.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());


    exit;
}



#
# action='new' -> add component entered in the 'action=add' screen
#

if ($action eq 'new') {

    CheckProduct($product);
    my $product_id = get_product_id($product);


    # Cleanups and valididy checks

    unless ($component) {
        ThrowUserError('component_blank_name',
                       {'name' => $component});
    }
    if (TestComponent($product, $component)) {
        ThrowUserError('component_already_exists',
                       {'name' => $component});
    }

    if (length($component) > 64) {
        ThrowUserError('component_name_too_long',
                       {'name' => $component});
    }

    my $description = trim($cgi->param('description') || '');

    if ($description eq '') {
        ThrowUserError('component_blank_description',
                       {'name' => $component});
    }

    my $initialowner = trim($cgi->param('initialowner') || '');

    if ($initialowner eq '') {
        ThrowUserError('component_need_initialowner',
                       {'name' => $component});
    }

    my $initialownerid = login_to_id ($initialowner);
    if (!$initialownerid) {
        ThrowUserError('component_need_valid_initialowner',
                       {'name' => $component});
    }

    my $initialqacontact = trim($cgi->param('initialqacontact') || '');
    my $initialqacontactid = login_to_id ($initialqacontact);
    if (Param('useqacontact')) {
        if (!$initialqacontactid && $initialqacontact ne '') {
            ThrowUserError('component_need_valid_initialqacontact',
                           {'name' => $component});
        }
    }
    my $initialqacontactsql =
              $initialqacontact ne '' ? SqlQuote($initialqacontactid) : 'NULL';

    # Add the new component
    SendSQL("INSERT INTO components ( " .
          "product_id, name, description, initialowner, initialqacontact " .
          " ) VALUES ( " .
          $product_id . "," .
          SqlQuote($component) . "," .
          SqlQuote($description) . "," .
          SqlQuote($initialownerid) . "," .
          $initialqacontactsql . ")");

    # Insert default charting queries for this product.
    # If they aren't using charting, this won't do any harm.
    GetVersionTable();

    my @series;

    my $prodcomp = "&product=" . url_quote($product) . 
                   "&component=" . url_quote($component);

    # For localisation reasons, we get the title of the queries from the
    # submitted form.
    my $open_name = $cgi->param('open_name');
    my $nonopen_name = $cgi->param('nonopen_name');
    my $open_query = "field0-0-0=resolution&type0-0-0=notregexp&value0-0-0=." .
                     $prodcomp;
    my $nonopen_query = "field0-0-0=resolution&type0-0-0=regexp&value0-0-0=." .
                        $prodcomp;

    # trick_taint is ok here, as these variables aren't used as a command
    # or in SQL unquoted
    trick_taint($open_name);
    trick_taint($nonopen_name);
    trick_taint($open_query);
    trick_taint($nonopen_query);

    push(@series, [$open_name, $open_query]);
    push(@series, [$nonopen_name, $nonopen_query]);

    foreach my $sdata (@series) {
        my $series = new Bugzilla::Series(undef, $product, $component,
                                          $sdata->[0], $::userid, 1,
                                          $sdata->[1], 1);
        $series->writeToDatabase();
    }

    # Make versioncache flush
    unlink "$datadir/versioncache";

    $vars->{'name'} = $component;
    $vars->{'product'} = $product;
    $template->process("admin/components/created.html.tmpl",
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

    CheckComponent($product, $component);
    my $component_id = get_component_id(get_product_id($product), $component);

    # display some data about the component
    SendSQL("SELECT products.name, products.description,
                    products.milestoneurl, products.disallownew,
                    components.name, components.initialowner,
                    components.initialqacontact, components.description
             FROM products
             LEFT JOIN components ON products.id = components.product_id
             WHERE components.id = $component_id");


    my ($product, $product_description, $milestoneurl, $disallownew,
        $component, $initialownerid, $initialqacontactid, $description) =
            FetchSQLData();


    my $initialowner = $initialownerid ? DBID_to_name ($initialownerid) : '';
    my $initialqacontact = $initialqacontactid ? DBID_to_name ($initialqacontactid) : '';
    $milestoneurl        ||= '';
    $product_description ||= '';
    $disallownew         ||= 0;
    $description         ||= '';
    
    if (Param('useqacontact')) {
        $vars->{'initialqacontact'} = $initialqacontact;
    }

    if (Param('usetargetmilestone')) {
        $vars->{'milestoneurl'} = $milestoneurl;
    }

    SendSQL("SELECT count(bug_id)
             FROM bugs
             WHERE component_id = $component_id");
    $vars->{'bug_count'} = FetchOneColumn() || 0;

    $vars->{'name'} = $component;
    $vars->{'description'} = $description;
    $vars->{'initialowner'} = $initialowner;
    $vars->{'product'} = $product;
    $vars->{'product_description'} = $product_description;
    $vars->{'disallownew'} = $disallownew;
    $template->process("admin/components/confirm-delete.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='delete' -> really delete the component
#

if ($action eq 'delete') {
    CheckComponent($product, $component);
    my $component_id = get_component_id(get_product_id($product), $component);

    my $bug_ids =
      $dbh->selectcol_arrayref("SELECT bug_id FROM bugs WHERE component_id = ?",
                               undef, $component_id);

    my $nb_bugs = scalar(@$bug_ids);
    if ($nb_bugs) {
        if (Param("allowbugdeletion")) {
            foreach my $bug_id (@$bug_ids) {
                my $bug = new Bugzilla::Bug($bug_id, $whoid);
                $bug->remove_from_db();
            }
        }
        else {
            ThrowUserError("component_has_bugs", { nb => $nb_bugs });
        }
    }

    $vars->{'deleted_bug_count'} = $nb_bugs;

    $dbh->bz_lock_tables('components WRITE', 'flaginclusions WRITE',
                         'flagexclusions WRITE');

    $dbh->do("DELETE FROM flaginclusions WHERE component_id = ?",
             undef, $component_id);
    $dbh->do("DELETE FROM flagexclusions WHERE component_id = ?",
             undef, $component_id);
    $dbh->do("DELETE FROM components WHERE id = ?", undef, $component_id);

    $dbh->bz_unlock_tables();

    unlink "$datadir/versioncache";

    $vars->{'name'} = $component;
    $vars->{'product'} = $product;
    $template->process("admin/components/deleted.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}



#
# action='edit' -> present the edit component form
#
# (next action would be 'update')
#

if ($action eq 'edit') {

    CheckComponent($product, $component);
    my $component_id = get_component_id(get_product_id($product), $component);

    # get data of component
    SendSQL("SELECT products.name,
                    components.name, components.initialowner,
                    components.initialqacontact, components.description
             FROM products LEFT JOIN components ON 
                  products.id = components.product_id
             WHERE components.id = $component_id");

    my ($product, $component, $initialownerid, $initialqacontactid,
        $description) = FetchSQLData();

    my $initialowner = $initialownerid ? DBID_to_name ($initialownerid) : '';
    my $initialqacontact = $initialqacontactid ? DBID_to_name ($initialqacontactid) : '';

    SendSQL("SELECT count(*)
             FROM bugs
             WHERE component_id = $component_id");

    $vars->{'bug_count'} = FetchOneColumn() || 0;

    $vars->{'name'} = $component;
    $vars->{'description'} = $description;
    $vars->{'initialowner'} = $initialowner;
    $vars->{'initialqacontact'} = $initialqacontact;
    $vars->{'product'} = $product;

    $template->process("admin/components/edit.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='update' -> update the component
#

if ($action eq 'update') {

    my $componentold        = trim($cgi->param('componentold')        || '');
    my $description         = trim($cgi->param('description')         || '');
    my $descriptionold      = trim($cgi->param('descriptionold')      || '');
    my $initialowner        = trim($cgi->param('initialowner')        || '');
    my $initialownerold     = trim($cgi->param('initialownerold')     || '');
    my $initialqacontact    = trim($cgi->param('initialqacontact')    || '');
    my $initialqacontactold = trim($cgi->param('initialqacontactold') || '');

    if (length($component) > 64) {
        ThrowUserError('component_name_too_long',
                       {'name' => $component});
    }

    # Note that the order of this tests is important. If you change
    # them, be sure to test for WHERE='$component' or WHERE='$componentold'

    $dbh->bz_lock_tables('components WRITE', 'products READ',
                         'profiles READ');
    CheckComponent($product, $componentold);
    my $component_id = get_component_id(get_product_id($product),
                                        $componentold);

    if ($description ne $descriptionold) {
        unless ($description) {
            ThrowUserError('component_blank_description',
                           {'name' => $componentold});
        }
        SendSQL("UPDATE components
                 SET description=" . SqlQuote($description) . "
                 WHERE id=$component_id");

        $vars->{'updated_description'} = 1;
        $vars->{'description'} = $description;
    }


    if ($initialowner ne $initialownerold) {

        my $initialownerid = login_to_id($initialowner);
        unless ($initialownerid) {
            ThrowUserError('component_need_valid_initialowner',
                           {'name' => $componentold});
        }

        SendSQL("UPDATE components
                 SET initialowner=" . SqlQuote($initialownerid) . "
                 WHERE id = $component_id");

        $vars->{'updated_initialowner'} = 1;
        $vars->{'initialowner'} = $initialowner;

    }

    if (Param('useqacontact') && $initialqacontact ne $initialqacontactold) {
        my $initialqacontactid = login_to_id($initialqacontact);
        if (!$initialqacontactid && $initialqacontact ne '') {
            ThrowUserError('component_need_valid_initialqacontact',
                           {'name' => $componentold});
        }
        my $initialqacontactsql =
              $initialqacontact ne '' ? SqlQuote($initialqacontactid) : 'NULL';

        SendSQL("UPDATE components
                 SET initialqacontact = $initialqacontactsql
                 WHERE id = $component_id");

        $vars->{'updated_initialqacontact'} = 1;
        $vars->{'initialqacontact'} = $initialqacontact;
    }


    if ($component ne $componentold) {
        unless ($component) {
            ThrowUserError('component_must_have_a_name',
                           {'name' => $componentold});
        }
        if (TestComponent($product, $component)) {
            ThrowUserError('component_already_exists',
                           {'name' => $component});
        }

        SendSQL("UPDATE components SET name=" . SqlQuote($component) . 
                 "WHERE id=$component_id");

        unlink "$datadir/versioncache";
        $vars->{'updated_name'} = 1;

    }

    $dbh->bz_unlock_tables();

    $vars->{'name'} = $component;
    $vars->{'product'} = $product;
    $template->process("admin/components/updated.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# No valid action found
#
ThrowUserError('no_valid_action', {'field' => "component"});
