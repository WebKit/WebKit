# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Auth::Login::APIKey;

use 5.10.1;
use strict;
use warnings;

use base qw(Bugzilla::Auth::Login);

use Bugzilla::Constants;
use Bugzilla::User::APIKey;
use Bugzilla::Util;
use Bugzilla::Error;

use constant requires_persistence  => 0;
use constant requires_verification => 0;
use constant can_login             => 0;
use constant can_logout            => 0;

# This method is only available to web services. An API key can never
# be used to authenticate a Web request.
sub get_login_info {
    my ($self) = @_;
    my $params = Bugzilla->input_params;
    my ($user_id, $login_cookie);

    my $api_key_text = trim(delete $params->{'Bugzilla_api_key'});
    if (!i_am_webservice() || !$api_key_text) {
        return { failure => AUTH_NODATA };
    }

    my $api_key = Bugzilla::User::APIKey->new({ name => $api_key_text });

    if (!$api_key or $api_key->api_key ne $api_key_text) {
        # The second part checks the correct capitalisation. Silly MySQL
        ThrowUserError("api_key_not_valid");
    }
    elsif ($api_key->revoked) {
        ThrowUserError('api_key_revoked');
    }

    $api_key->update_last_used();

    return { user_id => $api_key->user_id };
}

1;
