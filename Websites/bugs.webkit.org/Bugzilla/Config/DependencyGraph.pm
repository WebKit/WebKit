# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::DependencyGraph;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 800;

sub get_param_list {
  my $class = shift;
  my @param_list = (
  {
   name => 'webdotbase',
   type => 't',
   default => '',
   checker => \&check_webdotbase
  },

  {
   name => 'font_file',
   type => 't',
   default => '',
   checker => \&check_font_file
  });
  return @param_list;
}

1;
