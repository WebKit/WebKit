# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Server::REST::Resources::Classification;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::WebService::Constants;
use Bugzilla::WebService::Classification;

BEGIN {
    *Bugzilla::WebService::Classification::rest_resources = \&_rest_resources;
};

sub _rest_resources {
    my $rest_resources = [
        qr{^/classification/([^/]+)$}, {
            GET => {
                method => 'get',
                params => sub {
                    my $param = $_[0] =~ /^\d+$/ ? 'ids' : 'names';
                    return { $param => [ $_[0] ] };
                }
            }
        }
    ];
    return $rest_resources;
}

1;

__END__

=head1 NAME

Bugzilla::Webservice::Server::REST::Resources::Classification - The Classification REST API

=head1 DESCRIPTION

This part of the Bugzilla REST API allows you to deal with the available Classifications.
You will be able to get information about them as well as manipulate them.

See L<Bugzilla::WebService::Classification> for more details on how to use this part
of the REST API.
