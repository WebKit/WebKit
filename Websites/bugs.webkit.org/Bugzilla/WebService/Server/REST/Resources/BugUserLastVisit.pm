# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Server::REST::Resources::BugUserLastVisit;

use 5.10.1;
use strict;
use warnings;

BEGIN {
    *Bugzilla::WebService::BugUserLastVisit::rest_resources = \&_rest_resources;
}

sub _rest_resources {
    return [
        # bug-id
        qr{^/bug_user_last_visit/(\d+)$}, {
            GET => {
                method => 'get',
                params => sub {
                    return { ids => [$_[0]] };
                },
            },
            POST => {
                method => 'update',
                params => sub {
                    return { ids => [$_[0]] };
                },
            },
        },
    ];
}

1;
__END__

=head1 NAME

Bugzilla::Webservice::Server::REST::Resources::BugUserLastVisit - The
BugUserLastVisit REST API

=head1 DESCRIPTION

This part of the Bugzilla REST API allows you to lookup and update the last time
a user visited a bug.

See L<Bugzilla::WebService::BugUserLastVisit> for more details on how to use
this part of the REST API.
