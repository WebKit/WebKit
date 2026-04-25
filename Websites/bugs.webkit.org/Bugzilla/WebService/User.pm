# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::User;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::WebService);

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Group;
use Bugzilla::User;
use Bugzilla::Util qw(trim detaint_natural);
use Bugzilla::WebService::Util qw(filter filter_wants validate translate params_to_objects);

use List::Util qw(first min);

# Don't need auth to login
use constant LOGIN_EXEMPT => {
    login => 1,
    offer_account_by_email => 1,
};

use constant READ_ONLY => qw(
    get
);

use constant PUBLIC_METHODS => qw(
    create
    get
    login
    logout
    offer_account_by_email
    update
    valid_login
);

use constant MAPPED_FIELDS => {
    email => 'login',
    full_name => 'name',
    login_denied_text => 'disabledtext',
};

use constant MAPPED_RETURNS => {
    login_name => 'email',
    realname => 'full_name',
    disabledtext => 'login_denied_text',
};

##############
# User Login #
##############

sub login {
    my ($self, $params) = @_;

    # Check to see if we are already logged in
    my $user = Bugzilla->user;
    if ($user->id) {
        return $self->_login_to_hash($user);
    }

    # Username and password params are required 
    foreach my $param ("login", "password") {
        (defined $params->{$param} || defined $params->{'Bugzilla_' . $param})
            || ThrowCodeError('param_required', { param => $param });
    }

    $user = Bugzilla->login();
    return $self->_login_to_hash($user);
}

sub logout {
    my $self = shift;
    Bugzilla->logout;
}

sub valid_login {
    my ($self, $params) = @_;
    defined $params->{login}
        || ThrowCodeError('param_required', { param => 'login' });
    Bugzilla->login();
    if (Bugzilla->user->id && Bugzilla->user->login eq $params->{login}) {
        return $self->type('boolean', 1);
    }
    return $self->type('boolean', 0);
}

#################
# User Creation #
#################

sub offer_account_by_email {
    my $self = shift;
    my ($params) = @_;
    my $email = trim($params->{email})
        || ThrowCodeError('param_required', { param => 'email' });

    Bugzilla->user->check_account_creation_enabled;
    Bugzilla->user->check_and_send_account_creation_confirmation($email);
    return undef;
}

sub create {
    my $self = shift;
    my ($params) = @_;

    Bugzilla->user->in_group('editusers') 
        || ThrowUserError("auth_failure", { group  => "editusers",
                                            action => "add",
                                            object => "users"});

    my $email = trim($params->{email})
        || ThrowCodeError('param_required', { param => 'email' });
    my $realname = trim($params->{full_name});
    my $password = trim($params->{password}) || '*';

    my $user = Bugzilla::User->create({
        login_name    => $email,
        realname      => $realname,
        cryptpassword => $password
    });

    return { id => $self->type('int', $user->id) };
}


