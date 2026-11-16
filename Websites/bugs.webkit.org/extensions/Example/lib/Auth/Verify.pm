# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::Example::Auth::Verify;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Auth::Verify);
use Bugzilla::Constants;

# A verifier that always fails.
sub check_credentials {
    return { failure => AUTH_NO_SUCH_USER };
}

1;
