# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::OldBugMove::Params;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 700;

use constant get_param_list => (
  {
   name => 'move-to-url',
   type => 't',
   default => ''
  },

  {
   name => 'move-to-address',
   type => 't',
   default => 'bugzilla-import'
  },

  {
   name => 'movers',
   type => 't',
   default => ''
  },
);

1;
