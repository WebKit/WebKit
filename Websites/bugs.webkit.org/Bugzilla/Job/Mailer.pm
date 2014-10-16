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
# The Initial Developer of the Original Code is Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2008
# Mozilla Corporation. All Rights Reserved.
#
# Contributor(s): 
#   Mark Smith <mark@mozilla.com>
#   Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::Job::Mailer;
use strict;
use Bugzilla::Mailer;
BEGIN { eval "use base qw(TheSchwartz::Worker)"; }

# The longest we expect a job to possibly take, in seconds.
use constant grab_for => 300;
# We don't want email to fail permanently very easily. Retry for 30 days.
use constant max_retries => 725;

# The first few retries happen quickly, but after that we wait an hour for
# each retry.
sub retry_delay {
    my ($class, $num_retries) = @_;
    if ($num_retries < 5) {
        return (10, 30, 60, 300, 600)[$num_retries];
    }
    # One hour
    return 60*60;
}

sub work {
    my ($class, $job) = @_;
    my $msg = $job->arg->{msg};
    my $success = eval { MessageToMTA($msg, 1); 1; };
    if (!$success) {
        $job->failed($@);
        undef $@;
    } 
    else {
        $job->completed;
    }
}

1;
