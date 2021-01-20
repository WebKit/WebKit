# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::BugUrl::Debian;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl);

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;

    # Debian BTS URLs can look like various things:
    #   http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1234
    #   http://bugs.debian.org/1234
    return (lc($uri->authority) eq 'bugs.debian.org'
            and (($uri->path =~ /bugreport\.cgi$/
                  and $uri->query_param('bug') =~ m|^\d+$|)
                 or $uri->path =~ m|^/\d+$|)) ? 1 : 0;
}

sub _check_value {
    my $class = shift;

    my $uri = $class->SUPER::_check_value(@_);

    # This is the shortest standard URL form for Debian BTS URLs,
    # and so we reduce all URLs to this.
    $uri->path =~ m|^/(\d+)$| || $uri->query_param('bug') =~ m|^(\d+)$|;
    $uri = new URI("http://bugs.debian.org/$1");

    return $uri;
}

1;
