# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::RADIUS;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 1100;

sub get_param_list {
  my $class = shift;
  my @param_list = (
  {
   name =>    'RADIUS_server',
   type =>    't',
   default => ''
  },

  {
   name =>    'RADIUS_secret',
   type =>    't',
   default => ''
  },

  {
   name =>    'RADIUS_NAS_IP',
   type =>    't',
   default => ''
  },

  {
   name =>    'RADIUS_email_suffix',
   type =>    't',
   default => ''
  },
  );
  return @param_list;
}

1;
