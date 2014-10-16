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

# This module runs tests just like a normal FieldTest, AndTest,
# or OrTest, but in a NOT chart instead of a normal chart.
#
# Logically this should be a mixin of some sort so that we can apply
# it to OrTest and AndTest, but without Moose there isn't much of an
# easy way to do that.
package Bugzilla::Test::Search::NotTest;
use base qw(Bugzilla::Test::Search::FieldTest);
use strict;
use warnings;
use Bugzilla::Test::Search::Constants;

# We just clone a FieldTest because that's the best for performance,
# overall--that way we don't have to translate the value again.
sub new {
    my ($class, $field_test) = @_;
    my $self = { %$field_test };
    return bless $self, $class;
}

#############
# Accessors #
#############

sub name {
    my ($self) = @_;
    return "NOT(" . $self->SUPER::name . ")";
}

# True if this test is supposed to contain the numbered bug. Reversed for
# NOT tests.
sub bug_is_contained {
    my $self = shift;
    my ($number) = @_;
    # No search ever returns bug 6, because it's protected by security groups
    # that the searcher isn't a member of.
    return 0 if $number == 6;
    return $self->SUPER::bug_is_contained(@_) ? 0 : 1;
}

# NOT tests have their own constant for tracking broken-ness.
sub _known_broken {
    my ($self) = @_;
    return $self->SUPER::_known_broken(BROKEN_NOT, 'skip pg check');
}

sub search_params {
    my ($self) = @_;
    my %params = %{ $self->SUPER::search_params() };
    $params{negate0} = 1;
    return \%params;
}

1;