# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::MoreBugUrl::ReviewBoard;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl);

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;
    return ($uri->path =~ m|/r/\d+/?$|) ? 1 : 0;
}

sub _check_value {
    my $class = shift;

    my $uri = $class->SUPER::_check_value(@_);

    # Review Board URLs have only one form (the trailing slash is optional):
    #   http://reviews.reviewboard.org/r/111/

    # Make sure there are no query parameters.
    $uri->query(undef);
    # And remove any # part if there is one.
    $uri->fragment(undef);

    # make sure the trailing slash is present
    if ($uri->path !~ m|/$|) {
        $uri->path($uri->path . '/');
    }

    return $uri;
}

1;
