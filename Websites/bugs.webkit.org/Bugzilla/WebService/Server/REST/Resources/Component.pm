# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Server::REST::Resources::Component;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::WebService::Constants;
use Bugzilla::WebService::Component;

use Bugzilla::Error;

BEGIN {
    *Bugzilla::WebService::Component::rest_resources = \&_rest_resources;
};

sub _rest_resources {
    my $rest_resources = [
        qr{^/component$}, {
            POST => {
                method => 'create',
                success_code => STATUS_CREATED
            }
        },
    ];
    return $rest_resources;
}

1;

__END__

=head1 NAME

Bugzilla::Webservice::Server::REST::Resources::Component - The Component REST API

=head1 DESCRIPTION

This part of the Bugzilla REST API allows you create Components.

See L<Bugzilla::WebService::Component> for more details on how to use this
part of the REST API.
