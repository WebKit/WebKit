# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::UserMatch;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 1600;

sub get_param_list {
  my $class = shift;
  my @param_list = (
  {
   name => 'usemenuforusers',
   type => 'b',
   default => '0'
  },

  {
   name    => 'ajax_user_autocompletion', 
   type    => 'b', 
   default => '1', 
  },

  {
   name    => 'maxusermatches',
   type    => 't',
   default => '1000',
   checker => \&check_numeric
  },

  {
   name    => 'confirmuniqueusermatch',
   type    => 'b',
   default => 1,
  } );
  return @param_list;
}

1;
