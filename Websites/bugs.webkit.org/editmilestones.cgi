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
use Bugzilla::Milestone;
use Bugzilla::Token;

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};
# There is only one section about milestones in the documentation,
# so all actions point to the same page.
$vars->{'doc_section'} = 'administering/categorization.html#milestones';

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
my $isactive       = $cgi->param('isactive');

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

    my $milestone = Bugzilla::Milestone->create({ value    => $milestone_name,
                                                  product  => $product,
                                                  sortkey  => $sortkey });

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
    $milestone->set_is_active($isactive);
    my $changes = $milestone->update();
    # Reloading the product since the default milestone name
    # could have been changed.
    $product = new Bugzilla::Product({ name => $product_name });

    delete_token($token);

    $vars->{'message'} = 'milestone_updated';
    $vars->{'milestone'} = $milestone;
    $vars->{'product'} = $product;
    $vars->{'changes'} = $changes;
    $template->process("admin/milestones/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

# No valid action found
ThrowUserError('unknown_action', {action => $action});
