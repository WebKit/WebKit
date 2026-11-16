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
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Field;
use Bugzilla::Token;

my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};

# Make sure the user is logged in and is an administrator.
my $user = Bugzilla->login(LOGIN_REQUIRED);
$user->in_group('admin')
  || ThrowUserError('auth_failure', {group  => 'admin',
                                     action => 'edit',
                                     object => 'custom_fields'});

my $action = trim($cgi->param('action') || '');
my $token  = $cgi->param('token');

print $cgi->header();

# List all existing custom fields if no action is given.
if (!$action) {
    $template->process('admin/custom_fields/list.html.tmpl', $vars)
        || ThrowTemplateError($template->error());
}
# Interface to add a new custom field.
elsif ($action eq 'add') {
    $vars->{'token'} = issue_session_token('add_field');

    $template->process('admin/custom_fields/create.html.tmpl', $vars)
        || ThrowTemplateError($template->error());
}
elsif ($action eq 'new') {
    check_token_data($token, 'add_field');
    $vars->{'field'} = Bugzilla::Field->create({
        name        => scalar $cgi->param('name'),
        description => scalar $cgi->param('desc'),
        long_desc   => scalar $cgi->param('long_desc'),
        type        => scalar $cgi->param('type'),
        sortkey     => scalar $cgi->param('sortkey'),
        mailhead    => scalar $cgi->param('new_bugmail'),
        enter_bug   => scalar $cgi->param('enter_bug'),
        obsolete    => scalar $cgi->param('obsolete'),
        custom      => 1,
        buglist     => 1,
        visibility_field_id => scalar $cgi->param('visibility_field_id'),
        visibility_values => [ $cgi->param('visibility_values') ],
        value_field_id => scalar $cgi->param('value_field_id'),
        reverse_desc => scalar $cgi->param('reverse_desc'),
        is_mandatory => scalar $cgi->param('is_mandatory'),
    });

    delete_token($token);

    $vars->{'message'} = 'custom_field_created';

    $template->process('admin/custom_fields/list.html.tmpl', $vars)
        || ThrowTemplateError($template->error());
}
elsif ($action eq 'edit') {
    my $name = $cgi->param('name') || ThrowUserError('field_missing_name');
    # Custom field names must start with "cf_".
    if ($name !~ /^cf_/) {
        $name = 'cf_' . $name;
    }
    my $field = new Bugzilla::Field({'name' => $name});
    $field || ThrowUserError('customfield_nonexistent', {'name' => $name});

    $vars->{'field'} = $field;
    $vars->{'token'} = issue_session_token('edit_field');

    $template->process('admin/custom_fields/edit.html.tmpl', $vars)
        || ThrowTemplateError($template->error());
}
elsif ($action eq 'update') {
    check_token_data($token, 'edit_field');
    my $name = $cgi->param('name');

    # Validate fields.
    $name || ThrowUserError('field_missing_name');
    # Custom field names must start with "cf_".
    if ($name !~ /^cf_/) {
        $name = 'cf_' . $name;
    }
    my $field = new Bugzilla::Field({'name' => $name});
    $field || ThrowUserError('customfield_nonexistent', {'name' => $name});

    $field->set_description($cgi->param('desc'));
    $field->set_long_desc($cgi->param('long_desc'));
    $field->set_sortkey($cgi->param('sortkey'));
    $field->set_in_new_bugmail($cgi->param('new_bugmail'));
    $field->set_enter_bug($cgi->param('enter_bug'));
    $field->set_obsolete($cgi->param('obsolete'));
    $field->set_is_mandatory($cgi->param('is_mandatory'));
    $field->set_visibility_field($cgi->param('visibility_field_id'));
    $field->set_visibility_values([ $cgi->param('visibility_values') ]);
    $field->set_value_field($cgi->param('value_field_id'));
    $field->set_reverse_desc($cgi->param('reverse_desc'));
    $field->update();

    delete_token($token);

    $vars->{'field'}   = $field;
    $vars->{'message'} = 'custom_field_updated';

    $template->process('admin/custom_fields/list.html.tmpl', $vars)
        || ThrowTemplateError($template->error());
}
elsif ($action eq 'del') {
    my $name = $cgi->param('name');

    # Validate field.
    $name || ThrowUserError('field_missing_name');
    # Custom field names must start with "cf_".
    if ($name !~ /^cf_/) {
        $name = 'cf_' . $name;
    }
    my $field = new Bugzilla::Field({'name' => $name});
    $field || ThrowUserError('customfield_nonexistent', {'name' => $name});

    $vars->{'field'} = $field;
    $vars->{'token'} = issue_session_token('delete_field');

    $template->process('admin/custom_fields/confirm-delete.html.tmpl', $vars)
            || ThrowTemplateError($template->error());
}
elsif ($action eq 'delete') {
    check_token_data($token, 'delete_field');
    my $name = $cgi->param('name');

    # Validate fields.
    $name || ThrowUserError('field_missing_name');
    # Custom field names must start with "cf_".
    if ($name !~ /^cf_/) {
        $name = 'cf_' . $name;
    }
    my $field = new Bugzilla::Field({'name' => $name});
    $field || ThrowUserError('customfield_nonexistent', {'name' => $name});

    # Calling remove_from_db will check if field can be deleted.
    # If the field cannot be deleted, it will throw an error.
    $field->remove_from_db();
    
    $vars->{'field'}   = $field;
    $vars->{'message'} = 'custom_field_deleted';
    
    delete_token($token);

    $template->process('admin/custom_fields/list.html.tmpl', $vars)
        || ThrowTemplateError($template->error());
}
else {
    ThrowUserError('unknown_action', {action => $action});
}
