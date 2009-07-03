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
use Bugzilla::Util qw(diff_arrays);

# This code doesn't actually *do* anything, it's just here to show you
# how to use this hook.
my $args = Bugzilla->hook_args;
my ($bug, $timestamp, $old_flags, $new_flags) = 
    @$args{qw(bug timestamp old_flags new_flags)};
my ($removed, $added) = diff_arrays($old_flags, $new_flags);
my ($granted, $denied) = (0, 0);
foreach my $new_flag (@$added) {
    $granted++ if $new_flag =~ /\+$/;
    $denied++ if $new_flag =~ /-$/;
}
my $bug_id = $bug->id;
my $result = "$granted flags were granted and $denied flags were denied"
             . " on bug $bug_id at $timestamp.";
# Uncomment this line to see $result in your webserver's error log whenever
# you update flags.
# warn $result;
