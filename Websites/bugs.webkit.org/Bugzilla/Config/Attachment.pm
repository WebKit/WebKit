# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::Attachment;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 400;

sub get_param_list {
  my $class = shift;
  my @param_list = (
  {
   name => 'allow_attachment_display',
   type => 'b',
   default => 0
  },

  {
   name => 'attachment_base',
   type => 't',
   default => '',
   checker => \&check_urlbase
  },

  {
  name => 'allow_attachment_deletion',
  type => 'b',
  default => 0
  },

  {
   name => 'maxattachmentsize',
   type => 't',
   default => '1000',
   checker => \&check_maxattachmentsize
  },

  # The maximum size (in bytes) for patches and non-patch attachments.
  # The default limit is 1000KB, which is 24KB less than mysql's default
  # maximum packet size (which determines how much data can be sent in a
  # single mysql packet and thus how much data can be inserted into the
  # database) to provide breathing space for the data in other fields of
  # the attachment record as well as any mysql packet overhead (I don't
  # know of any, but I suspect there may be some.)

  {
   name => 'maxlocalattachment',
   type => 't',
   default => '0',
   checker => \&check_numeric
  } );
  return @param_list;
}

1;
