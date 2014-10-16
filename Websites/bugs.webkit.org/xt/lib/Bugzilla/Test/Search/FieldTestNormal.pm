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

# This is the same as a FieldTest, except that it uses normal URL
# parameters instead of Boolean Charts.
package Bugzilla::Test::Search::FieldTestNormal;
use strict;
use warnings;
use base qw(Bugzilla::Test::Search::FieldTest);

use Scalar::Util qw(blessed);

use constant CH_OPERATOR => {
    changedafter  => 'chfieldfrom',
    changedbefore => 'chfieldto',
    changedto     => 'chfieldvalue',
};

use constant EMAIL_FIELDS => qw(assigned_to qa_contact cc reporter commenter);

# Normally, we just clone a FieldTest because that's the best for performance,
# overall--that way we don't have to translate the value again. However,
# sometimes (like in Bugzilla::Test::Search's direct code) we just want
# to create a FieldTestNormal.
sub new {
    my $class = shift;
    my ($first_arg) = @_;
    if (blessed $first_arg
        and $first_arg->isa('Bugzilla::Test::Search::FieldTest'))
    {
        my $self = { %$first_arg };
        return bless $self, $class;
    }
    return $class->SUPER::new(@_);
}

sub name {
    my $self = shift;
    my $name = $self->SUPER::name(@_);
    return "$name (Normal Params)";
}

sub search_columns {
    my $self = shift;
    my $field = $self->field;
    # For the assigned_to, qa_contact, and reporter fields, have the
    # "Normal Params" test check that the _realname columns work
    # all by themselves.
    if (grep($_ eq $field, EMAIL_FIELDS) && $self->field_object->buglist) {
        return ['bug_id', "${field}_realname"]
    }
    return $self->SUPER::search_columns(@_);
}

sub search_params {
    my ($self) = @_;
    my $field = $self->field;
    my $operator = $self->operator;
    my $value = $self->translated_value;
    if ($operator eq 'anyexact') {
        $value = [split ',', $value];
    }
    
    if (my $ch_param = CH_OPERATOR->{$operator}) {
        if ($field eq 'creation_ts') {
            $field = '[Bug creation]';
        }
        return { chfield => $field, $ch_param => $value };
    }
    
    if ($field eq 'delta_ts' and $operator eq 'greaterthaneq') {
        return { chfieldfrom => $value };
    }
    if ($field eq 'delta_ts' and $operator eq 'lessthaneq') {
        return { chfieldto => $value };
    }
    
    if ($field eq 'deadline' and $operator eq 'greaterthaneq') {
        return { deadlinefrom => $value };
    }
    if ($field eq 'deadline' and $operator eq 'lessthaneq') {
        return { deadlineto => $value };
    }
    
    if (grep { $_ eq $field } EMAIL_FIELDS) {
        $field = 'longdesc' if $field eq 'commenter';
        return {
            email1           => $value,
            "email${field}1" => 1,
            emailtype1       => $operator,
            # Used to do extra tests on special sorts of email* combinations.
            %{ $self->test->{extra_params} || {} },
        };
    }

    $field =~ s/\./_/g;
    return { $field => $value, "${field}_type" => $operator };
}

1;
