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
use Bugzilla::Version;
use Bugzilla::Token;

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};
# There is only one section about versions in the documentation,
# so all actions point to the same page.
$vars->{'doc_section'} = 'administering/categorization.html#versions';

#
# Preliminary checks:
#

my $user = Bugzilla->login(LOGIN_REQUIRED);

print $cgi->header();

$user->in_group('editcomponents')
  || scalar(@{$user->get_products_by_permission('editcomponents')})
  || ThrowUserError("auth_failure", {group  => "editcomponents",
                                     action => "edit",
                                     object => "versions"});

#
# often used variables
#
my $product_name = trim($cgi->param('product') || '');
my $version_name = trim($cgi->param('version') || '');
my $action       = trim($cgi->param('action')  || '');
my $showbugcounts = (defined $cgi->param('showbugcounts'));
my $token        = $cgi->param('token');
my $isactive     = $cgi->param('isactive');

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

    $template->process("admin/versions/select-product.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

my $product = $user->check_can_admin_product($product_name);

#
# action='' -> Show nice list of versions
#

unless ($action) {
    $vars->{'showbugcounts'} = $showbugcounts;
    $vars->{'product'} = $product;
    $template->process("admin/versions/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# action='add' -> present form for parameters for new version
#
# (next action will be 'new')
#

if ($action eq 'add') {
    $vars->{'token'} = issue_session_token('add_version');
    $vars->{'product'} = $product;
    $template->process("admin/versions/create.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# action='new' -> add version entered in the 'action=add' screen
#

if ($action eq 'new') {
    check_token_data($token, 'add_version');
    my $version = Bugzilla::Version->create(
        { value => $version_name, product => $product });
    delete_token($token);

    $vars->{'message'} = 'version_created';
    $vars->{'version'} = $version;
    $vars->{'product'} = $product;
    $template->process("admin/versions/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    my $version = Bugzilla::Version->check({ product => $product,
                                             name    => $version_name });
    $vars->{'version'} = $version;
    $vars->{'product'} = $product;
    $vars->{'token'} = issue_session_token('delete_version');
    $template->process("admin/versions/confirm-delete.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# action='delete' -> really delete the version
#

if ($action eq 'delete') {
    check_token_data($token, 'delete_version');
    my $version = Bugzilla::Version->check({ product => $product, 
                                             name    => $version_name });
    $version->remove_from_db;
    delete_token($token);

    $vars->{'message'} = 'version_deleted';
    $vars->{'version'} = $version;
    $vars->{'product'} = $product;
    $vars->{'no_edit_version_link'} = 1;

    $template->process("admin/versions/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# action='edit' -> present the edit version form
#
# (next action would be 'update')
#

if ($action eq 'edit') {
    my $version = Bugzilla::Version->check({ product => $product,
                                             name    => $version_name });
    $vars->{'version'} = $version;
    $vars->{'product'} = $product;
    $vars->{'token'} = issue_session_token('edit_version');

    $template->process("admin/versions/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# action='update' -> update the version
#

if ($action eq 'update') {
    check_token_data($token, 'edit_version');
    my $version_old_name = trim($cgi->param('versionold') || '');
    my $version = Bugzilla::Version->check({ product => $product,
                                             name   => $version_old_name });

    $dbh->bz_start_transaction();

    $version->set_all({
        value    =>  $version_name,
        isactive =>  $isactive
    });
    my $changes = $version->update();

    $dbh->bz_commit_transaction();
    delete_token($token);

    $vars->{'message'} = 'version_updated';
    $vars->{'version'} = $version;
    $vars->{'product'} = $product;
    $vars->{'changes'} = $changes;
    $template->process("admin/versions/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}

# No valid action found
ThrowUserError('unknown_action', {action => $action});
