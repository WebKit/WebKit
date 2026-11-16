# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::BugUrl::Launchpad;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl);

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;

    # Launchpad bug URLs can look like various things:
    #   https://bugs.launchpad.net/ubuntu/+bug/1234
    #   https://launchpad.net/bugs/1234
    # All variations end with either "/bugs/1234" or "/+bug/1234"
    return ($uri->authority =~ /launchpad\.net$/
            and $uri->path =~ m|bugs?/\d+$|) ? 1 : 0;
}

sub _check_value {
    my ($class, $uri) = @_;

    $uri = $class->SUPER::_check_value($uri);

    # This is the shortest standard URL form for Launchpad bugs,
    # and so we reduce all URLs to this.
    $uri->path =~ m|bugs?/(\d+)$|;
    $uri = new URI("https://launchpad.net/bugs/$1");

    return $uri;
}

1;
