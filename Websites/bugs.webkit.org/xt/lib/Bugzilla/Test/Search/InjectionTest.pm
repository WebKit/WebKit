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

# This module represents the SQL Injection tests that get run on a single
# operator/field combination for Bugzilla::Test::Search.
package Bugzilla::Test::Search::InjectionTest;
use base qw(Bugzilla::Test::Search::FieldTest);

use strict;
use warnings;
use Bugzilla::Test::Search::Constants;
use Test::Exception;

sub num_tests { return NUM_SEARCH_TESTS }

sub _known_broken {
    my ($self) = @_;
    my $operator_broken = INJECTION_BROKEN_OPERATOR->{$self->operator};
    # We don't want to auto-vivify $operator_broken and thus make it true.
    my @field_ok = $operator_broken ? @{ $operator_broken->{field_ok} || [] }
                                    : ();
    $operator_broken = undef if grep { $_ eq $self->field } @field_ok;

    my $field_broken = INJECTION_BROKEN_FIELD->{$self->field}
                       || INJECTION_BROKEN_FIELD->{$self->field_object->type};
    # We don't want to auto-vivify $field_broken and thus make it true.
    my @operator_ok = $field_broken ? @{ $field_broken->{operator_ok} || [] }
                                    : ();
    $field_broken = undef if grep { $_ eq $self->operator } @operator_ok;

    return $operator_broken || $field_broken || {};
}

sub sql_error_ok { return $_[0]->_known_broken->{sql_error} }

# Injection tests only skip fields on certain dbs.
sub field_not_yet_implemented {
    my ($self) = @_;
    # We use the constant directly because we don't want operator_ok
    # or field_ok to stop us.
    my $broken = INJECTION_BROKEN_FIELD->{$self->field}
                 || INJECTION_BROKEN_FIELD->{$self->field_object->type};
    my $skip_for_dbs = $broken->{db_skip};
    return undef if !$skip_for_dbs;
    my $dbh = Bugzilla->dbh;
    if (my ($skip) = grep { $dbh->isa("Bugzilla::DB::$_") } @$skip_for_dbs) {
        my $field = $self->field;
        return "$field injection testing is not supported with $skip";
    }
    return undef;
}
# Injection tests don't do translation.
sub translated_value { $_[0]->test_value }

sub name { return "injection-" . $_[0]->SUPER::name; }

# Injection tests don't check content.
sub _test_content {}

sub _test_sql {
    my $self = shift;
    my ($sql) = @_;
    my $dbh = Bugzilla->dbh;
    my $name = $self->name;
    if (my $error_ok = $self->sql_error_ok) {
        throws_ok { $dbh->selectall_arrayref($sql) } $error_ok,
                  "$name: SQL query dies, as we expect";
        return;
    }
    return $self->SUPER::_test_sql(@_);
}

1;