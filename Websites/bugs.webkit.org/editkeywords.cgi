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
use Bugzilla::Keyword;
use Bugzilla::Token;

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};

my $user = Bugzilla->login(LOGIN_REQUIRED);

print $cgi->header();

$user->in_group('editkeywords')
  || ThrowUserError("auth_failure", {group  => "editkeywords",
                                     action => "edit",
                                     object => "keywords"});

my $action = trim($cgi->param('action')  || '');
my $key_id = $cgi->param('id');
my $token  = $cgi->param('token');

$vars->{'action'} = $action;


if ($action eq "") {
    $vars->{'keywords'} = Bugzilla::Keyword->get_all_with_bug_count();

    $template->process("admin/keywords/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'add') {
    $vars->{'token'} = issue_session_token('add_keyword');

    $template->process("admin/keywords/create.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='new' -> add keyword entered in the 'action=add' screen
#
if ($action eq 'new') {
    check_token_data($token, 'add_keyword');
    my $name = $cgi->param('name') || '';
    my $desc = $cgi->param('description')  || '';

    my $keyword = Bugzilla::Keyword->create(
        { name => $name, description => $desc });

    delete_token($token);

    $vars->{'message'} = 'keyword_created';
    $vars->{'name'} = $keyword->name;
    $vars->{'keywords'} = Bugzilla::Keyword->get_all_with_bug_count();

    $template->process("admin/keywords/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}


#
# action='edit' -> present the edit keywords from
#
# (next action would be 'update')
#

if ($action eq 'edit') {
    my $keyword = new Bugzilla::Keyword($key_id)
        || ThrowUserError('invalid_keyword_id', { id => $key_id });

    $vars->{'keyword'} = $keyword;
    $vars->{'token'} = issue_session_token('edit_keyword');

    $template->process("admin/keywords/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}


#
# action='update' -> update the keyword
#

if ($action eq 'update') {
    check_token_data($token, 'edit_keyword');
    my $keyword = new Bugzilla::Keyword($key_id)
        || ThrowUserError('invalid_keyword_id', { id => $key_id });

    $keyword->set_all({
        name        => scalar $cgi->param('name'),
        description => scalar $cgi->param('description'),
    });
    my $changes = $keyword->update();

    delete_token($token);

    $vars->{'message'} = 'keyword_updated';
    $vars->{'keyword'} = $keyword;
    $vars->{'changes'} = $changes;
    $vars->{'keywords'} = Bugzilla::Keyword->get_all_with_bug_count();

    $template->process("admin/keywords/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'del') {
    my $keyword =  new Bugzilla::Keyword($key_id)
        || ThrowUserError('invalid_keyword_id', { id => $key_id });

    $vars->{'keyword'} = $keyword;
    $vars->{'token'} = issue_session_token('delete_keyword');

    $template->process("admin/keywords/confirm-delete.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'delete') {
    check_token_data($token, 'delete_keyword');
    my $keyword =  new Bugzilla::Keyword($key_id)
        || ThrowUserError('invalid_keyword_id', { id => $key_id });

    $keyword->remove_from_db();

    delete_token($token);

    $vars->{'message'} = 'keyword_deleted';
    $vars->{'keyword'} = $keyword;
    $vars->{'keywords'} = Bugzilla::Keyword->get_all_with_bug_count();

    $template->process("admin/keywords/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

ThrowUserError('unknown_action', {action => $action});
