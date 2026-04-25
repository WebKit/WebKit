# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::Admin;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 200;

sub get_param_list {
  my $class = shift;
  my @param_list = (
  {
   name => 'allowbugdeletion',
   type => 'b',
   default => 0
  },

  {
   name => 'allowemailchange',
   type => 'b',
   default => 1
  },

  {
   name => 'allowuserdeletion',
   type => 'b',
   default => 0
  },

  {
   name => 'last_visit_keep_days',
   type => 't',
   default => 10,
   checker => \&check_numeric
  });
  return @param_list;
}

1;
