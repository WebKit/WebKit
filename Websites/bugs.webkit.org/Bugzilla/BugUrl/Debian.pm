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

package Bugzilla::BugUrl::Debian;
use strict;
use base qw(Bugzilla::BugUrl);

use Bugzilla::Error;
use Bugzilla::Util;

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;
    return ($uri->authority =~ /^bugs.debian.org$/i) ? 1 : 0;
}

sub _check_value {
    my $class = shift;

    my $uri = $class->SUPER::_check_value(@_);

    # Debian BTS URLs can look like various things:
    #   http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1234
    #   http://bugs.debian.org/1234
    my $bug_id;
    if ($uri->path =~ m|^/(\d+)$|) {
        $bug_id = $1;
    }
    elsif ($uri->path =~ /bugreport\.cgi$/) {
        $bug_id = $uri->query_param('bug');
        detaint_natural($bug_id);
    }
    if (!$bug_id) {
        ThrowUserError('bug_url_invalid',
                       { url => $uri->path, reason => 'id' });
    }
    # This is the shortest standard URL form for Debian BTS URLs,
    # and so we reduce all URLs to this.
    return new URI("http://bugs.debian.org/" . $bug_id);
}

1;
