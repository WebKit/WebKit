# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::Example::WebService;

use 5.10.1;
use strict;
use warnings;
use parent qw(Bugzilla::WebService);
use Bugzilla::Error;

use constant PUBLIC_METHODS => qw(
    hello
    throw_an_error
);

# This can be called as Example.hello() from the WebService.
sub hello { return 'Hello!'; }

sub throw_an_error { ThrowUserError('example_my_error') }

1;
