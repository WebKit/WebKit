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

package Bugzilla::Auth::Login::WWW::Env;

use strict;

use Bugzilla::Config;
use Bugzilla::Error;
use Bugzilla::Util;

sub login {
    my ($class, $type) = @_;

    # XXX This does not currently work correctly with Param('requirelogin').
    #     Bug 253636 will hopefully see that param's needs taken care of in a
    #     parent module, but for the time being, this module does not honor
    #     the param in the way that CGI.pm does.

    my $matched_userid    = '';
    my $matched_extern_id = '';
    my $disabledtext      = '';

    my $dbh = Bugzilla->dbh;
    my $sth;

    # Gather the environment variables
    my $env_id       = $ENV{Param("auth_env_id")};
    my $env_email    = $ENV{Param("auth_env_email")};
    my $env_realname = $ENV{Param("auth_env_realname")};

    # allow undefined values to work with trick_taint
    for ($env_id, $env_email, $env_realname) { $_ ||= '' };
    # make sure the email field contains only a valid email address
    my $emailregexp = Param("emailregexp");
    if ($env_email =~ /($emailregexp)/) {
        $env_email = $1;
    }
    else {
        return undef;
    }
    # untaint the remaining values
    trick_taint($env_id);
    trick_taint($env_realname);

    if ($env_id || $env_email) {
        # Look in the DB for the extern_id
        if ($env_id) {

            # Not having the email address defined but having an ID isn't
            # allowed.
            return undef unless $env_email;

            $sth = $dbh->prepare("SELECT userid, disabledtext " .
                                 "FROM profiles WHERE extern_id=?");
            $sth->execute($env_id);
            my $fetched = $sth->fetch;
            if ($fetched) {
                $matched_userid = $fetched->[0];
                $disabledtext   = $fetched->[1];
            }
        }

        unless ($matched_userid) {
            # There was either no match for the external ID given, or one was
            # not present.
            #
            # Check to see if the email address is in there and has no
            # external id assigned.  We test for both the login name (which we
            # also sent), and the id, so that we have a way of telling that we
            # got something instead of a bunch of NULLs
            $sth = $dbh->prepare("SELECT extern_id, userid, disabledtext " .
                                 "FROM profiles WHERE " .
                                 $dbh->sql_istrcmp('login_name', '?'));
            $sth->execute($env_email);

            $sth->execute();
            my $fetched = $sth->fetch();
            if ($fetched) {
                ($matched_extern_id, $matched_userid, $disabledtext) = @{$fetched};
            }
            if ($matched_userid) {
                if ($matched_extern_id) {
                    # someone with a different external ID has that address!
                    ThrowUserError("extern_id_conflict");
                }
                else
                {
                    # someone with no external ID used that address, time to
                    # add the ID!
                    $sth = $dbh->prepare("UPDATE profiles " .
                                         "SET extern_id=? WHERE userid=?");
                    $sth->execute($env_id, $matched_userid);
                }
            }
            else
            {
                # Need to create a new user with that email address.  Note
                # that cryptpassword has been filled in with '*', since the
                # user has no DB password.
                $sth = $dbh->prepare("INSERT INTO profiles ( " .
                                     "login_name, cryptpassword, " .
                                     "realname, disabledtext " .
                                     ") VALUES ( ?, ?, ?, '' )");
                $sth->execute($env_email, '*', $env_realname);
                $matched_userid = $dbh->bz_last_key('profiles', 'userid');
            }
        }
    }

    # now that we hopefully have a username, we need to see if the data
    # has to be updated
    if ($matched_userid) {
        $sth = $dbh->prepare("SELECT login_name, realname " .
                             "FROM profiles " .
                             "WHERE userid=?");
        $sth->execute($matched_userid);
        my $fetched = $sth->fetch;
        my $username = $fetched->[0];
        my $this_realname = $fetched->[1];
        if ( ($username ne $env_email) ||
             ($this_realname ne $env_realname) ) {

            $sth = $dbh->prepare("UPDATE profiles " .
                                 "SET login_name=?, " .
                                 "realname=? " .
                                 "WHERE userid=?");
            $sth->execute($env_email,
                          ($env_realname || $this_realname),
                          $matched_userid);
            $sth->execute;
        }
    }

    # Now we throw an error if the user has been disabled
    if ($disabledtext) {
        ThrowUserError("account_disabled",
                       {'disabled_reason' => $disabledtext});
    }

    return $matched_userid;

}

# This auth style does not allow the user to log out.
sub can_logout { return 0; }

1;

__END__

=head1 NAME

Bugzilla::Auth::Env - Environment Variable Authentication

=head1 DESCRIPTION

Many external user authentication systems supply login information to CGI
programs via environment variables.  This module checks to see if those
variables are populated and, if so, assumes authentication was successful and
returns the user's ID, having automatically created a new profile if
necessary.

=head1 SEE ALSO

L<Bugzilla::Auth>

