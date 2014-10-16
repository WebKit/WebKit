# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Tiago Mello.
# Portions created by Tiago Mello are Copyright (C) 2010
# Tiago Mello. All Rights Reserved.
#
# Contributor(s): Tiago Mello <timello@linux.vnet.ibm.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::BugUrl::Bugzilla;
use strict;
use base qw(Bugzilla::BugUrl);

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
