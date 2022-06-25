# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This module represents the tests that get run on a single operator
# from the TESTS constant in Bugzilla::Search::Test::Constants.
package Bugzilla::Test::Search::OperatorTest;

use strict;
use warnings;
use Bugzilla::Test::Search::Constants;
use Bugzilla::Test::Search::FieldTest;
use Bugzilla::Test::Search::FieldTestNormal;
use Bugzilla::Test::Search::InjectionTest;
use Bugzilla::Test::Search::OrTest;
use Bugzilla::Test::Search::AndTest;
use Bugzilla::Test::Search::NotTest;

###############
# Constructor #
###############

sub new {
    my ($invocant, $operator, $search_test) = @_;
    $search_test ||= $invocant->search_test;
    my $class = ref($invocant) || $invocant;
    return bless { search_test => $search_test, operator => $operator }, $class;
}

#############
# Accessors #
#############

# The Bugzilla::Test::Search object that this is a child of.
sub search_test { return $_[0]->{search_test} }
# The operator being tested
sub operator { return $_[0]->{operator} }
# The tests that we're going to run on this operator.
sub tests { return @{ TESTS->{$_[0]->operator } } }
# The fields we're going to test for this operator.
sub test_fields { return $_[0]->search_test->all_fields }

sub run {
    my ($self) = @_;

    foreach my $field ($self->test_fields) {
        foreach my $test ($self->tests) {
            my $field_test =
                new Bugzilla::Test::Search::FieldTest($self, $field, $test);
            $field_test->run();
            my $normal_test =
                new Bugzilla::Test::Search::FieldTestNormal($field_test);
            $normal_test->run();
            my $not_test = new Bugzilla::Test::Search::NotTest($field_test);
            $not_test->run();
            
            next if !$self->search_test->option('long');

            # Run the OR tests. This tests every other operator (including
            # this operator itself) in combination with every other field,
            # in an OR with this operator and field.
            foreach my $other_operator ($self->search_test->all_operators) {
                $self->run_join_tests($field_test, $other_operator);
            }
        }
        foreach my $test (INJECTION_TESTS) {
            my $injection_test =
                new Bugzilla::Test::Search::InjectionTest($self, $field, $test);
            $injection_test->run();
        }
    }
}

sub run_join_tests {
    my ($self, $field_test, $other_operator) = @_;

    my $other_operator_test = $self->new($other_operator);
    foreach my $other_test ($other_operator_test->tests) {
        foreach my $other_field ($self->test_fields) {
            $self->_run_one_join_test($field_test, $other_operator_test,
                                      $other_field, $other_test);
            $self->search_test->clean_test_history();
        }
    }
}

sub _run_one_join_test {
    my ($self, $field_test, $other_operator_test, $other_field, $other_test) = @_;
    my $other_field_test =
        new Bugzilla::Test::Search::FieldTest($other_operator_test,
                                              $other_field, $other_test);
    my $or_test = new Bugzilla::Test::Search::OrTest($field_test,
                                                     $other_field_test);
    $or_test->run();
    my $and_test = new Bugzilla::Test::Search::AndTest($field_test,
                                                       $other_field_test);
    $and_test->run();
}

1;
