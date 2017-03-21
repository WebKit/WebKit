# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Auth::Login::Env;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Auth::Login);

use Bugzilla::Constants;
use Bugzilla::Error;

use constant can_logout => 0;
use constant can_login  => 0;
use constant requires_persistence  => 0;
use constant requires_verification => 0;
use constant is_automatic => 1;
use constant extern_id_used => 1;

sub get_login_info {
    my ($self) = @_;

    my $env_id       = $ENV{Bugzilla->params->{"auth_env_id"}} || '';
    my $env_email    = $ENV{Bugzilla->params->{"auth_env_email"}} || '';
    my $env_realname = $ENV{Bugzilla->params->{"auth_env_realname"}} || '';

    return { failure => AUTH_NODATA } if !$env_email;

    return { username => $env_email, extern_id => $env_id, 
             realname => $env_realname };
}

sub fail_nodata {
    ThrowCodeError('env_no_email');
}

1;
