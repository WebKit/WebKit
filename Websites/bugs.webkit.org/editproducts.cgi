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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Holger
# Schurig. Portions created by Holger Schurig are
# Copyright (C) 1999 Holger Schurig. All
# Rights Reserved.
#
# Contributor(s): Holger Schurig <holgerschurig@nikocity.de>
#               Terry Weissman <terry@mozilla.org>
#               Dawn Endico <endico@mozilla.org>
#               Joe Robins <jmrobins@tgix.com>
#               Gavin Shelley <bugzilla@chimpychompy.org>
#               Frédéric Buclin <LpSolit@gmail.com>
#               Greg Hendricks <ghendricks@novell.com>
#               Lance Larsh <lance.larsh@oracle.com>
#               Elliotte Martin <elliotte.martin@yahoo.com>

use strict;
use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Group;
use Bugzilla::Product;
use Bugzilla::Classification;
use Bugzilla::Token;

#
# Preliminary checks:
#

my $user = Bugzilla->login(LOGIN_REQUIRED);
my $whoid = $user->id;

my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};
# Remove this as soon as the documentation about products has been
# improved and each action has its own section.
$vars->{'doc_section'} = 'products.html';

print $cgi->header();

$user->in_group('editcomponents')
  || scalar(@{$user->get_products_by_permission('editcomponents')})
  || ThrowUserError("auth_failure", {group  => "editcomponents",
                                     action => "edit",
                                     object => "products"});

#
# often used variables
#
my $classification_name = trim($cgi->param('classification') || '');
my $product_name = trim($cgi->param('product') || '');
my $action  = trim($cgi->param('action')  || '');
my $token = $cgi->param('token');

#
# product = '' -> Show nice list of classifications (if
# classifications enabled)
#

