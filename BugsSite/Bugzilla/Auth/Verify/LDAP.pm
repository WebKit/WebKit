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

package Bugzilla::Auth::Verify::LDAP;

use strict;

use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::User;

use Net::LDAP;

my $edit_options = {
    'new' => 0,
    'userid' => 0,
    'login_name' => 0,
    'realname' => 0,
};

sub can_edit {
    my ($class, $type) = @_;
    return $edit_options->{$type};
}

sub authenticate {
    my ($class, $username, $passwd) = @_;

    # If no password was provided, then fail the authentication.
    # While it may be valid to not have an LDAP password, when you
    # bind without a password (regardless of the binddn value), you
    # will get an anonymous bind.  I do not know of a way to determine
    # whether a bind is anonymous or not without making changes to the
    # LDAP access control settings
    return (AUTH_NODATA) unless $username && $passwd;

    # We need to bind anonymously to the LDAP server.  This is
    # because we need to get the Distinguished Name of the user trying
    # to log in.  Some servers (such as iPlanet) allow you to have unique
    # uids spread out over a subtree of an area (such as "People"), so
    # just appending the Base DN to the uid isn't sufficient to get the
    # user's DN.  For servers which don't work this way, there will still
    # be no harm done.
    my $LDAPserver = Param("LDAPserver");
    if ($LDAPserver eq "") {
        return (AUTH_ERROR, undef, "server_not_defined");
    }

    my $LDAPport = "389";  # default LDAP port
    if($LDAPserver =~ /:/) {
        ($LDAPserver, $LDAPport) = split(":",$LDAPserver);
    }
    my $LDAPconn = Net::LDAP->new($LDAPserver, port => $LDAPport, version => 3);
    if(!$LDAPconn) {
        return (AUTH_ERROR, undef, "connect_failed");
    }

    my $mesg;
    if (Param("LDAPbinddn")) {
        my ($LDAPbinddn,$LDAPbindpass) = split(":",Param("LDAPbinddn"));
        $mesg = $LDAPconn->bind($LDAPbinddn, password => $LDAPbindpass);
    }
    else {
        $mesg = $LDAPconn->bind();
    }
    if($mesg->code) {
        return (AUTH_ERROR, undef,
                "connect_failed",
                { errstr => $mesg->error });
    }

    # We've got our anonymous bind;  let's look up this user.
    $mesg = $LDAPconn->search( base   => Param("LDAPBaseDN"),
                               scope  => "sub",
                               filter => '(&(' . Param("LDAPuidattribute") . "=$username)" . Param("LDAPfilter") . ')',
                               attrs  => ['dn'],
                             );
    return (AUTH_LOGINFAILED, undef, "lookup_failure")
        unless $mesg->count;

    # Now we get the DN from this search.
    my $userDN = $mesg->shift_entry->dn;

    # Now we attempt to bind as the specified user.
    $mesg = $LDAPconn->bind( $userDN, password => $passwd);

    return (AUTH_LOGINFAILED) if $mesg->code;

    # And now we're going to repeat the search, so that we can get the
    # mail attribute for this user.
    $mesg = $LDAPconn->search( base   => Param("LDAPBaseDN"),
                               scope  => "sub",
                               filter => '(&(' . Param("LDAPuidattribute") . "=$username)" . Param("LDAPfilter") . ')',
                             );
    my $user_entry = $mesg->shift_entry if !$mesg->code && $mesg->count;
    if(!$user_entry || !$user_entry->exists(Param("LDAPmailattribute"))) {
        return (AUTH_ERROR, undef,
                "cannot_retreive_attr",
                { attr => Param("LDAPmailattribute") });
    }

    # get the mail attribute
    $username = $user_entry->get_value(Param("LDAPmailattribute"));
    # OK, so now we know that the user is valid. Lets try finding them in the
    # Bugzilla database

    # XXX - should this part be made more generic, and placed in
    # Bugzilla::Auth? Lots of login mechanisms may have to do this, although
    # until we actually get some more, its hard to know - BB

    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare_cached("SELECT userid, disabledtext " .
                                   "FROM profiles " .
                                   "WHERE " .
                                   $dbh->sql_istrcmp('login_name', '?'));
    my ($userid, $disabledtext) =
      $dbh->selectrow_array($sth,
                            undef,
                            $username);

    # If the user doesn't exist, then they need to be added
    unless ($userid) {
        # We'll want the user's name for this.
        my $userRealName = $user_entry->get_value("displayName");
        if($userRealName eq "") {
            $userRealName = $user_entry->get_value("cn");
        }
        insert_new_user($username, $userRealName);

        ($userid, $disabledtext) = $dbh->selectrow_array($sth,
                                                         undef,
                                                         $username);
        return (AUTH_ERROR, $userid, "no_userid")
          unless $userid;
    }

    # we're done, so disconnect
    $LDAPconn->unbind;

    # Test for disabled account
    return (AUTH_DISABLED, $userid, $disabledtext)
      if $disabledtext ne '';

    # If we get to here, then the user is allowed to login, so we're done!
    return (AUTH_OK, $userid);
}

1;

__END__

=head1 NAME

Bugzilla::Auth::Verify::LDAP - LDAP based authentication for Bugzilla

This is an L<authentication module|Bugzilla::Auth/"AUTHENTICATION"> for
Bugzilla, which logs the user in using an LDAP directory.

=head1 DISCLAIMER

B<This module is experimental>. It is poorly documented, and not very flexible.
Search L<http://bugzilla.mozilla.org/> for a list of known LDAP bugs.

None of the core Bugzilla developers, nor any of the large installations, use
this module, and so it has received less testing. (In fact, this iteration
hasn't been tested at all)

Patches are accepted.

=head1 SEE ALSO

L<Bugzilla::Auth>
