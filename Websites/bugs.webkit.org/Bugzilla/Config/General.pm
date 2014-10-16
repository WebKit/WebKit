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
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::Config::General;
use strict;
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
   name => 'docs_urlbase',
   type => 't',
   default => 'docs/%lang%/html/',
   checker => \&check_url
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
