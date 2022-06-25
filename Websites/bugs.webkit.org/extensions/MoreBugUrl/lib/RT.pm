# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::MoreBugUrl::RT;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl);

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;

    # RT URLs can look like various things:
    #   http://example.com/rt/Ticket/Display.html?id=1234
    #   https://example.com/Public/Bug/Display.html?id=1234
    return ($uri->path =~ m|/Display\.html$|
            and $uri->query_param('id') =~ /^\d+$/) ? 1 : 0;
}

sub _check_value {
    my $class = shift;

    my $uri = $class->SUPER::_check_value(@_);

    # And remove any # part if there is one.
    $uri->fragment(undef);

    return $uri;
}

1;
