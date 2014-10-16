#!/usr/bin/env perl -wT
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
# The Original Code is the Bugzilla Bug Tracking System.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Frédéric Buclin <LpSolit@gmail.com>

# This is a script to edit the values of fields that have drop-down
# or select boxes. It is largely a copy of editmilestones.cgi, but 
# with some cleanup.

use strict;
use lib qw(. lib);

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Constants;
use Bugzilla::Token;
use Bugzilla::Field;
use Bugzilla::Field::Choice;

###############
# Subroutines #
###############

sub display_field_values {
    my $vars = shift;
    my $template = Bugzilla->template;
    $vars->{'values'} = $vars->{'field'}->legal_values;
    $template->process("admin/fieldvalues/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

######################################################################
# Main Body Execution
######################################################################

# require the user to have logged in
Bugzilla->login(LOGIN_REQUIRED);

my $dbh      = Bugzilla->dbh;
my $cgi      = Bugzilla->cgi;
my $template = Bugzilla->template;
my $vars = {};

# Replace this entry by separate entries in templates when
# the documentation about legal values becomes bigger.
$vars->{'doc_section'} = 'edit-values.html';

print $cgi->header();

Bugzilla->user->in_group('admin') ||
    ThrowUserError('auth_failure', {group  => "admin",
                                    action => "edit",
                                    object => "field_values"});

#
# often-used variables
#
my $action = trim($cgi->param('action')  || '');
my $token  = $cgi->param('token');

#
# field = '' -> Show nice list of fields
#
if (!$cgi->param('field')) {
    my @field_list =
        @{ Bugzilla->fields({ is_select => 1, is_abnormal => 0 }) };

    $vars->{'fields'} = \@field_list;
    $template->process("admin/fieldvalues/select-field.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

# At this point, the field must be defined.
my $field = Bugzilla::Field->check($cgi->param('field'));
if (!$field->is_select || $field->is_abnormal) {
    ThrowUserError('fieldname_invalid', { field => $field });
}
$vars->{'field'} = $field;

#
# action='' -> Show nice list of values.
#
display_field_values($vars) unless $action;

#
# action='add' -> show form for adding new field value.
# (next action will be 'new')
#
if ($action eq 'add') {
    $vars->{'token'} = issue_session_token('add_field_value');
    $template->process("admin/fieldvalues/create.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='new' -> add field value entered in the 'action=add' screen
#
if ($action eq 'new') {
    check_token_data($token, 'add_field_value');

    my $created_value = Bugzilla::Field::Choice->type($field)->create({
        value   => scalar $cgi->param('value'), 
        sortkey => scalar $cgi->param('sortkey'),
        is_open => scalar $cgi->param('is_open'),
        visibility_value_id => scalar $cgi->param('visibility_value_id'),
    });

    delete_token($token);

    $vars->{'message'} = 'field_value_created';
    $vars->{'value'} = $created_value;
    display_field_values($vars);
}

# After this, we always have a value
my $value = Bugzilla::Field::Choice->type($field)->check($cgi->param('value'));
$vars->{'value'} = $value;

#
# action='del' -> ask if user really wants to delete
# (next action would be 'delete')
#
if ($action eq 'del') {
    # If the value cannot be deleted, throw an error.
    if ($value->is_static) {
        ThrowUserError('fieldvalue_not_deletable', $vars);
    }
    $vars->{'token'} = issue_session_token('delete_field_value');

    $template->process("admin/fieldvalues/confirm-delete.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}


#
# action='delete' -> really delete the field value
#
if ($action eq 'delete') {
    check_token_data($token, 'delete_field_value');
    $value->remove_from_db();
    delete_token($token);
    $vars->{'message'} = 'field_value_deleted';
    $vars->{'no_edit_link'} = 1;
    display_field_values($vars);
}


#
# action='edit' -> present the edit-value form
# (next action would be 'update')
#
if ($action eq 'edit') {
    $vars->{'token'} = issue_session_token('edit_field_value');
    $template->process("admin/fieldvalues/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}


#
# action='update' -> update the field value
#
if ($action eq 'update') {
    check_token_data($token, 'edit_field_value');
    $vars->{'value_old'} = $value->name;
    if ($cgi->should_set('is_active')) {
        $value->set_is_active($cgi->param('is_active'));
    }
    $value->set_name($cgi->param('value_new'));
    $value->set_sortkey($cgi->param('sortkey'));
    $value->set_visibility_value($cgi->param('visibility_value_id'));
    $vars->{'changes'} = $value->update();
    delete_token($token);
    $vars->{'message'} = 'field_value_updated';
    display_field_values($vars);
}

# No valid action found
ThrowUserError('unknown_action', {action => $action});
