# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::Example::Auth::Login;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Auth::Login);
use constant user_can_create_account => 0;
use Bugzilla::Constants;

# Always returns no data.
sub get_login_info {
    return { failure => AUTH_NODATA };
}

1;
