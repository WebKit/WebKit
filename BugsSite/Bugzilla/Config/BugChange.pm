# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dawn Endico <endico@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Joe Robins <jmrobins@tgix.com>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 J. Paul Reed <preed@sigkill.com>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Joseph Heenan <joseph@heenan.me.uk>
#                 Erik Stambaugh <erik@dasbistro.com>
#                 Frédéric Buclin <LpSolit@gmail.com>
#

package Bugzilla::Config::BugChange;

use strict;

use Bugzilla::Config::Common;
use Bugzilla::Status;

$Bugzilla::Config::BugChange::sortkey = "03";

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
   name => 'commentonclearresolution',
   type => 'b',
   default => 0
  },

  {
   name => 'commentonchange_resolution',
   type => 'b',
   default => 0
  },

  {
   name => 'commentonreassignbycomponent',
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
