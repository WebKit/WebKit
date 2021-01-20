# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::GroupSecurity;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;
use Bugzilla::Group;

our $sortkey = 900;

sub get_param_list {
  my $class = shift;

  my @param_list = (
  {
   name => 'makeproductgroups',
   type => 'b',
   default => 0
  },

  {
   name => 'chartgroup',
   type => 's',
   choices => \&_get_all_group_names,
   default => 'editbugs',
   checker => \&check_group
  },

  {
   name => 'insidergroup',
   type => 's',
   choices => \&_get_all_group_names,
   default => '',
   checker => \&check_group
  },

  {
   name => 'timetrackinggroup',
   type => 's',
   choices => \&_get_all_group_names,
   default => 'editbugs',
   checker => \&check_group
  },

  {
   name => 'querysharegroup',
   type => 's',
   choices => \&_get_all_group_names,
   default => 'editbugs',
   checker => \&check_group
  },

  {
   name => 'comment_taggers_group',
   type => 's',
   choices => \&_get_all_group_names,
   default => 'editbugs',
   checker => \&check_comment_taggers_group
  },

  {
   name => 'debug_group',
   type => 's',
   choices => \&_get_all_group_names,
   default => 'admin',
   checker => \&check_group
  },
  
  {
   name => 'usevisibilitygroups',
   type => 'b',
   default => 0
  }, 
  
  {
   name => 'strict_isolation',
   type => 'b',
   default => 0
  },

  {
   name => 'or_groups',
   type => 'b',
   default => 0
  } );
  return @param_list;
}

sub _get_all_group_names {
    my @group_names = map {$_->name} Bugzilla::Group->get_all;
    unshift(@group_names, '');
    return \@group_names;
}

1;
