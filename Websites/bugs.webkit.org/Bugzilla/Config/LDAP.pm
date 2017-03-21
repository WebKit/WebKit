# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::LDAP;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 1000;

sub get_param_list {
  my $class = shift;
  my @param_list = (
  {
   name => 'LDAPserver',
   type => 't',
   default => ''
  },

  {
   name => 'LDAPstarttls',
   type => 'b',
   default => 0
  },

  {
   name => 'LDAPbinddn',
   type => 't',
   default => ''
  },

  {
   name => 'LDAPBaseDN',
   type => 't',
   default => ''
  },

  {
   name => 'LDAPuidattribute',
   type => 't',
   default => 'uid'
  },

  {
   name => 'LDAPmailattribute',
   type => 't',
   default => 'mail'
  },

  {
   name => 'LDAPfilter',
   type => 't',
   default => '',
  } );
  return @param_list;
}

1;
