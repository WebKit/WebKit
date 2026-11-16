# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::BugUrl::Bugzilla;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl);

use Bugzilla::Error;
use Bugzilla::Util;

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;
    return ($uri->path =~ /show_bug\.cgi$/) ? 1 : 0;
}

sub _check_value {
    my ($class, $uri) = @_;

    $uri = $class->SUPER::_check_value($uri);

    my $bug_id = $uri->query_param('id');
    # We don't currently allow aliases, because we can't check to see
    # if somebody's putting both an alias link and a numeric ID link.
    # When we start validating the URL by accessing the other Bugzilla,
    # we can allow aliases.
    detaint_natural($bug_id);
    if (!$bug_id) {
        my $value = $uri->as_string;
        ThrowUserError('bug_url_invalid', { url => $value, reason => 'id' });
    }

    # Make sure that "id" is the only query parameter.
    $uri->query("id=$bug_id");
    # And remove any # part if there is one.
    $uri->fragment(undef);

    return $uri;
}

sub target_bug_id {
    my ($self) = @_;
    return new URI($self->name)->query_param('id');
}

1;
