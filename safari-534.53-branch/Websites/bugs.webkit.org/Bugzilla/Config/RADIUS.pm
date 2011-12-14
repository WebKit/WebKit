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
# The Initial Developer of the Original Code is Marc Schumann.
# Portions created by Marc Schumann are Copyright (c) 2007 Marc Schumann.
# All rights reserved.
#
# Contributor(s): Marc Schumann <wurblzap@gmail.com>
#

package Bugzilla::Config::RADIUS;

use strict;

use Bugzilla::Config::Common;

$Bugzilla::Config::RADIUS::sortkey = "09";

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
