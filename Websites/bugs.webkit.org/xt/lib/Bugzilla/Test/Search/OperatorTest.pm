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