# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::Example;

use 5.10.1;
use strict;
use warnings;

use constant NAME => 'Example';
use constant REQUIRED_MODULES => [
    {
        package => 'Data-Dumper',
        module  => 'Data::Dumper',
        version => 0,
    },
];

use constant OPTIONAL_MODULES => [
    {
        package => 'Acme',
        module  => 'Acme',
        version => 1.11,
        feature => ['example_acme'],
    },
];

__PACKAGE__->NAME;
