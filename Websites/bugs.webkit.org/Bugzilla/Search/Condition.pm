# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Search::Condition;

use 5.10.1;
use strict;
use warnings;

use parent qw(Exporter);
our @EXPORT_OK = qw(condition);

sub new {
    my ($class, $params) = @_;
    my %self = %$params;
    bless \%self, $class;
    return \%self;
}

sub field    { return $_[0]->{field}    }
sub value    { return $_[0]->{value}    }

sub operator {
    my ($self, $value) = @_;
    if (@_ == 2) {
        $self->{operator} = $value;
    }
    return $self->{operator};
}

sub fov {
    my ($self) = @_;
    return ($self->field, $self->operator, $self->value);
}

sub translated {
    my ($self, $params) = @_;
    if (@_ == 2) {
        $self->{translated} = $params;
    }
    return $self->{translated};
}

sub as_string {
    my ($self) = @_;
    my $term = $self->translated->{term};
    $term = "NOT( $term )" if $term && $self->negate;
    return $term;
}

sub as_params {
    my ($self) = @_;
    return { f => $self->field, o => $self->operator, v => $self->value,
             n => scalar $self->negate };
}

sub negate {
    my ($self, $value) = @_;
    if (@_ == 2) {
        $self->{negate} = $value ? 1 : 0;
    }
    return $self->{negate};
}

###########################
# Convenience Subroutines #
###########################

sub condition {
    my ($field, $operator, $value) = @_;
    return __PACKAGE__->new({ field => $field, operator => $operator,
                              value => $value });
}

1;

=head1 B<Methods in need of POD>

=over

=item as_string

=item fov

=item value

=item negate

=item translated

=item operator

=item as_params

=item condition

=item field

=back
