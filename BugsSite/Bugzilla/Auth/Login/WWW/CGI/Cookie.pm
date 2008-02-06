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

package Bugzilla::Auth::Login::WWW::CGI::Cookie;

use strict;

use Bugzilla::Auth;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Util;

sub authenticate {
    my ($class, $login, $login_cookie) = @_;

    return (AUTH_NODATA) unless defined $login && defined $login_cookie;

    my $cgi = Bugzilla->cgi;

    my $ipaddr = $cgi->remote_addr();
    my $netaddr = Bugzilla::Auth::get_netaddr($ipaddr);

    # Anything goes for these params - they're just strings which
    # we're going to verify against the db
    trick_taint($login);
    trick_taint($login_cookie);
    trick_taint($ipaddr);

    my $query = "SELECT profiles.userid, profiles.disabledtext " .
                "FROM logincookies, profiles " .
                "WHERE logincookies.cookie=? AND " .
                "  logincookies.userid=profiles.userid AND " .
                "  logincookies.userid=? AND " .
                "  (logincookies.ipaddr=?";
    my @params = ($login_cookie, $login, $ipaddr);
    if (defined $netaddr) {
        trick_taint($netaddr);
        $query .= " OR logincookies.ipaddr=?";
        push(@params, $netaddr);
    }
    $query .= ")";

    my $dbh = Bugzilla->dbh;
    my ($userid, $disabledtext) = $dbh->selectrow_array($query, undef, @params);

    return (AUTH_DISABLED, $userid, $disabledtext)
      if ($disabledtext);

    if ($userid) {
        # If we logged in successfully, then update the lastused time on the
        # login cookie
        $dbh->do("UPDATE logincookies SET lastused=NOW() WHERE cookie=?",
                 undef,
                 $login_cookie);

        return (AUTH_OK, $userid);
    }

    # If we get here, then the login failed.
    return (AUTH_LOGINFAILED);
}

1;

__END__

=head1 NAME

Bugzilla::Auth::Login::WWW::CGI::Cookie - cookie authentication for Bugzilla

=head1 SUMMARY

This is an L<authentication module|Bugzilla::Auth/"AUTHENTICATION"> for
Bugzilla, which logs the user in using a persistent cookie stored in the
C<logincookies> table.

The actual password is not stored in the cookie; only the userid and a
I<logincookie> (which is used to reverify the login without requiring the
password to be sent over the network) are. These I<logincookies> are
restricted to certain IP addresses as a security meaure. The exact
restriction can be specified by the admin via the C<loginnetmask> parameter.

This module does not ever send a cookie (It has no way of knowing when a user
is successfully logged in). Instead L<Bugzilla::Auth::Login::WWW::CGI> handles this.

=head1 SEE ALSO

L<Bugzilla::Auth>, L<Bugzilla::Auth::Login::WWW::CGI>
