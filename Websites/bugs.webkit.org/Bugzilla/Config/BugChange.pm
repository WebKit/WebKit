# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::BugChange;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;
use Bugzilla::Status;

our $sortkey = 500;

sub get_param_list {
  my $class = shift;

  # Hardcoded bug statuses which existed before Bugzilla 3.1.
  my @closed_bug_statuses = ('RESOLVED', 'VERIFIED', 'CLOSED');

  # If we are upgrading from 3.0 or older, bug statuses are not customisable
  # and bug_status.is_open is not yet defined (hence the eval), so we use
  # the bug statuses above as they are still hardcoded.
  eval {
      my @current_closed_states = map {$_->name} closed_bug_statuses();
      # If no closed state was found, use the default list above.
      @closed_bug_statuses = @current_closed_states if scalar(@current_closed_states);
  };

  my @param_list = (
  {
   name => 'duplicate_or_move_bug_status',
   type => 's',
   choices => \@closed_bug_statuses,
   default => $closed_bug_statuses[0],
   checker => \&check_bug_status
  },

  {
   name => 'letsubmitterchoosepriority',
   type => 'b',
   default => 1
  },

  {
   name => 'letsubmitterchoosemilestone',
   type => 'b',
   default => 1
  },

  {
   name => 'musthavemilestoneonaccept',
   type => 'b',
   default => 0
  },

  {
   name => 'commentonchange_resolution',
   type => 'b',
   default => 0
  },

  {
   name => 'commentonduplicate',
   type => 'b',
   default => 0
  },

  {
   name    => 'noresolveonopenblockers',
   type    => 'b',
   default => 0,
  } );
  return @param_list;
}

1;
