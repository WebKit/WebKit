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
# The Original Code is the Bugzilla Example Plugin.
#
# The Initial Developer of the Original Code is Everything Solved, Inc.
# Portions created by Everything Solved are Copyright (C) 2008 
# Everything Solved, Inc. All Rights Reserved.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

use strict;
use warnings;
use Bugzilla;
use Bugzilla::Status;

# This code doesn't actually *do* anything, it's just here to show you
# how to use this hook.
my $args = Bugzilla->hook_args;
my $bug = $args->{'bug'};
my $timestamp = $args->{'timestamp'};
my $changes = $args->{'changes'};

foreach my $field (keys %$changes) {
    my $used_to_be = $changes->{$field}->[0];
    my $now_it_is  = $changes->{$field}->[1];
}

my $status_message;
if (my $status_change = $changes->{'bug_status'}) {
    my $old_status = new Bugzilla::Status({ name => $status_change->[0] });
    my $new_status = new Bugzilla::Status({ name => $status_change->[1] });
    if ($new_status->is_open && !$old_status->is_open) {
        $status_message = "Bug re-opened!";
    }
    if (!$new_status->is_open && $old_status->is_open) {
        $status_message = "Bug closed!";
    }
}

my $bug_id = $bug->id;
my $num_changes = scalar keys %$changes;
my $result = "There were $num_changes changes to fields on bug $bug_id"
             . " at $timestamp.";
# Uncomment this line to see $result in your webserver's error log whenever
# you update a bug.
# warn $result;
