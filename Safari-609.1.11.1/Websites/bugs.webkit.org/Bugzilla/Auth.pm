# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Auth;

use 5.10.1;
use strict;
use warnings;

use fields qw(
    _info_getter
    _verifier
    _persister
);

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Mailer;
use Bugzilla::Util qw(datetime_from);
use Bugzilla::User::Setting ();
use Bugzilla::Auth::Login::Stack;
use Bugzilla::Auth::Verify::Stack;
use Bugzilla::Auth::Persist::Cookie;
use Socket;

sub new {
    my ($class, $params) = @_;
    my $self = fields::new($class);

    $params            ||= {};
    $params->{Login}   ||= Bugzilla->params->{'user_info_class'} . ',Cookie,APIKey';
    $params->{Verify}  ||= Bugzilla->params->{'user_verify_class'};

    $self->{_info_getter} = new Bugzilla::Auth::Login::Stack($params->{Login});
    $self->{_verifier} = new Bugzilla::Auth::Verify::Stack($params->{Verify});
    # If we ever have any other login persistence methods besides cookies,
    # this could become more configurable.
    $self->{_persister} = new Bugzilla::Auth::Persist::Cookie();

    return $self;
}

sub login {
    my ($self, $type) = @_;

    # Get login info from the cookie, form, environment variables, etc.
    my $login_info = $self->{_info_getter}->get_login_info();

    if ($login_info->{failure}) {
        return $self->_handle_login_result($login_info, $type);
    }

    # Now verify their username and password against the DB, LDAP, etc.
    if ($self->{_info_getter}->{successful}->requires_verification) {
        $login_info = $self->{_verifier}->check_credentials($login_info);
        if ($login_info->{failure}) {
            return $self->_handle_login_result($login_info, $type);
        }
        $login_info =
          $self->{_verifier}->{successful}->create_or_update_user($login_info);
    }
    else {
        $login_info = $self->{_verifier}->create_or_update_user($login_info);
    }

    if ($login_info->{failure}) {
        return $self->_handle_login_result($login_info, $type);
    }

    # Make sure the user isn't disabled.
    my $user = $login_info->{user};
    if (!$user->is_enabled) {
        return $self->_handle_login_result({ failure => AUTH_DISABLED,
                                              user    => $user }, $type);
    }
    $user->set_authorizer($self);

    return $self->_handle_login_result($login_info, $type);
}

sub can_change_password {
    my ($self) = @_;
    my $verifier = $self->{_verifier}->{successful};
    $verifier  ||= $self->{_verifier};
    my $getter   = $self->{_info_getter}->{successful};
    $getter      = $self->{_info_getter} 
        if (!$getter || $getter->isa('Bugzilla::Auth::Login::Cookie'));
    return $verifier->can_change_password &&
           $getter->user_can_create_account;
}

sub can_login {
    my ($self) = @_;
    my $getter = $self->{_info_getter}->{successful};
    $getter    = $self->{_info_getter}
        if (!$getter || $getter->isa('Bugzilla::Auth::Login::Cookie'));
    return $getter->can_login;
}

sub can_logout {
    my ($self) = @_;
    my $getter = $self->{_info_getter}->{successful};
    # If there's no successful getter, we're not logged in, so of
    # course we can't log out!
    return 0 unless $getter;
    return $getter->can_logout;
}

sub login_token {
    my ($self) = @_;
    my $getter = $self->{_info_getter}->{successful};
    if ($getter && $getter->isa('Bugzilla::Auth::Login::Cookie')) {
        return $getter->login_token;
    }
    return undef;
}

sub user_can_create_account {
    my ($self) = @_;
    my $verifier = $self->{_verifier}->{successful};
    $verifier  ||= $self->{_verifier};
    my $getter   = $self->{_info_getter}->{successful};
    $getter      = $self->{_info_getter}
        if (!$getter || $getter->isa('Bugzilla::Auth::Login::Cookie'));
    return $verifier->user_can_create_account
           && $getter->user_can_create_account;
}

sub extern_id_used {
    my ($self) = @_;
    return $self->{_info_getter}->extern_id_used
           ||  $self->{_verifier}->extern_id_used;
}

sub can_change_email {
    return $_[0]->user_can_create_account;
}

