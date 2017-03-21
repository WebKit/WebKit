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
use Bugzilla::Config qw(:admin);
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Group;
use Bugzilla::Product;
use Bugzilla::User;
use Bugzilla::Token;

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};

my $user = Bugzilla->login(LOGIN_REQUIRED);

print $cgi->header();

$user->in_group('creategroups')
  || ThrowUserError("auth_failure", {group  => "creategroups",
                                     action => "edit",
                                     object => "groups"});

my $action = trim($cgi->param('action') || '');
my $token  = $cgi->param('token');

# CheckGroupID checks that a positive integer is given and is
# actually a valid group ID. If all tests are successful, the
# trimmed group ID is returned.

sub CheckGroupID {
    my ($group_id) = @_;
    $group_id = trim($group_id || 0);
    ThrowUserError("group_not_specified") unless $group_id;
    (detaint_natural($group_id)
      && Bugzilla->dbh->selectrow_array("SELECT id FROM groups WHERE id = ?",
                                        undef, $group_id))
      || ThrowUserError("invalid_group_ID");
    return $group_id;
}

# CheckGroupRegexp checks that the regular expression is valid
# (the regular expression being optional, the test is successful
# if none is given, as expected). The trimmed regular expression
# is returned.

sub CheckGroupRegexp {
    my ($regexp) = @_;
    $regexp = trim($regexp || '');
    trick_taint($regexp);
    ThrowUserError("invalid_regexp") unless (eval {qr/$regexp/});
    return $regexp;
}

# A helper for displaying the edit.html.tmpl template.
sub get_current_and_available {
    my ($group, $vars) = @_;

    my @all_groups         = Bugzilla::Group->get_all;
    my @members_current    = @{$group->grant_direct(GROUP_MEMBERSHIP)};
    my @member_of_current  = @{$group->granted_by_direct(GROUP_MEMBERSHIP)};
    my @bless_from_current = @{$group->grant_direct(GROUP_BLESS)};
    my @bless_to_current   = @{$group->granted_by_direct(GROUP_BLESS)};
    my (@visible_from_current, @visible_to_me_current);
    if (Bugzilla->params->{'usevisibilitygroups'}) {
        @visible_from_current  = @{$group->grant_direct(GROUP_VISIBLE)};
        @visible_to_me_current = @{$group->granted_by_direct(GROUP_VISIBLE)};
    }

    # Figure out what groups are not currently a member of this group,
    # and what groups this group is not currently a member of.
    my (@members_available, @member_of_available,
        @bless_from_available, @bless_to_available,
        @visible_from_available, @visible_to_me_available);
    foreach my $group_option (@all_groups) {
        if (Bugzilla->params->{'usevisibilitygroups'}) {
            push(@visible_from_available, $group_option)
                if !grep($_->id == $group_option->id, @visible_from_current);
            push(@visible_to_me_available, $group_option)
                if !grep($_->id == $group_option->id, @visible_to_me_current);
        }

        push(@bless_from_available, $group_option)
            if !grep($_->id == $group_option->id, @bless_from_current);

        # The group itself should never show up in the membership lists,
        # and should show up in only one of the bless lists (otherwise
        # you can try to allow it to bless itself twice, leading to a
        # database unique constraint error).
        next if $group_option->id == $group->id;

        push(@members_available, $group_option)
            if !grep($_->id == $group_option->id, @members_current);
        push(@member_of_available, $group_option)
            if !grep($_->id == $group_option->id, @member_of_current);
        push(@bless_to_available, $group_option)
           if !grep($_->id == $group_option->id, @bless_to_current);
    }

    $vars->{'members_current'}     = \@members_current;
    $vars->{'members_available'}   = \@members_available;
    $vars->{'member_of_current'}   = \@member_of_current;
    $vars->{'member_of_available'} = \@member_of_available;

    $vars->{'bless_from_current'}   = \@bless_from_current;
    $vars->{'bless_from_available'} = \@bless_from_available;
    $vars->{'bless_to_current'}     = \@bless_to_current;
    $vars->{'bless_to_available'}   = \@bless_to_available;

    if (Bugzilla->params->{'usevisibilitygroups'}) {
        $vars->{'visible_from_current'}    = \@visible_from_current;
        $vars->{'visible_from_available'}  = \@visible_from_available;
        $vars->{'visible_to_me_current'}   = \@visible_to_me_current;
        $vars->{'visible_to_me_available'} = \@visible_to_me_available;
    }
}

