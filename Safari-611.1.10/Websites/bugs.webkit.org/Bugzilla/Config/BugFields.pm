# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::BugFields;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;
use Bugzilla::Field;

our $sortkey = 600;

sub get_param_list {
  my $class = shift;

  my @legal_priorities = @{get_legal_field_values('priority')};
  my @legal_severities = @{get_legal_field_values('bug_severity')};
  my @legal_platforms  = @{get_legal_field_values('rep_platform')};
  my @legal_OS         = @{get_legal_field_values('op_sys')};

  my @param_list = (
  {
   name => 'useclassification',
   type => 'b',
   default => 0
  },

  {
   name => 'usetargetmilestone',
   type => 'b',
   default => 0
  },

  {
   name => 'useqacontact',
   type => 'b',
   default => 0
  },

  {
   name => 'usestatuswhiteboard',
   type => 'b',
   default => 0
  },

  {
   name => 'use_see_also',
   type => 'b',
   default => 1
  },

  {
   name => 'defaultpriority',
   type => 's',
   choices => \@legal_priorities,
   default => $legal_priorities[-1],
   checker => \&check_priority
  },

  {
   name => 'defaultseverity',
   type => 's',
   choices => \@legal_severities,
   default => $legal_severities[-1],
   checker => \&check_severity
  },

  {
   name => 'defaultplatform',
   type => 's',
   choices => ['', @legal_platforms],
   default => '',
   checker => \&check_platform
  },

  {
   name => 'defaultopsys',
   type => 's',
   choices => ['', @legal_OS],
   default => '',
   checker => \&check_opsys
  },

  {
   name => 'collapsed_comment_tags',
   type => 't',
   default => 'obsolete, spam',
  });
  return @param_list;
}

1;
