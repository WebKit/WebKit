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

package Bugzilla::BugUrl::Launchpad;
use strict;
use base qw(Bugzilla::BugUrl);

use Bugzilla::Error;

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;
    return ($uri->authority =~ /launchpad.net$/) ? 1 : 0;
}

sub _check_value {
    my ($class, $uri) = @_;

    $uri = $class->SUPER::_check_value($uri);

    my $value = $uri->as_string;
    # Launchpad bug URLs can look like various things:
    #   https://bugs.launchpad.net/ubuntu/+bug/1234
    #   https://launchpad.net/bugs/1234
    # All variations end with either "/bugs/1234" or "/+bug/1234"
    if ($uri->path =~ m|bugs?/(\d+)$|) {
        # This is the shortest standard URL form for Launchpad bugs,
        # and so we reduce all URLs to this.
        $value = "https://launchpad.net/bugs/$1";
    }
    else {
        ThrowUserError('bug_url_invalid', { url => $value, reason => 'id' });
    }

    return new URI($value);
}

1;
