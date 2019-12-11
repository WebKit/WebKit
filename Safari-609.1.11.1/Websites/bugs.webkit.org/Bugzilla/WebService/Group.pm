# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Group;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::WebService);
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::WebService::Util qw(validate translate params_to_objects);

use constant PUBLIC_METHODS => qw(
    create
    get
    update
);

use constant MAPPED_RETURNS => {
    userregexp => 'user_regexp',
    isactive => 'is_active'
};

sub create {
    my ($self, $params) = @_;

    Bugzilla->login(LOGIN_REQUIRED);
    Bugzilla->user->in_group('creategroups') 
        || ThrowUserError("auth_failure", { group  => "creategroups",
                                            action => "add",
                                            object => "group"});
    # Create group
    my $group = Bugzilla::Group->create({
        name               => $params->{name},
        description        => $params->{description},
        userregexp         => $params->{user_regexp},
        isactive           => $params->{is_active},
        isbuggroup         => 1,
        icon_url           => $params->{icon_url}
    });
    return { id => $self->type('int', $group->id) };
}

sub update {
    my ($self, $params) = @_;

    my $dbh = Bugzilla->dbh;

    Bugzilla->login(LOGIN_REQUIRED);
    Bugzilla->user->in_group('creategroups')
        || ThrowUserError("auth_failure", { group  => "creategroups",
                                            action => "edit",
                                            object => "group" });

    defined($params->{names}) || defined($params->{ids})
        || ThrowCodeError('params_required',
               { function => 'Group.update', params => ['ids', 'names'] });

    my $group_objects = params_to_objects($params, 'Bugzilla::Group');

    my %values = %$params;
    
    # We delete names and ids to keep only new values to set.
    delete $values{names};
    delete $values{ids};

    $dbh->bz_start_transaction();
    foreach my $group (@$group_objects) {
        $group->set_all(\%values);
    }

    my %changes;
    foreach my $group (@$group_objects) {
        my $returned_changes = $group->update();
        $changes{$group->id} = translate($returned_changes, MAPPED_RETURNS);
    }
    $dbh->bz_commit_transaction();

    my @result;
    foreach my $group (@$group_objects) {
        my %hash = (
            id      => $group->id,
            changes => {},
        );
        foreach my $field (keys %{ $changes{$group->id} }) {
            my $change = $changes{$group->id}->{$field};
            $hash{changes}{$field} = {
                removed => $self->type('string', $change->[0]),
                added   => $self->type('string', $change->[1]) 
            };
        }
       push(@result, \%hash);
    }

    return { groups => \@result };
}

sub get {
    my ($self, $params) = validate(@_, 'ids', 'names', 'type');

    Bugzilla->login(LOGIN_REQUIRED);

    # Reject access if there is no sense in continuing.
    my $user = Bugzilla->user;
    my $all_groups = $user->in_group('editusers') || $user->in_group('creategroups');
    if (!$all_groups && !$user->can_bless) {
        ThrowUserError('group_cannot_view');
    }

    Bugzilla->switch_to_shadow_db();

    my $groups = [];

    if (defined $params->{ids}) {
        # Get the groups by id
        $groups = Bugzilla::Group->new_from_list($params->{ids});
    }

    if (defined $params->{names}) {
        # Get the groups by name. Check will throw an error if a bad name is given
        foreach my $name (@{$params->{names}}) {
            # Skip if we got this from params->{id}
            next if grep { $_->name eq $name } @$groups;

            push @$groups, Bugzilla::Group->check({ name => $name });
        }
    }

    if (!defined $params->{ids} && !defined $params->{names}) {
        if ($all_groups) {
            @$groups = Bugzilla::Group->get_all;
        }
        else {
            # Get only groups the user has bless groups too
            $groups = $user->bless_groups;
        }
    }

    # Now create a result entry for each.
    my @groups = map { $self->_group_to_hash($params, $_) } @$groups;
    return { groups => \@groups };
}

sub _group_to_hash {
    my ($self, $params, $group) = @_;
    my $user = Bugzilla->user;

    my $field_data = {
        id          => $self->type('int', $group->id),
        name        => $self->type('string', $group->name),
        description => $self->type('string', $group->description),
    };

    if ($user->in_group('creategroups')) {
        $field_data->{is_active}    = $self->type('boolean', $group->is_active);
        $field_data->{is_bug_group} = $self->type('boolean', $group->is_bug_group);
        $field_data->{user_regexp}  = $self->type('string', $group->user_regexp);
    }

    if ($params->{membership}) {
        $field_data->{membership} = $self->_get_group_membership($group, $params);
    }
    return $field_data;
}

