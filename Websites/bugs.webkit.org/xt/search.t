#!/usr/bin/env perl -w
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
# The Initial Developer of the Original Code is Everything Solved, Inc.
# Portions created by the Initial Developer are Copyright (C) 2010 the
# Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Max Kanat-Alexander <mkanat@bugzilla.org>

# For a description of this test, see Bugzilla::Test::Search
# in xt/lib/.

use strict;
use warnings;
use lib qw(. xt/lib lib);
use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Test::Search;
use Getopt::Long;
use Pod::Usage;

use Test::More;

my %switches;
GetOptions(\%switches, 'operators=s', 'top-operators=s', 'long',
                       'add-custom-fields', 'help|h') || die $@;

pod2usage(verbose => 1) if $switches{'help'};

plan skip_all => "BZ_WRITE_TESTS environment variable not set"
  if !$ENV{BZ_WRITE_TESTS};

Bugzilla->usage_mode(USAGE_MODE_TEST);

my $test = new Bugzilla::Test::Search(\%switches);
plan tests => $test->num_tests;
$test->run();

__END__

=head1 NAME

search.t - Test L<Bugzilla::Search>

=head1 DESCRIPTION

This test tests L<Bugzilla::Search>.

Note that users may be prevented from writing new bugs, products, components,
etc. to your database while this test is running.

=head1 OPTIONS

=over

=item --long

Run AND and OR tests in addition to normal tests. Specifying
--long without also specifying L</--top-operators> is likely to
run your system out of memory.

=item --add-custom-fields

This adds every type of custom field to the database, so that they can
all be tested. Note that this B<CANNOT BE REVERSED>, so do not use this
switch on a production installation.

=item --operators=a,b,c

Limit the test to testing only the listed operators.

=item --top-operators=a,b,c

Limit the top-level tested operators to the following list. This
means that for normal tests, only the listed operators will be tested.
However, for OR and AND tests, all other operators will be tested
along with the operators you listed.

=item --help

Display this help.

=back
