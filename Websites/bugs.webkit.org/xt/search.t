#!/usr/bin/perl -w
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

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
