# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Auth::Verify::LDAP;

use 5.10.1;
use strict;
use warnings;

use base qw(Bugzilla::Auth::Verify);
use fields qw(
    ldap
);

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Util;

use Net::LDAP;
use Net::LDAP::Util qw(escape_filter_value);

use constant admin_can_create_account => 0;
use constant user_can_create_account  => 0;

sub check_credentials {
    my ($self, $params) = @_;
    my $dbh = Bugzilla->dbh;

    # We need to bind anonymously to the LDAP server.  This is
    # because we need to get the Distinguished Name of the user trying
    # to log in.  Some servers (such as iPlanet) allow you to have unique
    # uids spread out over a subtree of an area (such as "People"), so
    # just appending the Base DN to the uid isn't sufficient to get the
    # user's DN.  For servers which don't work this way, there will still
    # be no harm done.
    $self->_bind_ldap_for_search();

    # Now, we verify that the user exists, and get a LDAP Distinguished
    # Name for the user.
    my $username = $params->{username};
    my $dn_result = $self->ldap->search(_bz_search_params($username),
                                        attrs  => ['dn']);
    return { failure => AUTH_ERROR, error => "ldap_search_error",
             details => {errstr => $dn_result->error, username => $username}
    } if $dn_result->code;

    return { failure => AUTH_NO_SUCH_USER } if !$dn_result->count;

    my $dn = $dn_result->shift_entry->dn;

    # Check the password.   
    my $pw_result = $self->ldap->bind($dn, password => $params->{password});
    return { failure => AUTH_LOGINFAILED } if $pw_result->code;

    # And now we fill in the user's details.

    # First try the search as the (already bound) user in question.
    my $user_entry;
    my $error_string;
    my $detail_result = $self->ldap->search(_bz_search_params($username));
    if ($detail_result->code) {
        # Stash away the original error, just in case
        $error_string = $detail_result->error;
    } else {
        $user_entry = $detail_result->shift_entry;
    }

    # If that failed (either because the search failed, or returned no
    # results) then try re-binding as the initial search user, but only
    # if the LDAPbinddn parameter is set.
    if (!$user_entry && Bugzilla->params->{"LDAPbinddn"}) {
        $self->_bind_ldap_for_search();

        $detail_result = $self->ldap->search(_bz_search_params($username));
        if (!$detail_result->code) {
            $user_entry = $detail_result->shift_entry;
        }
    }

    # If we *still* don't have anything in $user_entry then give up.
    return { failure => AUTH_ERROR, error => "ldap_search_error",
             details => {errstr => $error_string, username => $username}
    } if !$user_entry;


    my $mail_attr = Bugzilla->params->{"LDAPmailattribute"};
    if ($mail_attr) {
        if (!$user_entry->exists($mail_attr)) {
            return { failure => AUTH_ERROR,
                     error   => "ldap_cannot_retreive_attr",
                     details => {attr => $mail_attr} };
        }

        my @emails = $user_entry->get_value($mail_attr);

        # Default to the first email address returned.
        $params->{bz_username} = $emails[0];

        if (@emails > 1) {
            # Cycle through the adresses and check if they're Bugzilla logins.
            # Use the first one that returns a valid id. 
            foreach my $email (@emails) {
                if ( login_to_id($email) ) {
                    $params->{bz_username} = $email;
                    last;
                }
            }
        }

    } else {
        $params->{bz_username} = $username;
    }

    $params->{realname}  ||= $user_entry->get_value("displayName");
    $params->{realname}  ||= $user_entry->get_value("cn");

    $params->{extern_id} = $username;

    return $params;
}

sub _bz_search_params {
    my ($username) = @_;
    $username = escape_filter_value($username);
    return (base   => Bugzilla->params->{"LDAPBaseDN"},
            scope  => "sub",
            filter => '(&(' . Bugzilla->params->{"LDAPuidattribute"} 
                      . "=$username)"
                      . Bugzilla->params->{"LDAPfilter"} . ')');
}

sub _bind_ldap_for_search {
    my ($self) = @_;
    my $bind_result;
    if (Bugzilla->params->{"LDAPbinddn"}) {
        my ($LDAPbinddn,$LDAPbindpass) = 
            split(":",Bugzilla->params->{"LDAPbinddn"});
        $bind_result = 
            $self->ldap->bind($LDAPbinddn, password => $LDAPbindpass);
    }
    else {
        $bind_result = $self->ldap->bind();
    }
    ThrowCodeError("ldap_bind_failed", {errstr => $bind_result->error})
        if $bind_result->code;
}

# We can't just do this in new(), because we're not allowed to throw any
# error from anywhere under Bugzilla::Auth::new -- otherwise we
# could create a situation where the admin couldn't get to editparams
# to fix their mistake. (Because Bugzilla->login always calls
# Bugzilla::Auth->new, and almost every page calls Bugzilla->login.)
sub ldap {
    my ($self) = @_;
    return $self->{ldap} if $self->{ldap};

    my @servers = split(/[\s,]+/, Bugzilla->params->{"LDAPserver"});
    ThrowCodeError("ldap_server_not_defined") unless @servers;

    foreach (@servers) {
        $self->{ldap} = new Net::LDAP(trim($_));
        last if $self->{ldap};
    }
    ThrowCodeError("ldap_connect_failed", { server => join(", ", @servers) }) 
        unless $self->{ldap};

    # try to start TLS if needed
    if (Bugzilla->params->{"LDAPstarttls"}) {
        my $mesg = $self->{ldap}->start_tls();
        ThrowCodeError("ldap_start_tls_failed", { error => $mesg->error() })
            if $mesg->code();
    }

    return $self->{ldap};
}

1;
