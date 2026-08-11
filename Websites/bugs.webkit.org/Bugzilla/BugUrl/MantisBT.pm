# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::BugUrl::MantisBT;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl);

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;

    # MantisBT URLs look like the following ('bugs' directory is optional):
    #   http://www.mantisbt.org/bugs/view.php?id=1234
    return ($uri->path_query =~ m|view\.php\?id=\d+$|) ? 1 : 0;
}

sub _check_value {
    my $class = shift;

    my $uri = $class->SUPER::_check_value(@_);

    # Remove any # part if there is one.
    $uri->fragment(undef);

    return $uri;
}

1;
