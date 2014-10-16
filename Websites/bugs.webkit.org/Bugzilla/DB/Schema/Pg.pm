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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Andrew Dunstan <andrew@dunslane.net>,
#                 Edward J. Sabol <edwardjsabol@iname.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::DB::Schema::Pg;

###############################################################################
#
# DB::Schema implementation for PostgreSQL
#
###############################################################################

use strict;
use base qw(Bugzilla::DB::Schema);
use Storable qw(dclone);

#------------------------------------------------------------------------------
sub _initialize {

    my $self = shift;

    $self = $self->SUPER::_initialize(@_);

    # Remove FULLTEXT index types from the schemas.
    foreach my $table (keys %{ $self->{schema} }) {
        if ($self->{schema}{$table}{INDEXES}) {
            foreach my $index (@{ $self->{schema}{$table}{INDEXES} }) {
                if (ref($index) eq 'HASH') {
                    delete($index->{TYPE}) if (exists $index->{TYPE} 
                        && $index->{TYPE} eq 'FULLTEXT');
                }
            }
            foreach my $index (@{ $self->{abstract_schema}{$table}{INDEXES} }) {
                if (ref($index) eq 'HASH') {
                    delete($index->{TYPE}) if (exists $index->{TYPE} 
                        && $index->{TYPE} eq 'FULLTEXT');
                }
            }
        }
    }

    $self->{db_specific} = {

        BOOLEAN =>      'smallint',
        FALSE =>        '0', 
        TRUE =>         '1',

        INT1 =>         'integer',
        INT2 =>         'integer',
        INT3 =>         'integer',
        INT4 =>         'integer',

        SMALLSERIAL =>  'serial unique',
        MEDIUMSERIAL => 'serial unique',
        INTSERIAL =>    'serial unique',

        TINYTEXT =>     'varchar(255)',
        MEDIUMTEXT =>   'text',
        LONGTEXT =>     'text',

        LONGBLOB =>     'bytea',

        DATETIME =>     'timestamp(0) without time zone',

    };

    $self->_adjust_schema;

    return $self;

} #eosub--_initialize
#--------------------------------------------------------------------

sub get_rename_column_ddl {
    my ($self, $table, $old_name, $new_name) = @_;
    if (lc($old_name) eq lc($new_name)) {
        # if the only change is a case change, return an empty list, since Pg
        # is case-insensitive and will return an error about a duplicate name
        return ();
    }
    my @sql = ("ALTER TABLE $table RENAME COLUMN $old_name TO $new_name");
    my $def = $self->get_column_abstract($table, $old_name);
    if ($def->{TYPE} =~ /SERIAL/i) {
        # We have to rename the series also.
        push(@sql, "ALTER SEQUENCE ${table}_${old_name}_seq 
                         RENAME TO ${table}_${new_name}_seq");
    }
    return @sql;
}

sub get_rename_table_sql {
    my ($self, $old_name, $new_name) = @_;
    if (lc($old_name) eq lc($new_name)) {
        # if the only change is a case change, return an empty list, since Pg
        # is case-insensitive and will return an error about a duplicate name
        return ();
    }

    my @sql = ("ALTER TABLE $old_name RENAME TO $new_name");

    # If there's a SERIAL column on this table, we also need to rename the
    # sequence.
    # If there is a PRIMARY KEY, we need to rename it too.
    my @columns = $self->get_table_columns($old_name);
    foreach my $column (@columns) {
        my $def = $self->get_column_abstract($old_name, $column);
        if ($def->{TYPE} =~ /SERIAL/i) {
            my $old_seq = "${old_name}_${column}_seq";
            my $new_seq = "${new_name}_${column}_seq";
            push(@sql, "ALTER SEQUENCE $old_seq RENAME TO $new_seq");
            push(@sql, "ALTER TABLE $new_name ALTER COLUMN $column
                             SET DEFAULT NEXTVAL('$new_seq')");
        }
        if ($def->{PRIMARYKEY}) {
            my $old_pk = "${old_name}_pkey";
            my $new_pk = "${new_name}_pkey";
            push(@sql, "ALTER INDEX $old_pk RENAME to $new_pk");
        }
    }

    return @sql;
}

sub get_set_serial_sql {
    my ($self, $table, $column, $value) = @_;
    return ("SELECT setval('${table}_${column}_seq', $value, false)
               FROM $table");
}

sub _get_alter_type_sql {
    my ($self, $table, $column, $new_def, $old_def) = @_;
    my @statements;

    my $type = $new_def->{TYPE};
    $type = $self->{db_specific}->{$type} 
        if exists $self->{db_specific}->{$type};

    if ($type =~ /serial/i && $old_def->{TYPE} !~ /serial/i) {
        die("You cannot specify a DEFAULT on a SERIAL-type column.") 
            if $new_def->{DEFAULT};
    }

    $type =~ s/\bserial\b/integer/i;

    # On Pg, you don't need UNIQUE if you're a PK--it creates
    # two identical indexes otherwise.
    $type =~ s/unique//i if $new_def->{PRIMARYKEY};

    push(@statements, "ALTER TABLE $table ALTER COLUMN $column
                              TYPE $type");

    if ($new_def->{TYPE} =~ /serial/i && $old_def->{TYPE} !~ /serial/i) {
        push(@statements, "CREATE SEQUENCE ${table}_${column}_seq
                                  OWNED BY $table.$column");
        push(@statements, "SELECT setval('${table}_${column}_seq',
                                         MAX($table.$column))
                             FROM $table");
        push(@statements, "ALTER TABLE $table ALTER COLUMN $column 
                           SET DEFAULT nextval('${table}_${column}_seq')");
    }

    # If this column is no longer SERIAL, we need to drop the sequence
    # that went along with it.
    if ($old_def->{TYPE} =~ /serial/i && $new_def->{TYPE} !~ /serial/i) {
        push(@statements, "ALTER TABLE $table ALTER COLUMN $column 
                           DROP DEFAULT");
        push(@statements, "ALTER SEQUENCE ${table}_${column}_seq 
                           OWNED BY NONE");
        push(@statements, "DROP SEQUENCE ${table}_${column}_seq");
    }

    return @statements;
}

1;
