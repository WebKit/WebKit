# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config::Auth;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Config::Common;

our $sortkey = 300;

sub get_param_list {
  my $class = shift;
  my @param_list = (
  {
   name => 'auth_env_id',
   type    => 't',
   default => '',
  },

  {
   name    => 'auth_env_email',
   type    => 't',
   default => '',
  },

  {
   name    => 'auth_env_realname',
   type    => 't',
   default => '',
  },

  # XXX in the future:
  #
  # user_verify_class and user_info_class should have choices gathered from
  # whatever sits in their respective directories
  #
  # rather than comma-separated lists, these two should eventually become
  # arrays, but that requires alterations to editparams first

  {
   name => 'user_info_class',
   type => 's',
   choices => [ 'CGI', 'Env', 'Env,CGI' ],
   default => 'CGI',
   checker => \&check_multi
  },

  {
   name => 'user_verify_class',
   type => 'o',
   choices => [ 'DB', 'RADIUS', 'LDAP' ],
   default => 'DB',
   checker => \&check_user_verify_class
  },

  {
   name => 'rememberlogin',
   type => 's',
   choices => ['on', 'defaulton', 'defaultoff', 'off'],
   default => 'on',
   checker => \&check_multi
  },

  {
   name => 'requirelogin',
   type => 'b',
   default => '0'
  },

  {
   name => 'webservice_email_filter',
   type => 'b',
   default => 0
  },

  {
   name => 'emailregexp',
   type => 't',
   default => q:^[\\w\\.\\+\\-=']+@[\\w\\.\\-]+\\.[\\w\\-]+$:,
   checker => \&check_regexp
  },

  {
   name => 'emailregexpdesc',
   type => 'l',
   default => 'A legal address must contain exactly one \'@\', and at least ' .
              'one \'.\' after the @.'
  },

  {
   name => 'emailsuffix',
   type => 't',
   default => ''
  },

  {
   name => 'createemailregexp',
   type => 't',
   default => q:.*:,
   checker => \&check_regexp
  },

  # WEBKIT_CHANGES
  {
   name => 'password_complexity',
   type => 's',
   choices => [ 'no_constraints', 'mixed_letters', 'letters_numbers',
                'letters_numbers_specialchars', 'zxcvbn_password_checker' ],
   default => 'no_constraints',
   checker => \&check_multi
  },

  {
   name => 'password_check_on_login',
   type => 'b',
   default => '1'
  },
  );
  return @param_list;
}

1;