sub _get_group_membership {
    my ($self, $group, $params) = @_;
    my $user = Bugzilla->user;

    my %users_only;
    my $dbh = Bugzilla->dbh;
    my $editusers = $user->in_group('editusers');

    my $query = 'SELECT userid FROM profiles';
    my $visibleGroups;

    if (!$editusers && Bugzilla->params->{'usevisibilitygroups'}) {
        # Show only users in visible groups.
        $visibleGroups = $user->visible_groups_inherited;

        if (scalar @$visibleGroups) {
            $query .= qq{, user_group_map AS ugm
                         WHERE ugm.user_id = profiles.userid
                           AND ugm.isbless = 0
                           AND } . $dbh->sql_in('ugm.group_id', $visibleGroups);
        }
    } elsif ($editusers || $user->can_bless($group->id) || $user->in_group('creategroups')) {
        $visibleGroups = 1;
        $query .= qq{, user_group_map AS ugm
                     WHERE ugm.user_id = profiles.userid
                       AND ugm.isbless = 0
                    };
    }
    if (!$visibleGroups) {
        ThrowUserError('group_not_visible', { group => $group });
    }

    my $grouplist = Bugzilla::Group->flatten_group_membership($group->id);
    $query .= ' AND ' . $dbh->sql_in('ugm.group_id', $grouplist);

    my $userids = $dbh->selectcol_arrayref($query);
    my $user_objects = Bugzilla::User->new_from_list($userids);
    my @users =
        map {{
            id                => $self->type('int', $_->id),
            real_name         => $self->type('string', $_->name),
            name              => $self->type('string', $_->login),
            email             => $self->type('string', $_->email),
            can_login         => $self->type('boolean', $_->is_enabled),
            email_enabled     => $self->type('boolean', $_->email_enabled),
            login_denied_text => $self->type('string', $_->disabledtext),
        }} @$user_objects;

    return \@users;
}

1;

__END__

=head1 NAME

Bugzilla::Webservice::Group - The API for creating, changing, and getting
information about Groups.

=head1 DESCRIPTION

This part of the Bugzilla API allows you to create Groups and
get information about them.

=head1 METHODS

See L<Bugzilla::WebService> for a description of how parameters are passed,
and what B<STABLE>, B<UNSTABLE>, and B<EXPERIMENTAL> mean.

Although the data input and output is the same for JSONRPC, XMLRPC and REST,
the directions for how to access the data via REST is noted in each method
where applicable.

=head1 Group Creation and Modification

=head2 create

B<UNSTABLE>

=over

=item B<Description>

This allows you to create a new group in Bugzilla.

=item B<REST>

POST /rest/group

The params to include in the POST body as well as the returned data format,
are the same as below.

=item B<Params>

Some params must be set, or an error will be thrown. These params are
marked B<Required>.

=over

=item C<name>

B<Required> C<string> A short name for this group. Must be unique. This
is not usually displayed in the user interface, except in a few places.

=item C<description>

B<Required> C<string> A human-readable name for this group. Should be
relatively short. This is what will normally appear in the UI as the
name of the group.

=item C<user_regexp>

C<string> A regular expression. Any user whose Bugzilla username matches
this regular expression will automatically be granted membership in this group.

=item C<is_active>

C<boolean> C<True> if new group can be used for bugs, C<False> if this
is a group that will only contain users and no bugs will be restricted
to it.

=item C<icon_url>

C<string> A URL pointing to a small icon used to identify the group.
This icon will show up next to users' names in various parts of Bugzilla
if they are in this group.

=back

=item B<Returns>

A hash with one element, C<id>. This is the id of the newly-created group.

=item B<Errors>

=over

=item 800 (Empty Group Name)

You must specify a value for the C<name> field.

=item 801 (Group Exists)

There is already another group with the same C<name>.

=item 802 (Group Missing Description)

You must specify a value for the C<description> field.

=item 803 (Group Regexp Invalid)

You specified an invalid regular expression in the C<user_regexp> field.

=back

=item B<History>

=over

=item REST API call added in Bugzilla B<5.0>.

=back

=back

=head2 update

B<UNSTABLE>

=over

=item B<Description>

This allows you to update a group in Bugzilla.

=item B<REST>

PUT /rest/group/<group_name_or_id>

The params to include in the PUT body as well as the returned data format,
are the same as below. The C<ids> param will be overridden as it is pulled
from the URL path.

