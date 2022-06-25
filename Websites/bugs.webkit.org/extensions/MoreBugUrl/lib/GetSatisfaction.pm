# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::MoreBugUrl::GetSatisfaction;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl);

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;

    # GetSatisfaction URLs only have one form:
    #   http(s)://getsatisfaction.com/PROJECT_NAME/topics/TOPIC_NAME
    return (lc($uri->authority) eq 'getsatisfaction.com'
            and $uri->path =~ m|^/[^/]+/topics/[^/]+$|) ? 1 : 0;
}

sub _check_value {
    my ($class, $uri) = @_;

    $uri = $class->SUPER::_check_value($uri);

    # GetSatisfaction HTTP URLs redirect to HTTPS, so just use the HTTPS
    # scheme.
    $uri->scheme('https');

    return $uri;
}

1;
