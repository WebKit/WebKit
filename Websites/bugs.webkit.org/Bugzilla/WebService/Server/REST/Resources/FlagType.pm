# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Server::REST::Resources::FlagType;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::WebService::Constants;
use Bugzilla::WebService::FlagType;

use Bugzilla::Error;

BEGIN {
    *Bugzilla::WebService::FlagType::rest_resources = \&_rest_resources;
};

sub _rest_resources {
    my $rest_resources = [
        qr{^/flag_type$}, {
            POST => {
                method => 'create',
                success_code => STATUS_CREATED
            }
        },
        qr{^/flag_type/([^/]+)/([^/]+)$}, {
            GET => {
                method => 'get',
                params => sub {
                    return { product   => $_[0],
                             component => $_[1] };
                }
            }
        },
        qr{^/flag_type/([^/]+)$}, {
            GET => {
                method => 'get',
                params => sub {
                    return { product => $_[0] };
                }
            },
            PUT => {
                method => 'update',
                params => sub {
                    my $param = $_[0] =~ /^\d+$/ ? 'ids' : 'names';
                    return { $param => [ $_[0] ] };
                }
            }
        },
    ];
    return $rest_resources;
}

1;

__END__

=head1 NAME

Bugzilla::Webservice::Server::REST::Resources::FlagType - The Flag Type REST API

=head1 DESCRIPTION

This part of the Bugzilla REST API allows you to create and update Flag types.

See L<Bugzilla::WebService::FlagType> for more details on how to use this
part of the REST API.