# If no action is specified, get a list of all groups available.

unless ($action) {
    my @groups = Bugzilla::Group->get_all;
    $vars->{'groups'} = \@groups;

    $template->process("admin/groups/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='changeform' -> present form for altering an existing group
#
# (next action will be 'postchanges')
#

if ($action eq 'changeform') {
    # Check that an existing group ID is given
    my $group_id = CheckGroupID($cgi->param('group'));
    my $group = new Bugzilla::Group($group_id);

    get_current_and_available($group, $vars);
    $vars->{'group'} = $group;
    $vars->{'token'} = issue_session_token('edit_group');

    $template->process("admin/groups/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='add' -> present form for parameters for new group
#
# (next action will be 'new')
#

if ($action eq 'add') {
    $vars->{'token'} = issue_session_token('add_group');

    $template->process("admin/groups/create.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}



#
# action='new' -> add group entered in the 'action=add' screen
#

if ($action eq 'new') {
    check_token_data($token, 'add_group');
    my $group = Bugzilla::Group->create({
        name        => scalar $cgi->param('name'),
        description => scalar $cgi->param('desc'),
        userregexp  => scalar $cgi->param('regexp'),
        isactive    => scalar $cgi->param('isactive'),
        icon_url    => scalar $cgi->param('icon_url'),
        isbuggroup  => 1,
        use_in_all_products => scalar $cgi->param('insertnew'),
    });

    delete_token($token);

    $vars->{'message'} = 'group_created';
    $vars->{'group'} = $group;
    get_current_and_available($group, $vars);
    $vars->{'token'} = issue_session_token('edit_group');

    $template->process("admin/groups/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    # Check that an existing group ID is given
    my $group = Bugzilla::Group->check({ id => scalar $cgi->param('group') });
    $group->check_remove({ test_only => 1 });
    $vars->{'shared_queries'} =
        $dbh->selectrow_array('SELECT COUNT(*)
                                 FROM namedquery_group_map
                                WHERE group_id = ?', undef, $group->id);

    $vars->{'group'} = $group;
    $vars->{'token'} = issue_session_token('delete_group');

    $template->process("admin/groups/delete.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='delete' -> really delete the group
#

if ($action eq 'delete') {
    check_token_data($token, 'delete_group');
    # Check that an existing group ID is given
    my $group = Bugzilla::Group->check({ id => scalar $cgi->param('group') });
    $vars->{'name'} = $group->name;
    $group->remove_from_db({
        remove_from_users => scalar $cgi->param('removeusers'),
        remove_from_bugs  => scalar $cgi->param('removebugs'),
        remove_from_flags => scalar $cgi->param('removeflags'),
        remove_from_products => scalar $cgi->param('unbind'),
    });
    delete_token($token);

    $vars->{'message'} = 'group_deleted';
    $vars->{'groups'} = [Bugzilla::Group->get_all];

    $template->process("admin/groups/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='postchanges' -> update the groups
#

if ($action eq 'postchanges') {
    check_token_data($token, 'edit_group');
    my $changes = doGroupChanges();
    delete_token($token);

    my $group = new Bugzilla::Group($cgi->param('group_id'));
    get_current_and_available($group, $vars);
    $vars->{'message'} = 'group_updated';
    $vars->{'group'}   = $group;
    $vars->{'changes'} = $changes;
    $vars->{'token'} = issue_session_token('edit_group');

    $template->process("admin/groups/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'confirm_remove') {
    my $group = new Bugzilla::Group(CheckGroupID($cgi->param('group_id')));
    $vars->{'group'} = $group;
    $vars->{'regexp'} = CheckGroupRegexp($cgi->param('regexp'));
    $vars->{'token'} = issue_session_token('remove_group_members');

    $template->process('admin/groups/confirm-remove.html.tmpl', $vars)
        || ThrowTemplateError($template->error());
    exit;
}

if ($action eq 'remove_regexp') {
    check_token_data($token, 'remove_group_members');
    # remove all explicit users from the group with
    # gid = $cgi->param('group') that match the regular expression
    # stored in the DB for that group or all of them period

    my $group  = new Bugzilla::Group(CheckGroupID($cgi->param('group_id')));
    my $regexp = CheckGroupRegexp($cgi->param('regexp'));

    $dbh->bz_start_transaction();

    my $users = $group->members_direct();
    my $sth_delete = $dbh->prepare(
        "DELETE FROM user_group_map
           WHERE user_id = ? AND isbless = 0 AND group_id = ?");

    my @deleted;
    foreach my $member (@$users) {
        if ($regexp eq '' || $member->login =~ m/$regexp/i) {
            $sth_delete->execute($member->id, $group->id);
            push(@deleted, $member);
        }
    }
    $dbh->bz_commit_transaction();

    $vars->{'users'}  = \@deleted;
    $vars->{'regexp'} = $regexp;
    delete_token($token);

    $vars->{'message'} = 'group_membership_removed';
    $vars->{'group'} = $group->name;
    $vars->{'groups'} = [Bugzilla::Group->get_all];

    $template->process("admin/groups/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

# No valid action found
ThrowUserError('unknown_action', {action => $action});

# Helper sub to handle the making of changes to a group
sub doGroupChanges {
    my $cgi = Bugzilla->cgi;
    my $dbh = Bugzilla->dbh;

    $dbh->bz_start_transaction();

    # Check that the given group ID is valid and make a Group.
    my $group = new Bugzilla::Group(CheckGroupID($cgi->param('group_id')));

    if (defined $cgi->param('regexp')) {
        $group->set_user_regexp($cgi->param('regexp'));
    }

    if ($group->is_bug_group) {
        if (defined $cgi->param('name')) {
            $group->set_name($cgi->param('name'));
        }
        if (defined $cgi->param('desc')) {
            $group->set_description($cgi->param('desc'));
        }
        # Only set isactive if we came from the right form.
        if (defined $cgi->param('regexp')) {
            $group->set_is_active($cgi->param('isactive'));
        }
    }

    if (defined $cgi->param('icon_url')) {
        $group->set_icon_url($cgi->param('icon_url'));
    }

    my $changes = $group->update();

    my $sth_insert = $dbh->prepare('INSERT INTO group_group_map
                                    (member_id, grantor_id, grant_type)
                                    VALUES (?, ?, ?)');

    my $sth_delete = $dbh->prepare('DELETE FROM group_group_map
                                     WHERE member_id = ?
                                           AND grantor_id = ?
                                           AND grant_type = ?');

    # First item is the type, second is whether or not it's "reverse" 
    # (granted_by) (see _do_add for more explanation).
    my %fields = (
        members       => [GROUP_MEMBERSHIP, 0],
        bless_from    => [GROUP_BLESS, 0],
        visible_from  => [GROUP_VISIBLE, 0],
        member_of     => [GROUP_MEMBERSHIP, 1],
        bless_to      => [GROUP_BLESS, 1],
        visible_to_me => [GROUP_VISIBLE, 1]
    );
    while (my ($field, $data) = each %fields) {
        _do_add($group, $changes, $sth_insert, "${field}_add", 
                $data->[0], $data->[1]);
        _do_remove($group, $changes, $sth_delete, "${field}_remove",
                   $data->[0], $data->[1]);
    }

    $dbh->bz_commit_transaction();
    return $changes;
}

sub _do_add {
    my ($group, $changes, $sth_insert, $field, $type, $reverse) = @_;
    my $cgi = Bugzilla->cgi;

    my $current;
    # $reverse means we're doing a granted_by--that is, somebody else
    # is granting us something.
    if ($reverse) {
        $current = $group->granted_by_direct($type);
    }
    else {
        $current = $group->grant_direct($type);
    }

    my $add_items = Bugzilla::Group->new_from_list([$cgi->param($field)]);

    foreach my $add (@$add_items) {
        next if grep($_->id == $add->id, @$current);

        $changes->{$field} ||= [];
        push(@{$changes->{$field}}, $add->name);
        # They go this direction for a normal "This group is granting
        # $add something."
        my @ids = ($add->id, $group->id);
        # But they get reversed for "This group is being granted something
        # by $add."
        @ids = reverse @ids if $reverse;
        $sth_insert->execute(@ids, $type);
    }
}

sub _do_remove {
    my ($group, $changes, $sth_delete, $field, $type, $reverse) = @_;
    my $cgi = Bugzilla->cgi;
    my $remove_items = Bugzilla::Group->new_from_list([$cgi->param($field)]);

    foreach my $remove (@$remove_items) {
        my @ids = ($remove->id, $group->id);
        # See _do_add for an explanation of $reverse
        @ids = reverse @ids if $reverse;
        # Deletions always succeed and are harmless if they fail, so we
        # don't need to do any checks.
        $sth_delete->execute(@ids, $type);
        $changes->{$field} ||= [];
        push(@{$changes->{$field}}, $remove->name);
    }
}
