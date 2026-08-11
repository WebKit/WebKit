# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This test combines two field/operator combinations using AND in
# a single boolean chart.
package Bugzilla::Test::Search::AndTest;
use parent qw(Bugzilla::Test::Search::OrTest);

use Bugzilla::Test::Search::Constants;
use List::MoreUtils qw(all);

use constant type => 'AND';

#############
# Accessors #
#############

# In an AND test, bugs ARE supposed to be contained only if they are contained
# by ALL tests.
sub bug_is_contained {
    my ($self, $number) = @_;
    return all { $_->bug_is_contained($number) } $self->field_tests;
}

sub _bug_will_actually_be_contained {
    my ($self, $number) = @_;
    return all { $_->will_actually_contain_bug($number) } $self->field_tests;
}

##############################
# Bugzilla::Search arguments #
##############################

sub search_params {
    my ($self) = @_;
    my @all_params = map { $_->search_params } $self->field_tests;
    my %params;
    my $chart = 0;
    foreach my $item (@all_params) {
        $params{"field0-$chart-0"} = $item->{'field0-0-0'};
        $params{"type0-$chart-0"}  = $item->{'type0-0-0'};
        $params{"value0-$chart-0"} = $item->{'value0-0-0'};
        $chart++;
    }
    return \%params;
}

1;