# function to return user information by passing either user ids or 
# login names or both together:
# $call = $rpc->call( 'User.get', { ids => [1,2,3], 
#         names => ['testusera@redhat.com', 'testuserb@redhat.com'] });
sub get {
    my ($self, $params) = validate(@_, 'names', 'ids', 'match', 'group_ids', 'groups');

    Bugzilla->switch_to_shadow_db();

    defined($params->{names}) || defined($params->{ids})
        || defined($params->{match})
        || ThrowCodeError('params_required', 
               { function => 'User.get', params => ['ids', 'names', 'match'] });

    my @user_objects;
    @user_objects = map { Bugzilla::User->check($_) } @{ $params->{names} }
                    if $params->{names};

    # start filtering to remove duplicate user ids
    my %unique_users = map { $_->id => $_ } @user_objects;
    @user_objects = values %unique_users;
      
    my @users;

    # If the user is not logged in: Return an error if they passed any user ids.
    # Otherwise, return a limited amount of information based on login names.
    if (!Bugzilla->user->id){
        if ($params->{ids}){
            ThrowUserError("user_access_by_id_denied");
        }
        if ($params->{match}) {
            ThrowUserError('user_access_by_match_denied');
        }
        my $in_group = $self->_filter_users_by_group(
            \@user_objects, $params);
        @users = map { filter $params, {
                     id        => $self->type('int', $_->id),
                     real_name => $self->type('string', $_->name),
                     name      => $self->type('email', $_->login),
                 } } @$in_group;

        return { users => \@users };
    }

    my $obj_by_ids;
    $obj_by_ids = Bugzilla::User->new_from_list($params->{ids}) if $params->{ids};

    # obj_by_ids are only visible to the user if they can see
    # the otheruser, for non visible otheruser throw an error
    foreach my $obj (@$obj_by_ids) {
        if (Bugzilla->user->can_see_user($obj)){
            if (!$unique_users{$obj->id}) {
                push (@user_objects, $obj);
                $unique_users{$obj->id} = $obj;
            }
        }
        else {
            ThrowUserError('auth_failure', {reason => "not_visible",
                                            action => "access",
                                            object => "user",
                                            userid => $obj->id});
        }
    }

    # User Matching
    my $limit = Bugzilla->params->{maxusermatches};
    if ($params->{limit}) {
        detaint_natural($params->{limit})
            || ThrowCodeError('param_must_be_numeric',
                              { function => 'Bugzilla::WebService::User::match',
                                param    => 'limit' });
        $limit = $limit ? min($params->{limit}, $limit) : $params->{limit};
    }

    my $exclude_disabled = $params->{'include_disabled'} ? 0 : 1;
    foreach my $match_string (@{ $params->{'match'} || [] }) {
        my $matched = Bugzilla::User::match($match_string, $limit, $exclude_disabled);
        foreach my $user (@$matched) {
            if (!$unique_users{$user->id}) {
                push(@user_objects, $user);
                $unique_users{$user->id} = $user;
            }
        }
    }

    my $in_group = $self->_filter_users_by_group(\@user_objects, $params);
    foreach my $user (@$in_group) {
        my $user_info = filter $params, {
            id        => $self->type('int', $user->id),
            real_name => $self->type('string', $user->name),
            name      => $self->type('email', $user->login),
            email     => $self->type('email', $user->email),
            can_login => $self->type('boolean', $user->is_enabled ? 1 : 0),
        };

        if (Bugzilla->user->in_group('editusers')) {
            $user_info->{email_enabled}     = $self->type('boolean', $user->email_enabled);
            $user_info->{login_denied_text} = $self->type('string', $user->disabledtext);
        }

        if (Bugzilla->user->id == $user->id) {
            if (filter_wants($params, 'saved_searches')) {
                $user_info->{saved_searches} = [
                    map { $self->_query_to_hash($_) } @{ $user->queries }
                ];
            }
            if (filter_wants($params, 'saved_reports')) {
                $user_info->{saved_reports}  = [
                    map { $self->_report_to_hash($_) } @{ $user->reports }
                ];
            }
        }

        if (filter_wants($params, 'groups')) {
            if (Bugzilla->user->id == $user->id || Bugzilla->user->in_group('editusers')) {
                $user_info->{groups} = [
                    map { $self->_group_to_hash($_) } @{ $user->groups }
                ];
            }
            else {
                $user_info->{groups} = $self->_filter_bless_groups($user->groups);
            }
        }

        push(@users, $user_info);
    }

    return { users => \@users };
}

###############
# User Update #
###############