sub _handle_login_result {
    my ($self, $result, $login_type) = @_;
    my $dbh = Bugzilla->dbh;

    my $user      = $result->{user};
    my $fail_code = $result->{failure};

    if (!$fail_code) {
        # We don't persist logins over GET requests in the WebService,
        # because the persistance information can't be re-used again.
        # (See Bugzilla::WebService::Server::JSONRPC for more info.)
        if ($self->{_info_getter}->{successful}->requires_persistence
            and !Bugzilla->request_cache->{auth_no_automatic_login}) 
        {
            $user->{_login_token} = $self->{_persister}->persist_login($user);
        }
    }
    elsif ($fail_code == AUTH_ERROR) {
        if ($result->{user_error}) {
            ThrowUserError($result->{user_error}, $result->{details});
        }
        else {
            ThrowCodeError($result->{error}, $result->{details});
        }
    }
    elsif ($fail_code == AUTH_NODATA) {
        $self->{_info_getter}->fail_nodata($self) 
            if $login_type == LOGIN_REQUIRED;

        # If we're not LOGIN_REQUIRED, we just return the default user.
        $user = Bugzilla->user;
    }
    # The username/password may be wrong
    # Don't let the user know whether the username exists or whether
    # the password was just wrong. (This makes it harder for a cracker
    # to find account names by brute force)
    elsif ($fail_code == AUTH_LOGINFAILED or $fail_code == AUTH_NO_SUCH_USER) {
        my $remaining_attempts = MAX_LOGIN_ATTEMPTS 
                                 - ($result->{failure_count} || 0);
        ThrowUserError("invalid_login_or_password", 
                       { remaining => $remaining_attempts });
    }
    # The account may be disabled
    elsif ($fail_code == AUTH_DISABLED) {
        $self->{_persister}->logout();
        # XXX This is NOT a good way to do this, architecturally.
        $self->{_persister}->clear_browser_cookies();
        # and throw a user error
        ThrowUserError("account_disabled",
            {'disabled_reason' => $result->{user}->disabledtext});
    }
    elsif ($fail_code == AUTH_LOCKOUT) {
        my $attempts = $user->account_ip_login_failures;

        # We want to know when the account will be unlocked. This is 
        # determined by the 5th-from-last login failure (or more/less than
        # 5th, if MAX_LOGIN_ATTEMPTS is not 5).
        my $determiner = $attempts->[scalar(@$attempts) - MAX_LOGIN_ATTEMPTS];
        my $unlock_at = datetime_from($determiner->{login_time}, 
                                      Bugzilla->local_timezone);
        $unlock_at->add(minutes => LOGIN_LOCKOUT_INTERVAL);

        # If we were *just* locked out, notify the maintainer about the
        # lockout.
        if ($result->{just_locked_out}) {
            # We're sending to the maintainer, who may be not a Bugzilla 
            # account, but just an email address. So we use the
            # installation's default language for sending the email.
            my $default_settings = Bugzilla::User::Setting::get_defaults();
            my $template = Bugzilla->template_inner(
                               $default_settings->{lang}->{default_value});
            my $address = $attempts->[0]->{ip_addr};
            # Note: inet_aton will only resolve IPv4 addresses.
            # For IPv6 we'll need to use inet_pton which requires Perl 5.12.
            my $n = inet_aton($address);
            if ($n) {
                $address = gethostbyaddr($n, AF_INET) . " ($address)"
            }
            my $vars = {
                locked_user => $user,
                attempts    => $attempts,
                unlock_at   => $unlock_at,
                address     => $address,
            };
            my $message;
            $template->process('email/lockout.txt.tmpl', $vars, \$message)
                || ThrowTemplateError($template->error);
            MessageToMTA($message);
        }

        $unlock_at->set_time_zone($user->timezone);
        ThrowUserError('account_locked', 
            { ip_addr => $determiner->{ip_addr}, unlock_at => $unlock_at });
    }
    # If we get here, then we've run out of options, which shouldn't happen.
    else {
        ThrowCodeError("authres_unhandled", { value => $fail_code });
    }

    return $user;
}

1;

__END__

=head1 NAME

Bugzilla::Auth - An object that authenticates the login credentials for
                 a user.

=head1 DESCRIPTION

Handles authentication for Bugzilla users.

