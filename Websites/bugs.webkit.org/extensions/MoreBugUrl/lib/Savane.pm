# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::MoreBugUrl::Savane;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl);

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;
    return ($uri->as_string =~ m|/bugs/(index\.php)?\?\d+$|) ? 1 : 0;
}

sub _check_value {
    my $class = shift;

    my $uri = $class->SUPER::_check_value(@_);

    # Savane URLs have only two forms:
    #   http://gna.org/bugs/index.php?12345
    #   http://gna.org/bugs/?12345

    # And remove any # part if there is one.
    $uri->fragment(undef);

    return $uri;
}

1;
