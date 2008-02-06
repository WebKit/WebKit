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

use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::User;

my $edit_options = {
    'new' => 1,
    'userid' => 0,
    'login_name' => 1,
    'realname' => 1,
};

sub can_edit {
    my ($class, $type) = @_;
    return $edit_options->{$type};
}

sub authenticate {
    my ($class, $username, $passwd) = @_;

    return (AUTH_NODATA) unless defined $username && defined $passwd;

    my $userid = Bugzilla::User::login_to_id($username);
    return (AUTH_LOGINFAILED) unless $userid;

    return (AUTH_LOGINFAILED, $userid) 
        unless $class->check_password($userid, $passwd);

    # The user's credentials are okay, so delete any outstanding
    # password tokens they may have generated.
    require Bugzilla::Token;
    Bugzilla::Token::DeletePasswordTokens($userid, "user_logged_in");

    # Account may have been disabled
    my $disabledtext = $class->get_disabled($userid);
    return (AUTH_DISABLED, $userid, $disabledtext)
      if $disabledtext ne '';

    return (AUTH_OK, $userid);
}

sub get_disabled {
    my ($class, $userid) = @_;
    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare_cached("SELECT disabledtext FROM profiles " .
                                   "WHERE userid=?");
    my ($text) = $dbh->selectrow_array($sth, undef, $userid);
    return $text;
}

sub check_password {
    my ($class, $userid, $passwd) = @_;
    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare_cached("SELECT cryptpassword FROM profiles " .
                                   "WHERE userid=?");
    my ($realcryptpwd) = $dbh->selectrow_array($sth, undef, $userid);

    # Get the salt from the user's crypted password.
    my $salt = $realcryptpwd;

    # Using the salt, crypt the password the user entered.
    my $enteredCryptedPassword = crypt($passwd, $salt);

    return $enteredCryptedPassword eq $realcryptpwd;
}

sub change_password {
    my ($class, $userid, $password) = @_;
    my $dbh = Bugzilla->dbh;
    my $cryptpassword = bz_crypt($password);
    $dbh->do("UPDATE profiles SET cryptpassword = ? WHERE userid = ?", 
             undef, $cryptpassword, $userid);
}

1;

__END__

=head1 NAME

Bugzilla::Auth::Verify::DB - database authentication for Bugzilla

=head1 SUMMARY

This is an L<authentication module|Bugzilla::Auth/"AUTHENTICATION"> for
Bugzilla, which logs the user in using the password stored in the C<profiles>
table. This is the most commonly used authentication module.

=head1 SEE ALSO

L<Bugzilla::Auth>
