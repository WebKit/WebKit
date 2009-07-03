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
# The Initial Developer of the Original Code is Matt Masson.
# Portions created by Matt Masson are Copyright (C) 2000 Matt Masson.
# All Rights Reserved.
#
# Contributors : Matt Masson <matthew@zeroknowledge.com>
#                Gavin Shelley <bugzilla@chimpychompy.org>
#                Frédéric Buclin <LpSolit@gmail.com>

use strict;
use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Milestone;
use Bugzilla::Token;

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};
# There is only one section about milestones in the documentation,
# so all actions point to the same page.
$vars->{'doc_section'} = 'milestones.html';

#
# Preliminary checks:
#

my $user = Bugzilla->login(LOGIN_REQUIRED);

print $cgi->header();

$user->in_group('editcomponents')
  || scalar(@{$user->get_products_by_permission('editcomponents')})
  || ThrowUserError("auth_failure", {group  => "editcomponents",
                                     action => "edit",
                                     object => "milestones"});

#
# often used variables
#
my $product_name   = trim($cgi->param('product')     || '');
my $milestone_name = trim($cgi->param('milestone')   || '');
my $sortkey        = trim($cgi->param('sortkey')     || 0);
my $action         = trim($cgi->param('action')      || '');
my $showbugcounts = (defined $cgi->param('showbugcounts'));
my $token          = $cgi->param('token');

#
# product = '' -> Show nice list of products
#

unless ($product_name) {
    my $selectable_products = $user->get_selectable_products;
    # If the user has editcomponents privs for some products only,
    # we have to restrict the list of products to display.
    unless ($user->in_group('editcomponents')) {
        $selectable_products = $user->get_products_by_permission('editcomponents');
    }
    $vars->{'products'} = $selectable_products;
    $vars->{'showbugcounts'} = $showbugcounts;

    $template->process("admin/milestones/select-product.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

my $product = $user->check_can_admin_product($product_name);

#
# action='' -> Show nice list of milestones
#

unless ($action) {

    $vars->{'showbugcounts'} = $showbugcounts;
    $vars->{'product'} = $product;
    $template->process("admin/milestones/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='add' -> present form for parameters for new milestone
#
# (next action will be 'new')
#

if ($action eq 'add') {
    $vars->{'token'} = issue_session_token('add_milestone');
    $vars->{'product'} = $product;
    $template->process("admin/milestones/create.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='new' -> add milestone entered in the 'action=add' screen
#

if ($action eq 'new') {
    check_token_data($token, 'add_milestone');
    my $milestone = Bugzilla::Milestone->create({ name    => $milestone_name,
                                                  product => $product,
                                                  sortkey => $sortkey });
    delete_token($token);

    $vars->{'message'} = 'milestone_created';
    $vars->{'milestone'} = $milestone;
    $vars->{'product'} = $product;
    $template->process("admin/milestones/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    my $milestone = Bugzilla::Milestone->check({ product => $product,
                                                 name    => $milestone_name });
    
    $vars->{'milestone'} = $milestone;
    $vars->{'product'} = $product;

    # The default milestone cannot be deleted.
    if ($product->default_milestone eq $milestone->name) {
        ThrowUserError("milestone_is_default", { milestone => $milestone });
    }
    $vars->{'token'} = issue_session_token('delete_milestone');

    $template->process("admin/milestones/confirm-delete.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='delete' -> really delete the milestone
#

if ($action eq 'delete') {
    check_token_data($token, 'delete_milestone');
    my $milestone = Bugzilla::Milestone->check({ product => $product,
                                                 name    => $milestone_name });
    $milestone->remove_from_db;
    delete_token($token);

    $vars->{'message'} = 'milestone_deleted';
    $vars->{'milestone'} = $milestone;
    $vars->{'product'} = $product;
    $vars->{'no_edit_milestone_link'} = 1;

    $template->process("admin/milestones/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='edit' -> present the edit milestone form
#
# (next action would be 'update')
#

if ($action eq 'edit') {

    my $milestone = Bugzilla::Milestone->check({ product => $product,
                                                 name    => $milestone_name });

    $vars->{'milestone'} = $milestone;
    $vars->{'product'} = $product;
    $vars->{'token'} = issue_session_token('edit_milestone');

    $template->process("admin/milestones/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='update' -> update the milestone
#

if ($action eq 'update') {
    check_token_data($token, 'edit_milestone');
    my $milestone_old_name = trim($cgi->param('milestoneold') || '');
    my $milestone = Bugzilla::Milestone->check({ product => $product,
                                                 name    => $milestone_old_name });

    $milestone->set_name($milestone_name);
    $milestone->set_sortkey($sortkey);
    my $changes = $milestone->update();

    delete_token($token);

    $vars->{'message'} = 'milestone_updated';
    $vars->{'milestone'} = $milestone;
    $vars->{'product'} = $product;
    $vars->{'changes'} = $changes;
    $template->process("admin/milestones/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# No valid action found
#
ThrowUserError('no_valid_action', {'field' => "target_milestone"});
