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
use Bugzilla::User;
use Bugzilla::Component;
use Bugzilla::Token;

my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};
# There is only one section about components in the documentation,
# so all actions point to the same page.
$vars->{'doc_section'} = 'administering/categorization.html#components';

#
# Preliminary checks:
#

my $user = Bugzilla->login(LOGIN_REQUIRED);

print $cgi->header();

$user->in_group('editcomponents')
  || scalar(@{$user->get_products_by_permission('editcomponents')})
  || ThrowUserError("auth_failure", {group  => "editcomponents",
                                     action => "edit",
                                     object => "components"});

#
# often used variables
#
my $product_name  = trim($cgi->param('product')     || '');
my $comp_name     = trim($cgi->param('component')   || '');
my $action        = trim($cgi->param('action')      || '');
my $showbugcounts = (defined $cgi->param('showbugcounts'));
my $token         = $cgi->param('token');

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

    $template->process("admin/components/select-product.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

my $product = $user->check_can_admin_product($product_name);

#
# action='' -> Show nice list of components
#

unless ($action) {
    $vars->{'showbugcounts'} = $showbugcounts;
    $vars->{'product'} = $product;
    $template->process("admin/components/list.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='add' -> present form for parameters for new component
#
# (next action will be 'new')
#

if ($action eq 'add') {
    $vars->{'token'} = issue_session_token('add_component');
    $vars->{'product'} = $product;
    $template->process("admin/components/create.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='new' -> add component entered in the 'action=add' screen
#

if ($action eq 'new') {
    check_token_data($token, 'add_component');
    # Do the user matching
    Bugzilla::User::match_field ({
        'initialowner'     => { 'type' => 'single' },
        'initialqacontact' => { 'type' => 'single' },
        'initialcc'        => { 'type' => 'multi'  },
    });

    my $default_assignee   = trim($cgi->param('initialowner')     || '');
    my $default_qa_contact = trim($cgi->param('initialqacontact') || '');
    my $description        = trim($cgi->param('description')      || '');
    my @initial_cc         = $cgi->param('initialcc');
    my $isactive           = $cgi->param('isactive');

    my $component = Bugzilla::Component->create({
        name             => $comp_name,
        product          => $product,
        description      => $description,
        initialowner     => $default_assignee,
        initialqacontact => $default_qa_contact,
        initial_cc       => \@initial_cc,
        # XXX We should not be creating series for products that we
        # didn't create series for.
        create_series    => 1,
   });

    $vars->{'message'} = 'component_created';
    $vars->{'comp'} = $component;
    $vars->{'product'} = $product;
    delete_token($token);

    $template->process("admin/components/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    $vars->{'token'} = issue_session_token('delete_component');
    $vars->{'comp'} =
      Bugzilla::Component->check({ product => $product, name => $comp_name });
    $vars->{'product'} = $product;

    $template->process("admin/components/confirm-delete.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='delete' -> really delete the component
#

if ($action eq 'delete') {
    check_token_data($token, 'delete_component');
    my $component =
        Bugzilla::Component->check({ product => $product, name => $comp_name });

    $component->remove_from_db;

    $vars->{'message'} = 'component_deleted';
    $vars->{'comp'} = $component;
    $vars->{'product'} = $product;
    $vars->{'no_edit_component_link'} = 1;
    delete_token($token);

    $template->process("admin/components/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='edit' -> present the edit component form
#
# (next action would be 'update')
#

if ($action eq 'edit') {
    $vars->{'token'} = issue_session_token('edit_component');
    my $component =
        Bugzilla::Component->check({ product => $product, name => $comp_name });
    $vars->{'comp'} = $component;

    $vars->{'initial_cc_names'} = 
        join(', ', map($_->login, @{$component->initial_cc}));

    $vars->{'product'} = $product;

    $template->process("admin/components/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='update' -> update the component
#

if ($action eq 'update') {
    check_token_data($token, 'edit_component');
    # Do the user matching
    Bugzilla::User::match_field ({
        'initialowner'     => { 'type' => 'single' },
        'initialqacontact' => { 'type' => 'single' },
        'initialcc'        => { 'type' => 'multi'  },
    });

    my $comp_old_name         = trim($cgi->param('componentold')     || '');
    my $default_assignee      = trim($cgi->param('initialowner')     || '');
    my $default_qa_contact    = trim($cgi->param('initialqacontact') || '');
    my $description           = trim($cgi->param('description')      || '');
    my @initial_cc            = $cgi->param('initialcc');
    my $isactive              = $cgi->param('isactive');
  
    my $component =
        Bugzilla::Component->check({ product => $product, name => $comp_old_name });

    $component->set_name($comp_name);
    $component->set_description($description);
    $component->set_default_assignee($default_assignee);
    $component->set_default_qa_contact($default_qa_contact);
    $component->set_cc_list(\@initial_cc);
    $component->set_is_active($isactive);
    my $changes = $component->update();

    $vars->{'message'} = 'component_updated';
    $vars->{'comp'} = $component;
    $vars->{'product'} = $product;
    $vars->{'changes'} = $changes;
    delete_token($token);

    $template->process("admin/components/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

# No valid action found
ThrowUserError('unknown_action', {action => $action});
