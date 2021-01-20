# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Server::REST::Resources::Bug;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::WebService::Constants;
use Bugzilla::WebService::Bug;

BEGIN {
    *Bugzilla::WebService::Bug::rest_resources = \&_rest_resources;
};

sub _rest_resources {
    my $rest_resources = [
        qr{^/bug$}, {
            GET  => {
                method => 'search',
            },
            POST => {
                method => 'create',
                status_code => STATUS_CREATED
            }
        },
        qr{^/bug/$}, {
            GET => {
                method => 'get'
            }
        },
        qr{^/bug/([^/]+)$}, {
            GET => {
                method => 'get',
                params => sub {
                    return { ids => [ $_[0] ] };
                }
            },
            PUT => {
                method => 'update',
                params => sub {
                    return { ids => [ $_[0] ] };
                }
            }
        },
        qr{^/bug/([^/]+)/comment$}, {
            GET  => {
                method => 'comments',
                params => sub {
                    return { ids => [ $_[0] ] };
                }
            },
            POST => {
                method => 'add_comment',
                params => sub {
                    return { id => $_[0] };
                },
                success_code => STATUS_CREATED
            }
        },
        qr{^/bug/comment/([^/]+)$}, {
            GET => {
                method => 'comments',
                params => sub {
                    return { comment_ids => [ $_[0] ] };
                }
            }
        },
        qr{^/bug/comment/tags/([^/]+)$}, {
            GET => {
                method => 'search_comment_tags',
                params => sub {
                    return { query => $_[0] };
                },
            },
        },
        qr{^/bug/comment/([^/]+)/tags$}, {
            PUT => {
                method => 'update_comment_tags',
                params => sub {
                    return { comment_id => $_[0] };
                },
            },
        },
        qr{^/bug/([^/]+)/history$}, {
            GET => {
                method => 'history',
                params => sub {
                    return { ids => [ $_[0] ] };
                },
            }
        },
        qr{^/bug/([^/]+)/attachment$}, {
            GET  => {
                method => 'attachments',
                params => sub {
                    return { ids => [ $_[0] ] };
                }
            },
            POST => {
                method => 'add_attachment',
                params => sub {
                    return { ids => [ $_[0] ] };
                },
                success_code => STATUS_CREATED
            }
        },
        qr{^/bug/attachment/([^/]+)$}, {
            GET => {
                method => 'attachments',
                params => sub {
                    return { attachment_ids => [ $_[0] ] };
                }
            },
            PUT => {
                method => 'update_attachment',
                params => sub {
                    return { ids => [ $_[0] ] };
                }
            }
        },
        qr{^/field/bug$}, {
            GET => {
                method => 'fields',
            }
        },
        qr{^/field/bug/([^/]+)$}, {
            GET => {
                method => 'fields',
                params => sub {
                    my $value = $_[0];
                    my $param = 'names';
                    $param = 'ids' if $value =~ /^\d+$/;
                    return { $param => [ $_[0] ] };
                }
            }
        },
        qr{^/field/bug/([^/]+)/values$}, {
            GET => {
                method => 'legal_values',
                params => sub {
                    return { field => $_[0] };
                }
            }
        },
        qr{^/field/bug/([^/]+)/([^/]+)/values$}, {
            GET => {
                method => 'legal_values',
                params => sub {
                    return { field      => $_[0],
                             product_id => $_[1] };
                }
            }
        },
    ];
    return $rest_resources;
}

1;

__END__

=head1 NAME

Bugzilla::Webservice::Server::REST::Resources::Bug - The REST API for creating,
changing, and getting the details of bugs.

=head1 DESCRIPTION

This part of the Bugzilla REST API allows you to file a new bug in Bugzilla,
or get information about bugs that have already been filed.

See L<Bugzilla::WebService::Bug> for more details on how to use this part of
the REST API.
