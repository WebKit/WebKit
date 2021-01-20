# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::Core;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 100;

use constant get_param_list => (
  {
   name => 'urlbase',
   type => 't',
   default => '',
   checker => \&check_urlbase
  },

  {
   name => 'ssl_redirect',
   type => 'b',
   default => 0
  },

  {
   name => 'sslbase',
   type => 't',
   default => '',
   checker => \&check_sslbase
  },

  {
   name => 'cookiepath',
   type => 't',
   default => '/'
  },
);

1;
