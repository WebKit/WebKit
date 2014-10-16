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

package Bugzilla::Search::Condition;
use strict;
use base qw(Exporter);
our @EXPORT_OK = qw(condition);

sub new {
    my ($class, $params) = @_;
    my %self = %$params;
    bless \%self, $class;
    return \%self;
}

sub field    { return $_[0]->{field}    }
sub operator { return $_[0]->{operator} }
sub value    { return $_[0]->{value}    }

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
