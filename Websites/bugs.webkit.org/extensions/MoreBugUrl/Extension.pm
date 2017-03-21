# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::MoreBugUrl;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Extension);

use constant MORE_SUB_CLASSES => qw(
    Bugzilla::Extension::MoreBugUrl::BitBucket
    Bugzilla::Extension::MoreBugUrl::ReviewBoard
    Bugzilla::Extension::MoreBugUrl::Rietveld
    Bugzilla::Extension::MoreBugUrl::RT
    Bugzilla::Extension::MoreBugUrl::GetSatisfaction
    Bugzilla::Extension::MoreBugUrl::PHP
    Bugzilla::Extension::MoreBugUrl::Redmine
    Bugzilla::Extension::MoreBugUrl::Savane
);

# We need to update bug_see_also table because both
# Rietveld and ReviewBoard were originally under Bugzilla/BugUrl/.
sub install_update_db {
    my $dbh = Bugzilla->dbh;

    my $should_rename = $dbh->selectrow_array(
        q{SELECT 1 FROM bug_see_also
          WHERE class IN ('Bugzilla::BugUrl::Rietveld', 
                          'Bugzilla::BugUrl::ReviewBoard')});

    if ($should_rename) {
        my $sth = $dbh->prepare('UPDATE bug_see_also SET class = ?
                                 WHERE class = ?');
        $sth->execute('Bugzilla::Extension::MoreBugUrl::ReviewBoard',
                      'Bugzilla::BugUrl::ReviewBoard');

        $sth->execute('Bugzilla::Extension::MoreBugUrl::Rietveld',
                      'Bugzilla::BugUrl::Rietveld');
    }
}

sub bug_url_sub_classes {
    my ($self, $args) = @_;
    push @{ $args->{sub_classes} }, MORE_SUB_CLASSES;
}

__PACKAGE__->NAME;
