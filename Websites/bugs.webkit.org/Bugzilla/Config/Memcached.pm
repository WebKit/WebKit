# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::Memcached;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 1550;

sub get_param_list {
  return (
    {
        name    => 'memcached_servers',
        type    => 't',
        default => ''
    },
    {
        name    => 'memcached_namespace',
        type    => 't',
        default => 'bugzilla:',
    },
  );
}

1;
