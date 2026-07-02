# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Server::REST::Resources::Bugzilla;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::WebService::Constants;
use Bugzilla::WebService::Bugzilla;

BEGIN {
    *Bugzilla::WebService::Bugzilla::rest_resources = \&_rest_resources;
};

sub _rest_resources {
    my $rest_resources = [
        qr{^/version$}, {
            GET  => {
                method => 'version'
            }
        },
        qr{^/extensions$}, {
            GET => {
                method => 'extensions'
            }
        },
        qr{^/timezone$}, {
            GET => {
                method => 'timezone'
            }
        },
        qr{^/time$}, {
            GET => {
                method => 'time'
            }
        },
        qr{^/last_audit_time$}, {
            GET => {
                method => 'last_audit_time'
            }
        },
        qr{^/parameters$}, {
            GET => {
                method => 'parameters'
            }
        }
    ];
    return $rest_resources;
}

1;

__END__

=head1 NAME

Bugzilla::WebService::Bugzilla - Global functions for the webservice interface.

=head1 DESCRIPTION

This provides functions that tell you about Bugzilla in general.

See L<Bugzilla::WebService::Bugzilla> for more details on how to use this part
of the REST API.
