# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::BugUrl::Chromium;

use 5.10.1;
use strict;
use warnings;

use base qw(Bugzilla::BugUrl);

use Bugzilla::Error;
use Bugzilla::Util;

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;
    return ($uri->authority =~ /^bugs.chromium.org$/i) ? 1 : 0;
}

sub _check_value {
    my ($class, $uri) = @_;

    $uri = $class->SUPER::_check_value($uri);

    my $value = $uri->as_string;
    my $project_name;
    if ($uri->path =~ m|^/p/([^/]+)/issues/detail$|) {
        $project_name = $1;
    } else {
        ThrowUserError('bug_url_invalid', { url => $value });
    }
    my $bug_id = $uri->query_param('id');
    detaint_natural($bug_id);
    if (!$bug_id) {
        ThrowUserError('bug_url_invalid', { url => $value, reason => 'id' });
    }

    return URI->new($value);
}

1;
