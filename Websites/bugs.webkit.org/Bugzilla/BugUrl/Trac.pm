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
# The Initial Developer of the Original Code is Matt Selsky
# Portions created by Matt Selsky are Copyright (C) 2010
# Matt Selsky. All Rights Reserved.
#
# Contributor(s): Matt Selsky <selsky@columbia.edu>

package Bugzilla::BugUrl::Trac;
use strict;
use base qw(Bugzilla::BugUrl);

use Bugzilla::Error;
use Bugzilla::Util;

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;
    return ($uri->path =~ m|/ticket/\d+$|) ? 1 : 0;
}

sub _check_value {
    my $class = shift;

    my $uri = $class->SUPER::_check_value(@_);

    # Trac URLs can look like various things:
    #   http://dev.mutt.org/trac/ticket/1234
    #   http://trac.roundcube.net/ticket/1484130

    # Make sure there are no query parameters.
    $uri->query(undef);
    # And remove any # part if there is one.
    $uri->fragment(undef);

    return $uri;
}

1;
