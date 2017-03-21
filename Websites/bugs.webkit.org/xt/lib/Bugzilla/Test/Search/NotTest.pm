# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This module runs tests just like a normal FieldTest, AndTest,
# or OrTest, but in a NOT chart instead of a normal chart.
#
# Logically this should be a mixin of some sort so that we can apply
# it to OrTest and AndTest, but without Moose there isn't much of an
# easy way to do that.
package Bugzilla::Test::Search::NotTest;
use parent qw(Bugzilla::Test::Search::FieldTest);
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