sub update {
    my ($self, $params) = @_;

    my $dbh = Bugzilla->dbh;

    my $user = Bugzilla->login(LOGIN_REQUIRED);

    # Reject access if there is no sense in continuing.
    $user->in_group('editusers')
        || ThrowUserError("auth_failure", {group  => "editusers",
                                           action => "edit",
                                           object => "users"});

    defined($params->{names}) || defined($params->{ids})
        || ThrowCodeError('params_required', 
               { function => 'User.update', params => ['ids', 'names'] });

    my $user_objects = params_to_objects($params, 'Bugzilla::User');

    my $values = translate($params, MAPPED_FIELDS);

    # We delete names and ids to keep only new values to set.
    delete $values->{names};
    delete $values->{ids};

    $dbh->bz_start_transaction();
    foreach my $user (@$user_objects){
        $user->set_all($values);
    }

    my %changes;
    foreach my $user (@$user_objects){
        my $returned_changes = $user->update();
        $changes{$user->id} = translate($returned_changes, MAPPED_RETURNS);    
    }
    $dbh->bz_commit_transaction();

    my @result;
    foreach my $user (@$user_objects) {
        my %hash = (
            id      => $user->id,
            changes => {},
        );

        foreach my $field (keys %{ $changes{$user->id} }) {
            my $change = $changes{$user->id}->{$field};
            # We normalize undef to an empty string, so that the API
            # stays consistent for things that can become empty.
            $change->[0] = '' if !defined $change->[0];
            $change->[1] = '' if !defined $change->[1];
            # We also flatten arrays (used by groups and blessed_groups)
            $change->[0] = join(',', @{$change->[0]}) if ref $change->[0];
            $change->[1] = join(',', @{$change->[1]}) if ref $change->[1];

            $hash{changes}{$field} = {
                removed => $self->type('string', $change->[0]),
                added   => $self->type('string', $change->[1]) 
            };
        }

        push(@result, \%hash);
    }

    return { users => \@result };
}

sub _filter_users_by_group {
    my ($self, $users, $params) = @_;
    my ($group_ids, $group_names) = @$params{qw(group_ids groups)};

    # If no groups are specified, we return all users.
    return $users if (!$group_ids and !$group_names);

    my $user = Bugzilla->user;
    my (@groups, %groups);

    if ($group_ids) {
        @groups = map { Bugzilla::Group->check({ id => $_ }) } @$group_ids;
        $groups{$_->id} = $_ foreach @groups;
    }
    if ($group_names) {
        foreach my $name (@$group_names) {
            my $group = Bugzilla::Group->check({ name => $name, _error => 'invalid_group_name' });
            $user->in_group($group) || ThrowUserError('invalid_group_name', { name => $name });
            $groups{$group->id} = $group;
        }
    }
    @groups = values %groups;

    my @in_group = grep { $self->_user_in_any_group($_, \@groups) } @$users;
    return \@in_group;
}

sub _user_in_any_group {
    my ($self, $user, $groups) = @_;
    foreach my $group (@$groups) {
        return 1 if $user->in_group($group);
    }
    return 0;
}

sub _filter_bless_groups {
    my ($self, $groups) = @_;
    my $user = Bugzilla->user;

    my @filtered_groups;
    foreach my $group (@$groups) {
        next unless $user->can_bless($group->id);
        push(@filtered_groups, $self->_group_to_hash($group));
    }

    return \@filtered_groups;
}

sub _group_to_hash {
    my ($self, $group) = @_;
    my $item = {
        id          => $self->type('int', $group->id), 
        name        => $self->type('string', $group->name), 
        description => $self->type('string', $group->description), 
    };
    return $item;
}

sub _query_to_hash {
    my ($self, $query) = @_;
    my $item = {
        id    => $self->type('int', $query->id),
        name  => $self->type('string', $query->name),
        query => $self->type('string', $query->url),
    };
    return $item;
}

sub _report_to_hash {
    my ($self, $report) = @_;
    my $item = {
        id    => $self->type('int', $report->id),
        name  => $self->type('string', $report->name),
        query => $self->type('string', $report->query),
    };
    return $item;
}

sub _login_to_hash {
    my ($self, $user) = @_;
    my $item = { id => $self->type('int', $user->id) };
    if ($user->{_login_token}) {
        $item->{'token'} = $user->id . "-" . $user->{_login_token};
    }
    return $item;
}

1;

__END__

=head1 NAME

Bugzilla::Webservice::User - The User Account and Login API

=head1 DESCRIPTION

This part of the Bugzilla API allows you to create User Accounts and
log in/out using an existing account.

=head1 METHODS

See L<Bugzilla::WebService> for a description of how parameters are passed,
and what B<STABLE>, B<UNSTABLE>, and B<EXPERIMENTAL> mean.

Although the data input and output is the same for JSONRPC, XMLRPC and REST,
the directions for how to access the data via REST is noted in each method
where applicable.

=head1 Logging In and Out

