# -*- Mode: perl; indent-tabs-mode: nil -*-
#
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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Joe Robins <jmrobins@tgix.com>
#                 Dave Miller <justdave@syndicomm.com>
#                 Christopher Aillon <christopher@aillon.com>
#                 Gervase Markham <gerv@gerv.net>
#                 Christian Reis <kiko@async.com.br>
#                 Bradley Baetz <bbaetz@acm.org>
#                 Erik Stambaugh <erik@dasbistro.com>

package Bugzilla::Auth::Verify::DB;
use strict;
use base qw(Bugzilla::Auth::Verify);

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

    # Force the user to type a longer password if it's too short.
    if (length($password) < USER_PASSWORD_MIN_LENGTH) {
        return { failure => AUTH_ERROR, user_error => 'password_current_too_short',
                 details => { locked_user => $user } };
    }

    # The user's credentials are okay, so delete any outstanding
    # password tokens or login failures they may have generated.
    Bugzilla::Token::DeletePasswordTokens($user->id, "user_logged_in");
    $user->clear_login_failures();

    # If their old password was using crypt() or some different hash
    # than we're using now, convert the stored password to using
    # whatever hashing system we're using now.
    my $current_algorithm = PASSWORD_DIGEST_ALGORITHM;
    if ($real_password_crypted !~ /{\Q$current_algorithm\E}$/) {
        $user->set_password($password);
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
}

1;
