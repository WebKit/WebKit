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
# Contributor(s): Erik Stambaugh <erik@dasbistro.com>

package Bugzilla::Auth::Login::WWW;

use strict;

use Bugzilla::Constants;
use Bugzilla::Config;

# $current_login_class stores the name of the login style that succeeded.
my $current_login_class = undef;
sub login_class {
    my ($class, $type) = @_;
    if ($type) {
        $current_login_class = $type;
    }
    return $current_login_class;
}

# can_logout determines if a user may log out
sub can_logout {
    return 1 if (login_class && login_class->can_logout);
    return 0;
}

sub login {
    my ($class, $type) = @_;

    my $user = Bugzilla->user;

    # Avoid double-logins, which may confuse the auth code
    # (double cookies, odd compat code settings, etc)
    return $user if $user->id;

    $type = LOGIN_REQUIRED if Bugzilla->cgi->param('GoAheadAndLogIn');
    $type = LOGIN_NORMAL unless defined $type;

    # Log in using whatever methods are defined in user_info_class.
    # Please note the particularly strange way require() and the function
    # calls are being done, because we're calling a module that's named in
    # a string. I assure you it works, and it avoids the need for an eval().
    my $userid;
    for my $login_class (split(/,\s*/, Param('user_info_class'))) {
        require "Bugzilla/Auth/Login/WWW/" . $login_class . ".pm";
        $userid = "Bugzilla::Auth::Login::WWW::$login_class"->login($type);
        if ($userid) {
            $class->login_class("Bugzilla::Auth::Login::WWW::$login_class");
            last;
        }
    }

    if ($userid) {
        $user = new Bugzilla::User($userid);

        # Redirect to SSL if required
        if (Param('sslbase') ne '' and Param('ssl') ne 'never') {
            Bugzilla->cgi->require_https(Param('sslbase'));
        }

        $user->set_flags('can_logout' => $class->can_logout);

        # Compat stuff
        $::userid = $userid;
    } else {
        Bugzilla->logout_request();
    }
    return $user;
}

sub logout {
    my ($class, $user, $option) = @_;
    if (can_logout) {
        $class->login_class->logout($user, $option);
    }
}

1;


__END__

=head1 NAME

Bugzilla::Auth::Login::WWW - WWW login information gathering module

=head1 METHODS

=over

=item C<login>

Passes C<login> calls to each class defined in the param C<user_info_class>
and returns a C<Bugzilla::User> object from the first one that successfully
gathers user login information.

=back
