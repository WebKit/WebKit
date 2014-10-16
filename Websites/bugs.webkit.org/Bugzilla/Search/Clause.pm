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
# The Initial Developer of the Original Code is BugzillaSource, Inc.
# Portions created by the Initial Developer are Copyright (C) 2011 the
# Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::Search::Clause;
use strict;

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
            $callback->($child);
        }
        else {
            $child->walk_conditions($callback);
        }
    }
}

sub as_string {
    my ($self) = @_;
    my @strings;
    foreach my $child (@{ $self->children }) {
        next if $child->isa(__PACKAGE__) && !$child->has_translated_conditions;
        next if $child->isa('Bugzilla::Search::Condition')
                && !$child->translated;

        my $string = $child->as_string;
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
    return $sql;
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
