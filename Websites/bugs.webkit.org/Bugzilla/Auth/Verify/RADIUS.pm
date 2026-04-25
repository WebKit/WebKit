# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Auth::Verify::RADIUS;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Auth::Verify);

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
