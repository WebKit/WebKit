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

package Bugzilla::Config::MTA;

use strict;

use Bugzilla::Config::Common;
# Return::Value 1.666002 pollutes the error log with warnings about this
# deprecated module. We have to set NO_CLUCK = 1 before loading Email::Send
# to disable these warnings.
BEGIN {
    $Return::Value::NO_CLUCK = 1;
}
use Email::Send;

our $sortkey = 1200;

sub get_param_list {
  my $class = shift;
  my @param_list = (
  {
   name => 'mail_delivery_method',
   type => 's',
   # Bugzilla is not ready yet to send mails to newsgroups, and 'IO'
   # is of no use for now as we already have our own 'Test' mode.
   choices => [grep {$_ ne 'NNTP' && $_ ne 'IO'} Email::Send->new()->all_mailers(), 'None'],
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
   default => 'localhost'
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