These method are now deprecated, and will be removed in the release after
Bugzilla 5.0. The correct way of use these REST and RPC calls is noted in
L<Bugzilla::WebService>

=head2 login

B<DEPRECATED>

=over

=item B<Description>

Logging in, with a username and password, is required for many
Bugzilla installations, in order to search for bugs, post new bugs,
etc. This method logs in an user.

=item B<Params>

=over

=item C<login> (string) - The user's login name.

=item C<password> (string) - The user's password.

=item C<restrict_login> (bool) B<Optional> - If set to a true value,
the token returned by this method will only be valid from the IP address
which called this method.

=back

=item B<Returns>

On success, a hash containing two items, C<id>, the numeric id of the
user that was logged in, and a C<token> which can be passed in
the parameters as authentication in other calls. The token can be sent
along with any future requests to the webservice, for the duration of the
session, i.e. till L<User.logout|/logout> is called.

=item B<Errors>

=over

=item 300 (Invalid Username or Password)

The username does not exist, or the password is wrong.

=item 301 (Login Disabled)

The ability to login with this account has been disabled.  A reason may be
specified with the error.

=item 305 (New Password Required)

The current password is correct, but the user is asked to change
their password.

=item 50 (Param Required)

A login or password parameter was not provided.

=back

=item B<History>

=over

=item C<remember> was removed in Bugzilla B<5.0> as this method no longer
creates a login cookie.

=item C<restrict_login> was added in Bugzilla B<5.0>.

=item C<token> was added in Bugzilla B<4.4.3>.

=item This function will be removed in the release after Bugzilla 5.0, in favour of API keys.

=back

=back

=head2 logout

B<DEPRECATED>

=over

=item B<Description>

Log out the user. Does nothing if there is no user logged in.

=item B<Params> (none)

=item B<Returns> (nothing)

=item B<Errors> (none)

=back

=head2 valid_login

B<DEPRECATED>

=over

=item B<Description>

This method will verify whether a client's cookies or current login
token is still valid or have expired. A valid username must be provided
as well that matches.

=item B<Params>

=over

=item C<login>

The login name that matches the provided cookies or token.

=item C<token>

(string) Persistent login token current being used for authentication (optional).
Cookies passed by client will be used before the token if both provided.

=back

=item B<Returns>

Returns true/false depending on if the current cookies or token are valid
for the provided username.

=item B<Errors> (none)

=item B<History>

=over

=item Added in Bugzilla B<5.0>.

=item This function will be removed in the release after Bugzilla 5.0, in favour of API keys.

=back

=back

=head1 Account Creation and Modification

=head2 offer_account_by_email

B<STABLE>

=over

=item B<Description>

Sends an email to the user, offering to create an account.  The user
will have to click on a URL in the email, and choose their password
and real name.

This is the recommended way to create a Bugzilla account.

=item B<Param>

=over

=item C<email> (string) - the email to send the offer to.

=back

=item B<Returns> (nothing)

=item B<Errors>

=over

=item 500 (Account Already Exists)

An account with that email address already exists in Bugzilla.

=item 501 (Illegal Email Address)

This Bugzilla does not allow you to create accounts with the format of
email address you specified. Account creation may be entirely disabled.

=back

=back

=head2 create

B<STABLE>

=over

=item B<Description>

Creates a user account directly in Bugzilla, password and all.
Instead of this, you should use L</offer_account_by_email> when
possible, because that makes sure that the email address specified can
actually receive an email. This function does not check that.

You must be logged in and have the C<editusers> privilege in order to
call this function.

=item B<REST>

POST /rest/user

The params to include in the POST body as well as the returned data format,
are the same as below.

=item B<Params>

=over

=item C<email> (string) - The email address for the new user.

=item C<full_name> (string) B<Optional> - The user's full name. Will
be set to empty if not specified.

=item C<password> (string) B<Optional> - The password for the new user
account, in plain text.  It will be stripped of leading and trailing
whitespace.  If blank or not specified, the newly created account will
exist in Bugzilla, but will not be allowed to log in using DB
authentication until a password is set either by the user (through
resetting their password) or by the administrator.

=back

=item B<Returns>

