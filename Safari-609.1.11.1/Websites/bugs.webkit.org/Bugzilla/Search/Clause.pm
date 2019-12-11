# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Search::Clause;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Error;
use Bugzilla::Search::Condition qw(condition);
use Bugzilla::Util qw(trick_taint);

sub new {
    my ($class, $joiner) = @_;
    if ($joiner and $joiner ne 'OR' and $joiner ne 'AND') {
        ThrowCodeError('search_invalid_joiner', { joiner => $joiner });
    }
    # This will go into SQL directly so needs to be untainted.
    trick_taint($joiner) if $joiner;
    bless { joiner => $joiner || 'AND' }, $class;
}

sub children {
    my ($self) = @_;
    $self->{children} ||= [];
    return $self->{children};
}

sub update_search_args {
    my ($self, $search_args) = @_;
    # abstract
}

sub joiner { return $_[0]->{joiner} }

sub has_translated_conditions {
    my ($self) = @_;
    my $children = $self->children;
    return 1 if grep { $_->isa('Bugzilla::Search::Condition')
                       && $_->translated } @$children;
    foreach my $child (@$children) {
        next if $child->isa('Bugzilla::Search::Condition');
        return 1 if $child->has_translated_conditions;
    }
    return 0;
}

sub add {
    my $self = shift;
    my $children = $self->children;
    if (@_ == 3) {
        push(@$children, condition(@_));
        return;
    }
    
    my ($child) = @_;
    return if !defined $child;
    $child->isa(__PACKAGE__) || $child->isa('Bugzilla::Search::Condition')
        || die 'child not the right type: ' . $child;
    push(@{ $self->children }, $child);
}

sub negate {
    my ($self, $value) = @_;
    if (@_ == 2) {
        $self->{negate} = $value ? 1 : 0;
    }
    return $self->{negate};
}

sub walk_conditions {
    my ($self, $callback) = @_;
    foreach my $child (@{ $self->children }) {
        if ($child->isa('Bugzilla::Search::Condition')) {
            $callback->($self, $child);
        }
        else {
            $child->walk_conditions($callback);
        }
    }
}

sub as_string {
    my ($self) = @_;
    if (!$self->{sql}) {
        my @strings;
        foreach my $child (@{ $self->children }) {
            next if $child->isa(__PACKAGE__) && !$child->has_translated_conditions;
            next if $child->isa('Bugzilla::Search::Condition')
                    && !$child->translated;

            my $string = $child->as_string;
            next unless $string;
            if ($self->joiner eq 'AND') {
                $string = "( $string )" if $string =~ /OR/;
            }
            else {
                $string = "( $string )" if $string =~ /AND/;
            }
            push(@strings, $string);
        }

        my $sql = join(' ' . $self->joiner . ' ', @strings);
        $sql = "NOT( $sql )" if $sql && $self->negate;
        $self->{sql} = $sql;
    }
    return $self->{sql};
}

# Search.pm converts URL parameters to Clause objects. This helps do the
# reverse.
sub as_params {
    my ($self) = @_;
    my @params;
    foreach my $child (@{ $self->children }) {
        if ($child->isa(__PACKAGE__)) {
            my %open_paren = (f => 'OP', n => scalar $child->negate,
                              j => $child->joiner);
            push(@params, \%open_paren);
            push(@params, $child->as_params);
            my %close_paren =  (f => 'CP');
            push(@params, \%close_paren);
        }
        else {
            push(@params, $child->as_params);
        }
    }
    return @params;
}

1;

=head1 B<Methods in need of POD>

=over

=item has_translated_conditions

=item as_string

=item add

=item children

=item negate

=item update_search_args

=item walk_conditions

=item joiner

=item as_params

=back
