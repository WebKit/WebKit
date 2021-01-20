# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::MTA;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 1200;

sub get_param_list {
  my $class = shift;
  my @param_list = (
  {
   name => 'mail_delivery_method',
   type => 's',
   choices => ['Sendmail', 'SMTP', 'Test', 'None'],
   default => 'Sendmail',
   checker => \&check_mail_delivery_method
  },

  {
   name => 'mailfrom',
   type => 't',
   default => 'bugzilla-daemon'
  },

  {
   name => 'use_mailer_queue',
   type => 'b',
   default => 0,
   checker => \&check_theschwartz_available,
  },

  {
   name => 'smtpserver',
   type => 't',
   default => 'localhost',
   checker => \&check_smtp_server
  },
  {
   name => 'smtp_username',
   type => 't',
   default => '',
   checker => \&check_smtp_auth
  },
  {
   name => 'smtp_password',
   type => 'p',
   default => ''
  },
  {
   name => 'smtp_ssl',
   type => 'b',
   default => 0,
   checker => \&check_smtp_ssl
  },
  {
   name => 'smtp_debug',
   type => 'b',
   default => 0
  },
  {
   name => 'whinedays',
   type => 't',
   default => 7,
   checker => \&check_numeric
  },
  {
   name => 'globalwatchers',
   type => 't',
   default => '',
  }, );
  return @param_list;
}

1;
