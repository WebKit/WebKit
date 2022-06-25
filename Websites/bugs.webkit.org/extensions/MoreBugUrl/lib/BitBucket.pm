# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::MoreBugUrl::BitBucket;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl);

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;

    # BitBucket issues have the form of
    # bitbucket.org/user/project/issue/1234
    return (lc($uri->authority) eq "bitbucket.org"
            && $uri->path =~ m|[^/]+/[^/]+/issue/\d+|i) ? 1 : 0;
}

sub _check_value {
    my $class = shift;

    my $uri = $class->SUPER::_check_value(@_);

    my ($path) = $uri->path =~ m|([^/]+/[^/]+/issue/\d+)|i;
    $uri = new URI("https://bitbucket.org/$path");

    return $uri;
}

1;

