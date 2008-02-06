#!/usr/bin/perl -wT
# -*- Mode: perl; indent-tabs-mode: nil -*-

#
# This is a script to edit the target milestones. It is largely a copy of
# the editversions.cgi script, since the two fields were set up in a
# very similar fashion.
#
# (basically replace each occurance of 'milestone' with 'version', and
# you'll have the original script)
#
# Matt Masson <matthew@zeroknowledge.com>
#
# Contributors : Gavin Shelley <bugzilla@chimpychompy.org>
#                Frédéric Buclin <LpSolit@gmail.com>
#


use strict;
use lib ".";

require "CGI.pl";
require "globals.pl";

use Bugzilla::Constants;
use Bugzilla::Config qw(:DEFAULT $datadir);
use Bugzilla::User;

use vars qw($template $vars);

my $cgi = Bugzilla->cgi;

# TestProduct:  just returns if the specified product does exists
# CheckProduct: same check, optionally  emit an error text
# TestMilestone:  just returns if the specified product/version combination exists
# CheckMilestone: same check, optionally emit an error text

sub TestProduct ($)
{
    my $product = shift;

    trick_taint($product);

    # does the product exist?
    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare_cached("SELECT name
                                    FROM products
                                    WHERE name = ?");
    $sth->execute($product);

    my ($row) = $sth->fetchrow_array;

    $sth->finish;

    return $row;
}

sub CheckProduct ($)
{
    my $product = shift;

    # do we have a product?
    unless ($product) {
        ThrowUserError('product_not_specified');    
    }

    # Does it exist in the DB?
    unless (TestProduct $product) {
        ThrowUserError('product_doesnt_exist',
                       {'product' => $product});
    }
}

sub TestMilestone ($$)
{
    my ($product, $milestone) = @_;

    my $dbh = Bugzilla->dbh;

    # does the product exist?
    my $sth = $dbh->prepare_cached("
             SELECT products.name, value
             FROM milestones
             INNER JOIN products
                ON milestones.product_id = products.id
             WHERE products.name = ?
               AND value = ?");

    trick_taint($product);
    trick_taint($milestone);

    $sth->execute($product, $milestone);

    my ($db_milestone) = $sth->fetchrow_array();

    $sth->finish();

    return $db_milestone;
}

sub CheckMilestone ($$)
{
    my ($product, $milestone) = @_;

    # do we have the milestone and product combination?
    unless ($milestone) {
        ThrowUserError('milestone_not_specified');
    }

    CheckProduct($product);

    unless (TestMilestone $product, $milestone) {
        ThrowUserError('milestone_not_valid',
                       {'product' => $product,
                        'milestone' => $milestone});
    }
}

sub CheckSortkey ($$)
{
    my ($milestone, $sortkey) = @_;
    # Keep a copy in case detaint_signed() clears the sortkey
    my $stored_sortkey = $sortkey;

    if (!detaint_signed($sortkey) || $sortkey < -32768 || $sortkey > 32767) {
        ThrowUserError('milestone_sortkey_invalid',
                       {'name' => $milestone,
                        'sortkey' => $stored_sortkey});
    }

    return $sortkey;
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
                                     object => "milestones"});

#
# often used variables
#
my $product = trim($cgi->param('product')     || '');
my $milestone = trim($cgi->param('milestone') || '');
my $sortkey = trim($cgi->param('sortkey')     || '0');
my $action  = trim($cgi->param('action')      || '');

#
# product = '' -> Show nice list of milestones
#

unless ($product) {

    my @products = ();

    my $dbh = Bugzilla->dbh;

    my $sth = $dbh->prepare_cached('SELECT products.name, products.description
                                    FROM products 
                                    ORDER BY products.name');

    my $data = $dbh->selectall_arrayref($sth);

    foreach my $aref (@$data) {

        my $prod = {};

        my ($name, $description) = @$aref;

        $prod->{'name'} = $name;
        $prod->{'description'} = $description;

        push(@products, $prod);
    }

    $vars->{'products'} = \@products;
    $template->process("admin/milestones/select-product.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='' -> Show nice list of milestones
#

unless ($action) {

    CheckProduct($product);
    my $product_id = get_product_id($product);
    my @milestones = ();

    my $dbh = Bugzilla->dbh;

    my $sth = $dbh->prepare_cached('SELECT value, sortkey
                                    FROM milestones
                                    WHERE product_id = ?
                                    ORDER BY sortkey, value');

    my $data = $dbh->selectall_arrayref($sth,
                                        undef,
                                        $product_id);

    foreach my $aref (@$data) {

        my $milestone = {};
        my ($name, $sortkey) = @$aref;

        $milestone->{'name'} = $name;
        $milestone->{'sortkey'} = $sortkey;

        push(@milestones, $milestone);
    }

    $vars->{'product'} = $product;
    $vars->{'milestones'} = \@milestones;
    $template->process("admin/milestones/list.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}




#
# action='add' -> present form for parameters for new milestone
#
# (next action will be 'new')
#

if ($action eq 'add') {

    CheckProduct($product);
    my $product_id = get_product_id($product);

    $vars->{'product'} = $product;
    $template->process("admin/milestones/create.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='new' -> add milestone entered in the 'action=add' screen
#

if ($action eq 'new') {

    CheckProduct($product);
    my $product_id = get_product_id($product);

    # Cleanups and valididy checks
    unless ($milestone) {
        ThrowUserError('milestone_blank_name',
                       {'name' => $milestone});
    }

    if (length($milestone) > 20) {
        ThrowUserError('milestone_name_too_long',
                       {'name' => $milestone});
    }

    $sortkey = CheckSortkey($milestone, $sortkey);

    if (TestMilestone($product, $milestone)) {
        ThrowUserError('milestone_already_exists',
                       {'name' => $milestone,
                        'product' => $product});
    }

    # Add the new milestone
    my $dbh = Bugzilla->dbh;
    trick_taint($milestone);
    $dbh->do('INSERT INTO milestones ( value, product_id, sortkey )
              VALUES ( ?, ?, ? )',
             undef,
             $milestone,
             $product_id,
             $sortkey);

    # Make versioncache flush
    unlink "$datadir/versioncache";

    $vars->{'name'} = $milestone;
    $vars->{'product'} = $product;
    $template->process("admin/milestones/created.html.tmpl",
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
    CheckMilestone($product, $milestone);
    my $product_id = get_product_id($product);
    my $dbh = Bugzilla->dbh;

    $vars->{'default_milestone'} =
      $dbh->selectrow_array('SELECT defaultmilestone
                             FROM products WHERE id = ?',
                             undef, $product_id);

    trick_taint($milestone);
    $vars->{'name'} = $milestone;
    $vars->{'product'} = $product;

    # The default milestone cannot be deleted.
    if ($vars->{'default_milestone'} eq $milestone) {
        ThrowUserError("milestone_is_default", $vars);
    }

    $vars->{'bug_count'} =
      $dbh->selectrow_array("SELECT COUNT(bug_id) FROM bugs
                             WHERE product_id = ? AND target_milestone = ?",
                             undef, ($product_id, $milestone)) || 0;

    $template->process("admin/milestones/confirm-delete.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}



#
# action='delete' -> really delete the milestone
#

if ($action eq 'delete') {
    CheckMilestone($product, $milestone);
    my $product_id = get_product_id($product);
    my $dbh = Bugzilla->dbh;

    my $default_milestone =
      $dbh->selectrow_array("SELECT defaultmilestone
                             FROM products WHERE id = ?",
                             undef, $product_id);

    trick_taint($milestone);
    $vars->{'name'} = $milestone;
    $vars->{'product'} = $product;

    # The default milestone cannot be deleted.
    if ($milestone eq $default_milestone) {
        ThrowUserError("milestone_is_default", $vars);
    }

    # We don't want to delete bugs when deleting a milestone.
    # Bugs concerned are reassigned to the default milestone.
    my $bug_ids =
      $dbh->selectcol_arrayref("SELECT bug_id FROM bugs
                                WHERE product_id = ? AND target_milestone = ?",
                                undef, ($product_id, $milestone));

    my $nb_bugs = scalar(@$bug_ids);
    if ($nb_bugs) {
        my $timestamp = $dbh->selectrow_array("SELECT NOW()");
        foreach my $bug_id (@$bug_ids) {
            $dbh->do("UPDATE bugs SET target_milestone = ?,
                      delta_ts = ? WHERE bug_id = ?",
                      undef, ($default_milestone, $timestamp, $bug_id));
            # We have to update the 'bugs_activity' table too.
            LogActivityEntry($bug_id, 'target_milestone', $milestone,
                             $default_milestone, $whoid, $timestamp);
        }
    }

    $vars->{'bug_count'} = $nb_bugs;

    $dbh->do("DELETE FROM milestones WHERE product_id = ? AND value = ?",
             undef, ($product_id, $milestone));

    unlink "$datadir/versioncache";

    $template->process("admin/milestones/deleted.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}



#
# action='edit' -> present the edit milestone form
#
# (next action would be 'update')
#

if ($action eq 'edit') {

    CheckMilestone($product, $milestone);
    my $product_id = get_product_id($product);

    my $dbh = Bugzilla->dbh;

    my $sth = $dbh->prepare_cached('SELECT sortkey
                                    FROM milestones
                                    WHERE product_id = ?
                                    AND value = ?');

    trick_taint($milestone);

    $vars->{'sortkey'} = $dbh->selectrow_array($sth,
                                               undef,
                                               $product_id,
                                               $milestone) || 0;

    $vars->{'name'} = $milestone;
    $vars->{'product'} = $product;

    $template->process("admin/milestones/edit.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='update' -> update the milestone
#

if ($action eq 'update') {

    my $milestoneold = trim($cgi->param('milestoneold') || '');
    my $sortkeyold = trim($cgi->param('sortkeyold')     || '0');

    CheckMilestone($product, $milestoneold);
    my $product_id = get_product_id($product);

    if (length($milestone) > 20) {
        ThrowUserError('milestone_name_too_long',
                       {'name' => $milestone});
    }

    my $dbh = Bugzilla->dbh;

    $dbh->bz_lock_tables('bugs WRITE',
                         'milestones WRITE',
                         'products WRITE');

    if ($sortkey ne $sortkeyold) {
        $sortkey = CheckSortkey($milestone, $sortkey);

        trick_taint($milestoneold);

        $dbh->do('UPDATE milestones SET sortkey = ?
                  WHERE product_id = ?
                  AND value = ?',
                 undef,
                 $sortkey,
                 $product_id,
                 $milestoneold);

        unlink "$datadir/versioncache";
        $vars->{'updated_sortkey'} = 1;
        $vars->{'sortkey'} = $sortkey;
    }

    if ($milestone ne $milestoneold) {
        unless ($milestone) {
            ThrowUserError('milestone_blank_name');
        }
        if (TestMilestone($product, $milestone)) {
            ThrowUserError('milestone_already_exists',
                           {'name' => $milestone,
                            'product' => $product});
        }

        trick_taint($milestone);
        trick_taint($milestoneold);

        $dbh->do('UPDATE bugs
                  SET target_milestone = ?
                  WHERE target_milestone = ?
                  AND product_id = ?',
                 undef,
                 $milestone,
                 $milestoneold,
                 $product_id);

        $dbh->do("UPDATE milestones
                  SET value = ?
                  WHERE product_id = ?
                  AND value = ?",
                 undef,
                 $milestone,
                 $product_id,
                 $milestoneold);

        $dbh->do("UPDATE products
                  SET defaultmilestone = ?
                  WHERE id = ?
                  AND defaultmilestone = ?",
                 undef,
                 $milestone,
                 $product_id,
                 $milestoneold);

        unlink "$datadir/versioncache";

        $vars->{'updated_name'} = 1;
    }

    $dbh->bz_unlock_tables();

    $vars->{'name'} = $milestone;
    $vars->{'product'} = $product;
    $template->process("admin/milestones/updated.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}


#
# No valid action found
#
ThrowUserError('no_valid_action', {'field' => "target_milestone"});
