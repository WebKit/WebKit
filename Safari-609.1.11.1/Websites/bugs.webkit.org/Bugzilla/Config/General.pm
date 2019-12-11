# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::General;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 150;

use constant get_param_list => (
  {
   name => 'maintainer',
   type => 't',
   no_reset => '1',
   default => '',
   checker => \&check_email
  },

  {
   name => 'utf8',
   type => 'b',
   default => '0',
   checker => \&check_utf8
  },

  {
   name => 'shutdownhtml',
   type => 'l',
   default => ''
  },

  {
   name => 'announcehtml',
   type => 'l',
   default => ''
  },

  {
   name => 'upgrade_notification',
   type => 's',
   choices => ['development_snapshot', 'latest_stable_release',
               'stable_branch_release', 'disabled'],
   default => 'latest_stable_release',
   checker => \&check_notification
  },
);

1;