if (Bugzilla->params->{'useclassification'} 
    && !$classification_name
    && !$product_name)
{
    my $class;
    if ($user->in_group('editcomponents')) {
        $class = [Bugzilla::Classification->get_all];
    }
    else {
        # Only keep classifications containing at least one product
        # which you can administer.
        my $products = $user->get_products_by_permission('editcomponents');
        my %class_ids = map { $_->classification_id => 1 } @$products;
        $class = Bugzilla::Classification->new_from_list([keys %class_ids]);
    }
    $vars->{'classifications'} = $class;

    $template->process("admin/products/list-classifications.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}


#
# action = '' -> Show a nice list of products, unless a product
#                is already specified (then edit it)
#

if (!$action && !$product_name) {
    my $classification;
    my $products;

    if (Bugzilla->params->{'useclassification'}) {
        $classification = Bugzilla::Classification->check($classification_name);
        $products = $user->get_selectable_products($classification->id);
        $vars->{'classification'} = $classification;
    } else {
        $products = $user->get_selectable_products;
    }

    # If the user has editcomponents privs for some products only,
    # we have to restrict the list of products to display.
    unless ($user->in_group('editcomponents')) {
        $products = $user->get_products_by_permission('editcomponents');
        if (Bugzilla->params->{'useclassification'}) {
            @$products = grep {$_->classification_id == $classification->id} @$products;
        }
    }
    $vars->{'products'} = $products;
    $vars->{'showbugcounts'} = $cgi->param('showbugcounts') ? 1 : 0;

    $template->process("admin/products/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}




#
# action='add' -> present form for parameters for new product
#
# (next action will be 'new')
#

if ($action eq 'add') {
    # The user must have the global editcomponents privs to add
    # new products.
    $user->in_group('editcomponents')
      || ThrowUserError("auth_failure", {group  => "editcomponents",
                                         action => "add",
                                         object => "products"});

    if (Bugzilla->params->{'useclassification'}) {
        my $classification = Bugzilla::Classification->check($classification_name);
        $vars->{'classification'} = $classification;
    }
    $vars->{'token'} = issue_session_token('add_product');

    $template->process("admin/products/create.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}


#
# action='new' -> add product entered in the 'action=add' screen
#

if ($action eq 'new') {
    # The user must have the global editcomponents privs to add
    # new products.
    $user->in_group('editcomponents')
      || ThrowUserError("auth_failure", {group  => "editcomponents",
                                         action => "add",
                                         object => "products"});

    check_token_data($token, 'add_product');

    my %create_params = (
        classification   => $classification_name,
        name             => $product_name,
        description      => scalar $cgi->param('description'),
        version          => scalar $cgi->param('version'),
        defaultmilestone => scalar $cgi->param('defaultmilestone'),
        isactive         => scalar $cgi->param('is_active'),
        create_series    => scalar $cgi->param('createseries'),
        allows_unconfirmed => scalar $cgi->param('allows_unconfirmed'),
    );
    my $product = Bugzilla::Product->create(\%create_params);

    delete_token($token);

    $vars->{'message'} = 'product_created';
    $vars->{'product'} = $product;
    if (Bugzilla->params->{'useclassification'}) {
        $vars->{'classification'} = new Bugzilla::Classification($product->classification_id);
    }
    $vars->{'token'} = issue_session_token('edit_product');

    $template->process("admin/products/edit.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    my $product = $user->check_can_admin_product($product_name);

    if (Bugzilla->params->{'useclassification'}) {
        $vars->{'classification'} = new Bugzilla::Classification($product->classification_id);
    }
    $vars->{'product'} = $product;
    $vars->{'token'} = issue_session_token('delete_product');
    
    Bugzilla::Hook::process('product_confirm_delete', { vars => $vars });
    
    $template->process("admin/products/confirm-delete.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='delete' -> really delete the product
#

if ($action eq 'delete') {
    my $product = $user->check_can_admin_product($product_name);
    check_token_data($token, 'delete_product');

    $product->remove_from_db({ delete_series => scalar $cgi->param('delete_series')});
    delete_token($token);

    $vars->{'message'} = 'product_deleted';
    $vars->{'product'} = $product;
    $vars->{'no_edit_product_link'} = 1;

    if (Bugzilla->params->{'useclassification'}) {
        $vars->{'classifications'} = $user->in_group('editcomponents') ?
          [Bugzilla::Classification->get_all] : $user->get_selectable_classifications;

        $template->process("admin/products/list-classifications.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
    }
    else {
        my $products = $user->get_selectable_products;
        # If the user has editcomponents privs for some products only,
        # we have to restrict the list of products to display.
        unless ($user->in_group('editcomponents')) {
            $products = $user->get_products_by_permission('editcomponents');
        }
        $vars->{'products'} = $products;

        $template->process("admin/products/list.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
    }
    exit;
}

#
# action='edit' -> present the 'edit product' form
# If a product is given with no action associated with it, then edit it.
#
# (next action would be 'update')
#

if ($action eq 'edit' || (!$action && $product_name)) {
    my $product = $user->check_can_admin_product($product_name);

    if (Bugzilla->params->{'useclassification'}) {
        $vars->{'classification'} = new Bugzilla::Classification($product->classification_id);
    }
    $vars->{'product'} = $product;
    $vars->{'token'} = issue_session_token('edit_product');

    $template->process("admin/products/edit.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='update' -> update the product
#
if ($action eq 'update') {
    check_token_data($token, 'edit_product');
    my $product_old_name = trim($cgi->param('product_old_name') || '');
    my $product = $user->check_can_admin_product($product_old_name);

    $product->set_all({
        name        => $product_name,
        description => scalar $cgi->param('description'),
        is_active   => scalar $cgi->param('is_active'),
        allows_unconfirmed => scalar $cgi->param('allows_unconfirmed'),
        default_milestone  => scalar $cgi->param('defaultmilestone'),
    });

    my $changes = $product->update();

    delete_token($token);

    if (Bugzilla->params->{'useclassification'}) {
        $vars->{'classification'} = new Bugzilla::Classification($product->classification_id);
    }
    $vars->{'product'} = $product;
    $vars->{'changes'} = $changes;

    $template->process("admin/products/updated.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='editgroupcontrols' -> display product group controls
#

if ($action eq 'editgroupcontrols') {
    my $product = $user->check_can_admin_product($product_name);

    $vars->{'product'} = $product;
    $vars->{'token'} = issue_session_token('edit_group_controls');

    $template->process("admin/products/groupcontrol/edit.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='updategroupcontrols' -> update product group controls
#

if ($action eq 'updategroupcontrols') {
    my $product = $user->check_can_admin_product($product_name);
    check_token_data($token, 'edit_group_controls');

    my @now_na = ();
    my @now_mandatory = ();
    foreach my $f ($cgi->param()) {
        if ($f =~ /^membercontrol_(\d+)$/) {
            my $id = $1;
            if ($cgi->param($f) == CONTROLMAPNA) {
                push @now_na,$id;
            } elsif ($cgi->param($f) == CONTROLMAPMANDATORY) {
                push @now_mandatory,$id;
            }
        }
    }
    if (!defined $cgi->param('confirmed')) {
        my $na_groups;
        if (@now_na) {
            $na_groups = $dbh->selectall_arrayref(
                    'SELECT groups.name, COUNT(bugs.bug_id) AS count
                       FROM bugs
                 INNER JOIN bug_group_map
                         ON bug_group_map.bug_id = bugs.bug_id
                 INNER JOIN groups
                         ON bug_group_map.group_id = groups.id
                      WHERE groups.id IN (' . join(', ', @now_na) . ')
                        AND bugs.product_id = ? ' .
                       $dbh->sql_group_by('groups.name'),
                   {'Slice' => {}}, $product->id);
        }

        # return the mandatory groups which need to have bug entries
        # added to the bug_group_map and the corresponding bug count

        my $mandatory_groups;
        if (@now_mandatory) {
            $mandatory_groups = $dbh->selectall_arrayref(
                    'SELECT groups.name,
                           (SELECT COUNT(bugs.bug_id)
                              FROM bugs
                             WHERE bugs.product_id = ?
                               AND bugs.bug_id NOT IN
                                (SELECT bug_group_map.bug_id FROM bug_group_map
                                  WHERE bug_group_map.group_id = groups.id))
                           AS count
                      FROM groups
                     WHERE groups.id IN (' . join(', ', @now_mandatory) . ')
                     ORDER BY groups.name',
                   {'Slice' => {}}, $product->id);
            # remove zero counts
            @$mandatory_groups = grep { $_->{count} } @$mandatory_groups;

        }
        if (($na_groups && scalar(@$na_groups))
            || ($mandatory_groups && scalar(@$mandatory_groups)))
        {
            $vars->{'product'} = $product;
            $vars->{'na_groups'} = $na_groups;
            $vars->{'mandatory_groups'} = $mandatory_groups;
            $template->process("admin/products/groupcontrol/confirm-edit.html.tmpl", $vars)
                || ThrowTemplateError($template->error());
            exit;
        }
    }

    my $groups = Bugzilla::Group->match({isactive => 1, isbuggroup => 1});
    foreach my $group (@$groups) {
        my $group_id = $group->id;
        $product->set_group_controls($group,
                                     {entry          => scalar $cgi->param("entry_$group_id") || 0,
                                      membercontrol  => scalar $cgi->param("membercontrol_$group_id") || CONTROLMAPNA,
                                      othercontrol   => scalar $cgi->param("othercontrol_$group_id") || CONTROLMAPNA,
                                      canedit        => scalar $cgi->param("canedit_$group_id") || 0,
                                      editcomponents => scalar $cgi->param("editcomponents_$group_id") || 0,
                                      editbugs       => scalar $cgi->param("editbugs_$group_id") || 0,
                                      canconfirm     => scalar $cgi->param("canconfirm_$group_id") || 0});
    }
    my $changes = $product->update;

    delete_token($token);

    $vars->{'product'} = $product;
    $vars->{'changes'} = $changes;

    $template->process("admin/products/groupcontrol/updated.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

# No valid action found
ThrowUserError('unknown_action', {action => $action});
