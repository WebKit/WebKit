# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::MoreBugUrl::Rietveld;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl);

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;
    return ($uri->authority =~ /\.appspot\.com$/i
            and $uri->path =~ m#^/\d+(?:/|/show)?$#) ? 1 : 0;
}

sub _check_value {
    my ($class, $uri) = @_;

    $uri = $class->SUPER::_check_value($uri);

    # Rietveld URLs have three forms:
    #   http(s)://example.appspot.com/1234
    #   http(s)://example.appspot.com/1234/
    #   http(s)://example.appspot.com/1234/show
    if ($uri->path =~ m#^/(\d+)(?:/|/show)$#) {
        # This is the shortest standard URL form for Rietveld issues,
        # and so we reduce all URLs to this.
        $uri->path('/' . $1);
    }

    # Make sure there are no query parameters.
    $uri->query(undef);
    # And remove any # part if there is one.
    $uri->fragment(undef);

    return $uri;
}

1;
