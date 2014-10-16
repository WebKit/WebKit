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
#                 Reed Loden <reed@reedloden.com>

package Bugzilla::BugUrl::Google;
use strict;
use base qw(Bugzilla::BugUrl);

use Bugzilla::Error;
use Bugzilla::Util;

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;
    return ($uri->authority =~ /^code.google.com$/i) ? 1 : 0;
}

sub _check_value {
    my ($class, $uri) = @_;
    
    $uri = $class->SUPER::_check_value($uri);

    my $value = $uri->as_string;
    # Google Code URLs only have one form:
    #   http(s)://code.google.com/p/PROJECT_NAME/issues/detail?id=1234
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
    # While Google Code URLs can be either HTTP or HTTPS,
    # always go with the HTTP scheme, as that's the default.
    $value = "http://code.google.com/p/" . $project_name .
             "/issues/detail?id=" . $bug_id;

    return new URI($value);
}

1;