Authentication from Bugzilla involves two sets of modules. One set is
used to obtain the username/password (from CGI, email, etc), and the 
other set uses this data to authenticate against the datasource 
(the Bugzilla DB, LDAP, PAM, etc.).

Modules for obtaining the username/password are subclasses of 
L<Bugzilla::Auth::Login>, and modules for authenticating are subclasses
of L<Bugzilla::Auth::Verify>.

=head1 AUTHENTICATION ERROR CODES

Whenever a method in the C<Bugzilla::Auth> family fails in some way,
it will return a hashref containing at least a single key called C<failure>.
C<failure> will point to an integer error code, and depending on the error
code the hashref may contain more data.

The error codes are explained here below.

=head2 C<AUTH_NODATA>

Insufficient login data was provided by the user. This may happen in several
cases, such as cookie authentication when the cookie is not present.

=head2 C<AUTH_ERROR>

An error occurred when trying to use the login mechanism.

The hashref will also contain an C<error> element, which is the name
of an error from C<template/en/default/global/code-error.html> --
the same type of error that would be thrown by 
L<Bugzilla::Error::ThrowCodeError>.

The hashref *may* contain an element called C<details>, which is a hashref
that should be passed to L<Bugzilla::Error::ThrowCodeError> as the 
various fields to be used in the error message.

=head2 C<AUTH_LOGINFAILED>

An incorrect username or password was given.

The hashref may also contain a C<failure_count> element, which specifies
how many times the account has failed to log in within the lockout
period (see L</AUTH_LOCKOUT>). This is used to warn the user when
they are getting close to being locked out.

=head2 C<AUTH_NO_SUCH_USER>

This is an optional more-specific version of C<AUTH_LOGINFAILED>.
Modules should throw this error when they discover that the
requested user account actually does not exist, according to them.

That is, for example, L<Bugzilla::Auth::Verify::LDAP> would throw
this if the user didn't exist in LDAP.

The difference between C<AUTH_NO_SUCH_USER> and C<AUTH_LOGINFAILED>
should never be communicated to the user, for security reasons.

=head2 C<AUTH_DISABLED>

The user successfully logged in, but their account has been disabled.
Usually this is throw only by C<Bugzilla::Auth::login>.

=head2 C<AUTH_LOCKOUT>

The user's account is locked out after having failed to log in too many
times within a certain period of time (as specified by
L<Bugzilla::Constants/LOGIN_LOCKOUT_INTERVAL>).

The hashref will also contain a C<user> element, representing the
L<Bugzilla::User> whose account is locked out.

=head1 LOGIN TYPES

The C<login> function (below) can do different types of login, depending
on what constant you pass into it:

=head2 C<LOGIN_OPTIONAL>

A login is never required to access this data. Attempting to login is
still useful, because this allows the page to be personalised. Note that
an incorrect login will still trigger an error, even though the lack of
a login will be OK.

=head2 C<LOGIN_NORMAL>

A login may or may not be required, depending on the setting of the
I<requirelogin> parameter. This is the default if you don't specify a
type.

=head2 C<LOGIN_REQUIRED>

A login is always required to access this data.

=head1 METHODS

These are methods that can be called on a C<Bugzilla::Auth> object 
itself.

=head2 Login

=over 4

=item C<login($type)>

Description: Logs a user in. For more details on how this works
             internally, see the section entitled "STRUCTURE."
Params:      $type - One of the Login Types from above.
Returns:     An authenticated C<Bugzilla::User>. Or, if the type was
             not C<LOGIN_REQUIRED>, then we return an
             empty C<Bugzilla::User> if no login data was passed in.

=back

=head2 Info Methods

These are methods that give information about the Bugzilla::Auth object.

=over 4

=item C<can_change_password>

Description: Tells you whether or not the current login system allows
             changing passwords.
Params:      None
Returns:     C<true> if users and administrators should be allowed to
             change passwords, C<false> otherwise.

=item C<can_login>

Description: Tells you whether or not the current login system allows
             users to log in through the web interface.
Params:      None
Returns:     C<true> if users can log in through the web interface,
             C<false> otherwise.

=item C<can_logout>

Description: Tells you whether or not the current login system allows
             users to log themselves out.
Params:      None
Returns:     C<true> if users can log themselves out, C<false> otherwise.
             If a user isn't logged in, we always return C<false>.

=item C<user_can_create_account>

