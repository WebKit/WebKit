# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::ShadowDB;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 1500;

sub get_param_list {
  my $class = shift;
  my @param_list = (
  {
   name => 'shadowdbhost',
   type => 't',
   default => '',
  },

  {
   name => 'shadowdbport',
   type => 't',
   default => '3306',
   checker => \&check_numeric,
  },

  {
   name => 'shadowdbsock',
   type => 't',
   default => '',
  },

  # This entry must be _after_ the shadowdb{host,port,sock} settings so that
  # they can be used in the validation here
  {
   name => 'shadowdb',
   type => 't',
   default => '',
   checker => \&check_shadowdb
  } );
  return @param_list;
}

1;
