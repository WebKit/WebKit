# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This module represents the SQL Injection tests that get run on a single
# operator/field combination for Bugzilla::Test::Search.
package Bugzilla::Test::Search::InjectionTest;
use parent qw(Bugzilla::Test::Search::FieldTest);

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
