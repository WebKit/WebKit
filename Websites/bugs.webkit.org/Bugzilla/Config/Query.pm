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

package Bugzilla::Config::Query;

use strict;

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
   name => 'mostfreqthreshold',
   type => 't',
   default => '2',
   checker => \&check_numeric
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
