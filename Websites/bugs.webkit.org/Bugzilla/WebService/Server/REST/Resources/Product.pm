# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Server::REST::Resources::Product;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::WebService::Constants;
use Bugzilla::WebService::Product;

use Bugzilla::Error;

BEGIN {
    *Bugzilla::WebService::Product::rest_resources = \&_rest_resources;
};

sub _rest_resources {
    my $rest_resources = [
        qr{^/product_accessible$}, {
            GET => {
                method => 'get_accessible_products'
            }
        },
        qr{^/product_enterable$}, {
            GET => {
                method => 'get_enterable_products'
            }
        },
        qr{^/product_selectable$}, {
            GET => {
                method => 'get_selectable_products'
            }
        },
        qr{^/product$}, {
            GET  => {
                method => 'get'
            },
            POST => {
                method => 'create',
                success_code => STATUS_CREATED
            }
        },
        qr{^/product/([^/]+)$}, {
            GET => {
                method => 'get',
                params => sub {
                    my $param = $_[0] =~ /^\d+$/ ? 'ids' : 'names';
                    return { $param => [ $_[0] ] };
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

Bugzilla::Webservice::Server::REST::Resources::Product - The Product REST API

=head1 DESCRIPTION

This part of the Bugzilla REST API allows you to list the available Products and
get information about them.

See L<Bugzilla::WebService::Product> for more details on how to use this part of
the REST API.