=item B<Params>

At least C<ids> or C<names> must be set, or an error will be thrown.

=over

=item C<ids>

B<Required> C<array> Contain ids of groups to update.

=item C<names>

B<Required> C<array> Contain names of groups to update.

=item C<name>

C<string> A new name for group.

=item C<description>

C<string> A new description for groups. This is what will appear in the UI
as the name of the groups.

=item C<user_regexp>

C<string> A new regular expression for email. Will automatically grant
membership to these groups to anyone with an email address that matches
this perl regular expression.

=item C<is_active>

C<boolean> Set if groups are active and eligible to be used for bugs.
True if bugs can be restricted to this group, false otherwise.

=item C<icon_url>

C<string> A URL pointing to an icon that will appear next to the name of
users who are in this group.

=back

=item B<Returns>

A C<hash> with a single field "groups". This points to an array of hashes
with the following fields:

=over

=item C<id>

C<int> The id of the group that was updated.

=item C<changes>

C<hash> The changes that were actually done on this group. The keys are
the names of the fields that were changed, and the values are a hash
with two keys:

=over

=item C<added>

C<string> The values that were added to this field,
possibly a comma-and-space-separated list if multiple values were added.

=item C<removed>

C<string> The values that were removed from this field, possibly a
comma-and-space-separated list if multiple values were removed.

=back

=back

=item B<Errors>

The same as L</create>.

=item B<History>

=over

=item REST API call added in Bugzilla B<5.0>.

=back

=back

=head1 Group Information

=head2 get

B<UNSTABLE>

=over

=item B<Description>

Returns information about L<Bugzilla::Group|Groups>.

=item B<REST>

To return information about a specific group by C<id> or C<name>:

GET /rest/group/<group_id_or_name>

You can also return information about more than one specific group
by using the following in your query string:

GET /rest/group?ids=1&ids=2&ids=3 or GET /group?names=ProductOne&names=Product2

the returned data format is same as below.

=item B<Params>

If neither ids or names is passed, and you are in the creategroups or
editusers group, then all groups will be retrieved. Otherwise, only groups
that you have bless privileges for will be returned.

=over

=item C<ids>

C<array> Contain ids of groups to update.

=item C<names>

C<array> Contain names of groups to update.

=item C<membership>

C<boolean> Set to 1 then a list of members of the passed groups' names and
ids will be returned.

=back

=item B<Returns>

If the user is a member of the "creategroups" group they will receive
information about all groups or groups matching the criteria that they passed.
You have to be in the creategroups group unless you're requesting membership
information.

If the user is not a member of the "creategroups" group, but they are in the
"editusers" group or have bless privileges to the groups they require
membership information for, the is_active, is_bug_group and user_regexp values
are not supplied.

The return value will be a hash containing group names as the keys, each group
name will point to a hash that describes the group and has the following items:

=over

=item id

C<int> The unique integer ID that Bugzilla uses to identify this group.
Even if the name of the group changes, this ID will stay the same.

=item name

C<string> The name of the group.

=item description

C<string> The description of the group.

=item is_bug_group

C<int> Whether this groups is to be used for bug reports or is only administrative specific.

=item user_regexp

C<string> A regular expression that allows users to be added to this group if their login matches.

=item is_active

C<int> Whether this group is currently active or not.

=item users

C<array> An array of hashes, each hash contains a user object for one of the
members of this group, only returned if the user sets the C<membership>
parameter to 1, the user hash has the following items:

=over

=item id

C<int> The id of the user.

=item real_name

C<string> The actual name of the user.

=item email

C<string> The email address of the user.

=item name

C<string> The login name of the user. Note that in some situations this is
different than their email.

=item can_login

C<boolean> A boolean value to indicate if the user can login into bugzilla.

=item email_enabled

C<boolean> A boolean value to indicate if bug-related mail will be sent
to the user or not.

=item disabled_text

C<string> A text field that holds the reason for disabling a user from logging
into bugzilla, if empty then the user account is enabled otherwise it is
disabled/closed.

=back

=back

=item B<Errors>

=over

=item 51 (Invalid Object)

A non existing group name was passed to the function, as a result no
group object existed for that invalid name.

=item 805 (Cannot view groups)

Logged-in users are not authorized to edit bugzilla groups as they are not
members of the creategroups group in bugzilla, or they are not authorized to
access group member's information as they are not members of the "editusers"
group or can bless the group.

=back

=item B<History>

=over

=item This function was added in Bugzilla B<5.0>.

=back

=back

=cut
