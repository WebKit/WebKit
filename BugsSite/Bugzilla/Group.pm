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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Joel Peshkin <bugreport@peshkin.net>
#                 Erik Stambaugh <erik@dasbistro.com>

use strict;

package Bugzilla::Group;

use Bugzilla::Config;

# ValidateGroupName checks to see if ANY of the users in the provided list 
# of user objects can see the named group.  It returns the group id if
# successful and undef otherwise.
sub ValidateGroupName {
    my ($name, @users) = (@_);
    my $dbh = Bugzilla->dbh;
    my $query = "SELECT id FROM groups " .
                "WHERE name = ?";
    if (Param('usevisibilitygroups')) {
        my @visible = (-1);
        foreach my $user (@users) {
            $user && push @visible, @{$user->visible_groups_direct};
        }
        my $visible = join(', ', @visible);
        $query .= " AND id IN($visible)";
    }
    my $sth = $dbh->prepare($query);
    $sth->execute($name);
    my ($ret) = $sth->fetchrow_array();
    return $ret;
}

1;
