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
# The Initial Developer of the Original Code is Reed Loden.
# Portions created by Reed Loden are Copyright (C) 2010
# Reed Loden. All Rights Reserved.
#
# Contributor(s): Reed Loden <reed@reedloden.com>

package Bugzilla::BugUrl::MantisBT;
use strict;
use base qw(Bugzilla::BugUrl);

use Bugzilla::Error;
use Bugzilla::Util;

###############################
####        Methods        ####
###############################

sub should_handle {
    my ($class, $uri) = @_;
    return ($uri->path_query =~ m|view\.php\?id=\d+$|) ? 1 : 0;
}

sub _check_value {
    my $class = shift;

    my $uri = $class->SUPER::_check_value(@_);

    # MantisBT URLs look like the following ('bugs' directory is optional):
    #   http://www.mantisbt.org/bugs/view.php?id=1234

    # Remove any # part if there is one.
    $uri->fragment(undef);

    return $uri;
}

1;