A hash containing one item, C<id>, the numeric id of the user that was
created.

=item B<Errors>

The same as L</offer_account_by_email>. If a password is specified,
the function may also throw:

=over

=item 502 (Password Too Short)

The password specified is too short. (Usually, this means the
password is under three characters.)

=back

=item B<History>

=over

=item Error 503 (Password Too Long) removed in Bugzilla B<3.6>.

=item REST API call added in Bugzilla B<5.0>.

=back

=back

=head2 update 

B<EXPERIMENTAL>

=over

=item B<Description>

Updates user accounts in Bugzilla.

=item B<REST>

PUT /rest/user/<user_id_or_name>

The params to include in the PUT body as well as the returned data format,
are the same as below. The C<ids> and C<names> params are overridden as they
are pulled from the URL path.

=item B<Params>

=over

=item C<ids>

C<array> Contains ids of user to update.

=item C<names>

C<array> Contains email/login of user to update.

=item C<full_name>

C<string> The new name of the user.

=item C<email>

C<string> The email of the user. Note that email used to login to bugzilla.
Also note that you can only update one user at a time when changing the 
login name / email. (An error will be thrown if you try to update this field 
for multiple users at once.)

=item C<password>

C<string> The password of the user.

=item C<email_enabled>

C<boolean> A boolean value to enable/disable sending bug-related mail to the user.

=item C<login_denied_text>

C<string> A text field that holds the reason for disabling a user from logging
into bugzilla, if empty then the user account is enabled otherwise it is
disabled/closed.

=item C<groups>

C<hash> These specify the groups that this user is directly a member of.
To set these, you should pass a hash as the value. The hash may contain
the following fields:

=over

=item C<add> An array of C<int>s or C<string>s. The group ids or group names
that the user should be added to.

=item C<remove> An array of C<int>s or C<string>s. The group ids or group names
that the user should be removed from.

=item C<set> An array of C<int>s or C<string>s. An exact set of group ids
and group names that the user should be a member of. NOTE: This does not
remove groups from the user where the person making the change does not
have the bless privilege for.

If you specify C<set>, then C<add> and C<remove> will be ignored. A group in
both the C<add> and C<remove> list will be added. Specifying a group that the
user making the change does not have bless rights will generate an error.

=back

=item C<bless_groups>

C<hash> - This is the same as groups, but affects what groups a user
has direct membership to bless that group. It takes the same inputs as
groups.

=back

=item B<Returns>

A C<hash> with a single field "users". This points to an array of hashes
with the following fields:

=over

=item C<id>

C<int> The id of the user that was updated.

=item C<changes>

C<hash> The changes that were actually done on this user. The keys are
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

=over

=item 51 (Bad Login Name)

You passed an invalid login name in the "names" array.

=item 304 (Authorization Required)

Logged-in users are not authorized to edit other users.

=back

=item B<History>

=over

=item REST API call added in Bugzilla B<5.0>.

=back

=back

=head1 User Info

=head2 get

B<STABLE>

=over

=item B<Description>

Gets information about user accounts in Bugzilla.

=item B<REST>

To get information about a single user:

GET /rest/user/<user_id_or_name>

To search for users by name, group using URL params same as below:

GET /rest/user

The returned data format is the same as below.

=item B<Params>

B<Note>: At least one of C<ids>, C<names>, or C<match> must be specified.

B<Note>: Users will not be returned more than once, so even if a user 
is matched by more than one argument, only one user will be returned.

In addition to the parameters below, this method also accepts the
standard L<include_fields|Bugzilla::WebService/include_fields> and
L<exclude_fields|Bugzilla::WebService/exclude_fields> arguments.

=over

=item C<ids> (array) 

An array of integers, representing user ids.

Logged-out users cannot pass this parameter to this function. If they try,
they will get an error. Logged-in users will get an error if they specify
the id of a user they cannot see.

=item C<names> (array)

An array of login names (strings).

=item C<match> (array)

An array of strings. This works just like "user matching" in
Bugzilla itself. Users will be returned whose real name or login name
contains any one of the specified strings. Users that you cannot see will
not be included in the returned list.

