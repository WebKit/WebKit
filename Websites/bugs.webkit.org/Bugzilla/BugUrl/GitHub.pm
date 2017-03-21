# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::BugUrl::GitHub;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl);

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;

    # GitHub issue URLs have only one form:
    #  https://github.com/USER_OR_TEAM_OR_ORGANIZATION_NAME/REPOSITORY_NAME/issues/111
    # GitHub pull request URLs have only one form:
    #  https://github.com/USER_OR_TEAM_OR_ORGANIZATION_NAME/REPOSITORY_NAME/pull/111
    return (lc($uri->authority) eq 'github.com'
            and $uri->path =~ m!^/[^/]+/[^/]+/(?:issues|pull)/\d+$!) ? 1 : 0;
}

sub _check_value {
    my ($class, $uri) = @_;

    $uri = $class->SUPER::_check_value($uri);

    # GitHub HTTP URLs redirect to HTTPS, so just use the HTTPS scheme.
    $uri->scheme('https');

    return $uri;
}

1;
