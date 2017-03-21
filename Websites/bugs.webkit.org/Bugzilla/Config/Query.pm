# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::Query;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 1400;

sub get_param_list {
  my $class = shift;
  my @param_list = (
  {
   name => 'quip_list_entry_control',
   type => 's',
   choices => ['open', 'moderated', 'closed'],
   default => 'open',
   checker => \&check_multi
  },

  {
   name => 'mybugstemplate',
   type => 't',
   default => 'buglist.cgi?resolution=---&amp;emailassigned_to1=1&amp;emailreporter1=1&amp;emailtype1=exact&amp;email1=%userid%'
  },

  {
   name => 'defaultquery',
   type => 't',
   default => 'resolution=---&emailassigned_to1=1&emailassigned_to2=1&emailreporter2=1&emailcc2=1&emailqa_contact2=1&emaillongdesc3=1&order=Importance&long_desc_type=substring'
  },

  {
   name => 'search_allow_no_criteria',
   type => 'b',
   default => 1
  },

  {
    name => 'default_search_limit',
    type => 't',
    default => '500',
    checker => \&check_numeric
  },

  {
    name => 'max_search_results',
    type => 't',
    default => '10000',
    checker => \&check_numeric
  },
  );
  return @param_list;
}

1;
