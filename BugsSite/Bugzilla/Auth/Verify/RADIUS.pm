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
# The Initial Developer of the Original Code is Marc Schumann.
# Portions created by Marc Schumann are Copyright (c) 2007 Marc Schumann.
# All rights reserved.
#
# Contributor(s): Marc Schumann <wurblzap@gmail.com>

package Bugzilla::Auth::Verify::RADIUS;
use strict;
use base qw(Bugzilla::Auth::Verify);

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;

use Authen::Radius;

use constant admin_can_create_account => 0;
use constant user_can_create_account  => 0;

sub check_credentials {
    my ($self, $params) = @_;
    my $dbh = Bugzilla->dbh;
    my $address_suffix = Bugzilla->params->{'RADIUS_email_suffix'};
    my $username = $params->{username};

    # If we're using RADIUS_email_suffix, we may need to cut it off from
    # the login name.
    if ($address_suffix) {
        $username =~ s/\Q$address_suffix\E$//i;
    }

    # Create RADIUS object.
    my $radius =
        new Authen::Radius(Host   => Bugzilla->params->{'RADIUS_server'},
                           Secret => Bugzilla->params->{'RADIUS_secret'})
        || return { failure => AUTH_ERROR, error => 'radius_preparation_error',
                    details => {errstr => Authen::Radius::strerror() } };

    # Check the password.
    $radius->check_pwd($username, $params->{password},
                       Bugzilla->params->{'RADIUS_NAS_IP'} || undef)
        || return { failure => AUTH_LOGINFAILED };

    # Build the user account's e-mail address.
    $params->{bz_username} = $username . $address_suffix;

    return $params;
}

1;