Description: Tells you whether or not users are allowed to manually create
             their own accounts, based on the current login system in use.
             Note that this doesn't check the C<createemailregexp>
             parameter--you have to do that by yourself in your code.
Params:      None
Returns:     C<true> if users are allowed to create new Bugzilla accounts,
             C<false> otherwise.

=item C<extern_id_used>

Description: Whether or not current login system uses extern_id.

=item C<can_change_email>

Description: Whether or not the current login system allows users to
             change their own email address.
Params:      None
Returns:     C<true> if users can change their own email address,
             C<false> otherwise.

=item C<login_token>

Description: If a login token was used instead of a cookie then this
             will return the current login token data such as user id
             and the token itself.
Params:      None
Returns:     A hash containing C<login_token> and C<user_id>.

=back

=head1 STRUCTURE

This section is mostly interesting to developers who want to implement
a new authentication type. It describes the general structure of the
Bugzilla::Auth family, and how the C<login> function works.

A C<Bugzilla::Auth> object is essentially a collection of a few other
objects: the "Info Getter," the "Verifier," and the "Persistence 
Mechanism."

They are used inside the C<login> function in the following order:

=head2 The Info Getter

This is a C<Bugzilla::Auth::Login> object. Basically, it gets the
username and password from the user, somehow. Or, it just gets enough
information to uniquely identify a user, and passes that on down the line.
(For example, a C<user_id> is enough to uniquely identify a user,
even without a username and password.)

Some Info Getters don't require any verification. For example, if we got
the C<user_id> from a Cookie, we don't need to check the username and 
password.

If an Info Getter returns only a C<user_id> and no username/password,
then it MUST NOT require verification. If an Info Getter requires
verfication, then it MUST return at least a C<username>.

=head2 The Verifier

This verifies that the username and password are valid.

It's possible that some methods of verification don't require a password.

=head2 The Persistence Mechanism

This makes it so that the user doesn't have to log in on every page.
Normally this object just sends a cookie to the user's web browser,
as that's the most common method of "login persistence."

=head2 Other Things We Do

After we verify the username and password, sometimes we automatically
create an account in the Bugzilla database, for certain authentication
types. We use the "Account Source" to get data about the user, and
create them in the database. (Or, if their data has changed since the
last time they logged in, their data gets updated.)

=head2 The C<$login_data> Hash

All of the C<Bugzilla::Auth::Login> and C<Bugzilla::Auth::Verify>
methods take an argument called C<$login_data>. This is basically
a hash that becomes more and more populated as we go through the
C<login> function.

All C<Bugzilla::Auth::Login> and C<Bugzilla::Auth::Verify> methods
also *return* the C<$login_data> structure, when they succeed. They
may have added new data to it.

For all C<Bugzilla::Auth::Login> and C<Bugzilla::Auth::Verify> methods,
the rule is "you must return the same hashref you were passed in." You can
modify the hashref all you want, but you can't create a new one. The only
time you can return a new one is if you're returning some error code
instead of the C<$login_data> structure.

Each C<Bugzilla::Auth::Login> or C<Bugzilla::Auth::Verify> method
explains in its documentation which C<$login_data> elements are
required by it, and which are set by it.

Here are all of the elements that *may* be in C<$login_data>:

=over 4

=item C<user_id>

A Bugzilla C<user_id> that uniquely identifies a user.

=item C<username>

The username that was provided by the user.

=item C<bz_username>

The username of this user inside of Bugzilla. Sometimes this differs from
C<username>.

=item C<password>

The password provided by the user.

=item C<realname>

The real name of the user.

=item C<extern_id>

Some string that uniquely identifies the user in an external account 
source. If this C<extern_id> already exists in the database with
a different username, the username will be *changed* to be the
username specified in this C<$login_data>.

That is, let's my extern_id is C<mkanat>. I already have an account
in Bugzilla with the username of C<mkanat@foo.com>. But this time,
when I log in, I have an extern_id of C<mkanat> and a C<username>
of C<mkanat@bar.org>. So now, Bugzilla will automatically change my 
username to C<mkanat@bar.org> instead of C<mkanat@foo.com>.

=item C<user>

A L<Bugzilla::User> object representing the authenticated user. 
Note that C<Bugzilla::Auth::login> may modify this object at various points.

=back
