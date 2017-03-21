# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Auth::Verify::DB;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Auth::Verify);

use Bugzilla::Constants;
use Bugzilla::Token;
use Bugzilla::Util;
use Bugzilla::User;

sub check_credentials {
    my ($self, $login_data) = @_;
    my $dbh = Bugzilla->dbh;

    my $username = $login_data->{username};
    my $user = new Bugzilla::User({ name => $username });

    return { failure => AUTH_NO_SUCH_USER } unless $user;

    $login_data->{user} = $user;
    $login_data->{bz_username} = $user->login;

    if ($user->account_is_locked_out) {
        return { failure => AUTH_LOCKOUT, user => $user };
    }

    my $password = $login_data->{password};
    my $real_password_crypted = $user->cryptpassword;

    # Using the internal crypted password as the salt,
    # crypt the password the user entered.
    my $entered_password_crypted = bz_crypt($password, $real_password_crypted);

    if ($entered_password_crypted ne $real_password_crypted) {
        # Record the login failure
        $user->note_login_failure();

        # Immediately check if we are locked out
        if ($user->account_is_locked_out) {
            return { failure => AUTH_LOCKOUT, user => $user,
                     just_locked_out => 1 };
        }

        return { failure => AUTH_LOGINFAILED,
                 failure_count => scalar(@{ $user->account_ip_login_failures }),
               };
    } 

    # Force the user to change their password if it does not meet the current
    # criteria. This should usually only happen if the criteria has changed.
    if (Bugzilla->usage_mode == USAGE_MODE_BROWSER &&
        Bugzilla->params->{password_check_on_login})
    {
        my $check = validate_password_check($password);
        if ($check) {
            return {
                failure => AUTH_ERROR,
                user_error => $check,
                details => { locked_user => $user }
            }
        }
    }

    # The user's credentials are okay, so delete any outstanding
    # password tokens or login failures they may have generated.
    Bugzilla::Token::DeletePasswordTokens($user->id, "user_logged_in");
    $user->clear_login_failures();

    my $update_password = 0;

    # If their old password was using crypt() or some different hash
    # than we're using now, convert the stored password to using
    # whatever hashing system we're using now.
    my $current_algorithm = PASSWORD_DIGEST_ALGORITHM;
    $update_password = 1 if ($real_password_crypted !~ /{\Q$current_algorithm\E}$/);

    # If their old password was using a different length salt than what
    # we're using now, update the password to use the new salt length.
    if ($real_password_crypted =~ /^([^,]+),/) {
        $update_password = 1 if (length($1) != PASSWORD_SALT_LENGTH);
    }

    # If needed, update the user's password.
    if ($update_password) {
        # We can't call $user->set_password because we don't want the password
        # complexity rules to apply here.
        $user->{cryptpassword} = bz_crypt($password);
        $user->update();
    }

    return $login_data;
}

sub change_password {
    my ($self, $user, $password) = @_;
    my $dbh = Bugzilla->dbh;
    my $cryptpassword = bz_crypt($password);
    $dbh->do("UPDATE profiles SET cryptpassword = ? WHERE userid = ?",
             undef, $cryptpassword, $user->id);
    Bugzilla->memcached->clear({ table => 'profiles', id => $user->id });
}

1;