Most installations have a limit on how many matches are returned for
each string, which defaults to 1000 but can be changed by the Bugzilla
administrator.

Logged-out users cannot use this argument, and an error will be thrown
if they try. (This is to make it harder for spammers to harvest email
addresses from Bugzilla, and also to enforce the user visibility
restrictions that are implemented on some Bugzillas.)

=item C<limit> (int)

Limit the number of users matched by the C<match> parameter. If value
is greater than the system limit, the system limit will be used. This
parameter is only used when user matching using the C<match> parameter
is being performed.

=item C<group_ids> (array)

=item C<groups> (array)

C<group_ids> is an array of numeric ids for groups that a user can be in.
C<groups> is an array of names of groups that a user can be in.
If these are specified, they limit the return value to users who are
in I<any> of the groups specified.

=item C<include_disabled> (boolean)

By default, when using the C<match> parameter, disabled users are excluded
from the returned results unless their full username is identical to the
match string. Setting C<include_disabled> to C<true> will include disabled
users in the returned results even if their username doesn't fully match
the input string.

=back

=item B<Returns> 

A hash containing one item, C<users>, that is an array of
hashes. Each hash describes a user, and has the following items:

=over

=item id

C<int> The unique integer ID that Bugzilla uses to represent this user. 
Even if the user's login name changes, this will not change.

=item real_name

C<string> The actual name of the user. May be blank.

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

=item login_denied_text

C<string> A text field that holds the reason for disabling a user from logging
into bugzilla, if empty then the user account is enabled. Otherwise it is 
disabled/closed.

=item groups

C<array> An array of group hashes the user is a member of. If the currently
logged in user is querying their own account or is a member of the 'editusers'
group, the array will contain all the groups that the user is a
member of. Otherwise, the array will only contain groups that the logged in
user can bless. Each hash describes the group and contains the following items:

=over

=item id

C<int> The group id

=item name

C<string> The name of the group

=item description

C<string> The description for the group

=back

=item saved_searches

C<array> An array of hashes, each of which represents a user's saved search and has
the following keys:

=over

=item id

C<int> An integer id uniquely identifying the saved search.

=item name

C<string> The name of the saved search.

=item query

C<string> The CGI parameters for the saved search.

=back

=item saved_reports

C<array> An array of hashes, each of which represents a user's saved report and has
the following keys:

=over

=item id

C<int> An integer id uniquely identifying the saved report.

=item name

C<string> The name of the saved report.

=item query

C<string> The CGI parameters for the saved report.

=back

B<Note>: If you are not logged in to Bugzilla when you call this function, you
will only be returned the C<id>, C<name>, and C<real_name> items. If you are
logged in and not in editusers group, you will only be returned the C<id>, C<name>, 
C<real_name>, C<email>, C<can_login>, and C<groups> items. The groups returned are
filtered based on your permission to bless each group.
The C<saved_searches> and C<saved_reports> items are only returned if you are
querying your own account, even if you are in the editusers group.

=back

=item B<Errors>

=over

=item 51 (Bad Login Name or Group ID)

You passed an invalid login name in the "names" array or a bad
group ID in the C<group_ids> argument.

=item 52 (Invalid Parameter)

The value used must be an integer greater than zero.

=item 304 (Authorization Required)

You are logged in, but you are not authorized to see one of the users you
wanted to get information about by user id.

=item 505 (User Access By Id or User-Matching Denied)

Logged-out users cannot use the "ids" or "match" arguments to this 
function.

=item 804 (Invalid Group Name)

You passed a group name in the C<groups> argument which either does not
exist or you do not belong to it.

=back

=item B<History>

=over

=item Added in Bugzilla B<3.4>.

=item C<group_ids> and C<groups> were added in Bugzilla B<4.0>.

=item C<include_disabled> was added in Bugzilla B<4.0>. Default
behavior for C<match> was changed to only return enabled accounts.

=item Error 804 has been added in Bugzilla 4.0.9 and 4.2.4. It's now
illegal to pass a group name you don't belong to.

=item C<groups>, C<saved_searches>, and C<saved_reports> were added
in Bugzilla B<4.4>.

=item REST API call added in Bugzilla B<5.0>.

=back

=back
