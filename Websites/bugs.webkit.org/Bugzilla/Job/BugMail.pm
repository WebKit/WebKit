# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Job::BugMail;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::BugMail;
BEGIN { eval "use parent qw(Bugzilla::Job::Mailer)"; }

sub work {
    my ($class, $job) = @_;
    my $success = eval {
        Bugzilla::BugMail::dequeue($job->arg->{vars});
        1;
    };
    if (!$success) {
        $job->failed($@);
        undef $@;
    }
    else {
        $job->completed;
    }
}

1;
