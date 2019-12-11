# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::BugUrl::Google;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl);

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;

    # Google Code URLs only have one form:
    #   http(s)://code.google.com/p/PROJECT_NAME/issues/detail?id=1234
    return (lc($uri->authority) eq 'code.google.com'
            and $uri->path =~ m|^/p/[^/]+/issues/detail$|
            and $uri->query_param('id') =~ /^\d+$/) ? 1 : 0;
}

sub _check_value {
    my ($class, $uri) = @_;
    
    $uri = $class->SUPER::_check_value($uri);

    # While Google Code URLs can be either HTTP or HTTPS,
    # always go with the HTTP scheme, as that's the default.
    if ($uri->scheme eq 'https') {
        $uri->scheme('http');
    }

    return $uri;
}

1;
