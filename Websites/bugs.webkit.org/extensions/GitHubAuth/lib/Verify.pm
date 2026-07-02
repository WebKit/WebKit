# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::GitHubAuth::Verify;

use 5.10.1;
use strict;
use warnings;

use base qw(Bugzilla::Auth::Verify);

use Bugzilla::Constants qw( AUTH_NO_SUCH_USER );

sub check_credentials {
    my ($self, $login_data) = @_;

    return { failure => AUTH_NO_SUCH_USER } unless $login_data->{github_auth};

    return $login_data;
}

1;
