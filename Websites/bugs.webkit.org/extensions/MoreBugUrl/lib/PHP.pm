# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::MoreBugUrl::PHP;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl);

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;

    # PHP Bug URLs have only one form:
    #   https://bugs.php.net/bug.php?id=1234
    return (lc($uri->authority) eq 'bugs.php.net'
            and $uri->path =~ m|/bug\.php$|
            and $uri->query_param('id') =~ /^\d+$/) ? 1 : 0;
}

sub _check_value {
    my $class = shift;

    my $uri = $class->SUPER::_check_value(@_);

    # PHP Bug URLs redirect to HTTPS, so just use the HTTPS scheme.
    $uri->scheme('https');

    # And remove any # part if there is one.
    $uri->fragment(undef);

    return $uri;
}

1;
