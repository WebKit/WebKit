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
# The Initial Developer of the Original Code is Tiago Mello
# Portions created by Tiago Mello are Copyright (C) 2011
# Tiago Mello. All Rights Reserved.
#
# Contributor(s): Tiago Mello <timello@linux.vnet.ibm.com>

package Bugzilla::BugUrl::SourceForge;
use strict;
use base qw(Bugzilla::BugUrl);

use Bugzilla::Error;
use Bugzilla::Util;

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;
    return ($uri->authority =~ /^sourceforge.net$/i
            and $uri->path =~ m|/tracker/|) ? 1 : 0;
}

sub _check_value {
    my $class = shift;

    my $uri = $class->SUPER::_check_value(@_);

    # SourceForge tracker URLs have only one form:
    #  http://sourceforge.net/tracker/?func=detail&aid=111&group_id=111&atid=111
    if ($uri->query_param('func') eq 'detail' and $uri->query_param('aid')
        and $uri->query_param('group_id') and $uri->query_param('atid'))
    {
        # Remove any # part if there is one.
        $uri->fragment(undef);
        return $uri;
    }
    else {
        my $value = $uri->as_string;
        ThrowUserError('bug_url_invalid', { url => $value });
    }
}

1;
