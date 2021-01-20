# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::DB::Schema::Oracle;

###############################################################################
#
# DB::Schema implementation for Oracle
#
###############################################################################

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::DB::Schema);
use Carp qw(confess);
use Bugzilla::Util;

use constant ADD_COLUMN => 'ADD';
use constant MULTIPLE_FKS_IN_ALTER => 0;
# Whether this is true or not, this is what it needs to be in order for
# hash_identifier to maintain backwards compatibility with versions before
# 3.2rc2.
use constant MAX_IDENTIFIER_LEN => 27;

#------------------------------------------------------------------------------
sub _initialize {

    my $self = shift;

    $self = $self->SUPER::_initialize(@_);

    $self->{db_specific} = {

        BOOLEAN =>      'integer',
        FALSE =>        '0', 
        TRUE =>         '1',

        INT1 =>         'integer',
        INT2 =>         'integer',
        INT3 =>         'integer',
        INT4 =>         'integer',

        SMALLSERIAL  => 'integer',
        MEDIUMSERIAL => 'integer',
        INTSERIAL    => 'integer',

        TINYTEXT   =>   'varchar(255)',
        MEDIUMTEXT =>   'varchar(4000)',
        LONGTEXT   =>   'clob',

        LONGBLOB =>     'blob',

        DATETIME =>     'date',
        DATE     =>     'date',
    };

    $self->_adjust_schema;

    return $self;

} #eosub--_initialize
#--------------------------------------------------------------------

sub get_table_ddl {
    my $self = shift;
    my $table = shift;
    unshift @_, $table;
    my @ddl = $self->SUPER::get_table_ddl(@_);

    my @fields = @{ $self->{abstract_schema}{$table}{FIELDS} || [] };
    while (@fields) {
        my $field_name = shift @fields;
        my $field_info = shift @fields;
        # Create triggers to deal with empty string. 
        if ( $field_info->{TYPE} =~ /varchar|TEXT/i 
                && $field_info->{NOTNULL} ) {
             push (@ddl, _get_notnull_trigger_ddl($table, $field_name));
        }
        # Create sequences and triggers to emulate SERIAL datatypes.
        if ( $field_info->{TYPE} =~ /SERIAL/i ) {
            push (@ddl, $self->_get_create_seq_ddl($table, $field_name));
        }
    }
    return @ddl;

} #eosub--get_table_ddl

# Extend superclass method to create Oracle Text indexes if index type 
# is FULLTEXT from schema. Returns a "create index" SQL statement.
sub _get_create_index_ddl {

    my ($self, $table_name, $index_name, $index_fields, $index_type) = @_;
    $index_name = "idx_" . $self->_hash_identifier($index_name);
    if ($index_type eq 'FULLTEXT') {
        my $sql = "CREATE INDEX $index_name ON $table_name (" 
                  . join(',',@$index_fields)
                  . ") INDEXTYPE IS CTXSYS.CONTEXT "
                  . " PARAMETERS('LEXER BZ_LEX SYNC(ON COMMIT)')" ;
        return $sql;
    }

    return($self->SUPER::_get_create_index_ddl($table_name, $index_name, 
                                               $index_fields, $index_type));

}

sub get_drop_index_ddl {
    my $self = shift;
    my ($table, $name) = @_;

    $name = 'idx_' . $self->_hash_identifier($name);
    return $self->SUPER::get_drop_index_ddl($table, $name);
}

# Oracle supports the use of FOREIGN KEY integrity constraints 
# to define the referential integrity actions, including:
# - Update and delete No Action (default)
# - Delete CASCADE
# - Delete SET NULL
sub get_fk_ddl {
    my $self = shift;
    my $ddl = $self->SUPER::get_fk_ddl(@_);

    # iThe Bugzilla Oracle driver implements UPDATE via a trigger.
    $ddl =~ s/ON UPDATE \S+//i;
    # RESTRICT is the default for DELETE on Oracle and may not be specified.
    $ddl =~ s/ON DELETE RESTRICT//i;

    return $ddl;
}

sub get_add_fks_sql {
    my $self = shift;
    my ($table, $column_fks) = @_;
    my @sql = $self->SUPER::get_add_fks_sql(@_);

    foreach my $column (keys %$column_fks) {
        my $fk = $column_fks->{$column};
        next if $fk->{UPDATE} && uc($fk->{UPDATE}) ne 'CASCADE';
        my $fk_name   = $self->_get_fk_name($table, $column, $fk);
        my $to_column = $fk->{COLUMN};
        my $to_table  = $fk->{TABLE};

        my $trigger = <<END;
CREATE OR REPLACE TRIGGER ${fk_name}_UC
          AFTER UPDATE OF $to_column ON $to_table
              REFERENCING NEW AS NEW OLD AS OLD
             FOR EACH ROW
                    BEGIN
                   UPDATE $table
                      SET $column = :NEW.$to_column
                    WHERE $column = :OLD.$to_column;
                      END ${fk_name}_UC;
END
        push(@sql, $trigger);
    }

    return @sql;
}

sub get_drop_fk_sql {
    my $self = shift;
    my ($table, $column, $references) = @_;
    my $fk_name = $self->_get_fk_name(@_);
    my @sql;
    if (!$references->{UPDATE} || $references->{UPDATE} =~ /CASCADE/i) {
        push(@sql, "DROP TRIGGER ${fk_name}_uc");
    }
    push(@sql, $self->SUPER::get_drop_fk_sql(@_));
    return @sql;
}

sub _get_fk_name {
    my ($self, $table, $column, $references) = @_;
    my $to_table  = $references->{TABLE};
    my $to_column = $references->{COLUMN};
    my $fk_name   = "${table}_${column}_${to_table}_${to_column}";
    $fk_name      = "fk_" . $self->_hash_identifier($fk_name);
    
    return $fk_name;
}

sub get_add_column_ddl {
    my $self = shift;
    my ($table, $column, $definition, $init_value) = @_;
    my @sql;

    # Create sequences and triggers to emulate SERIAL datatypes.
    if ($definition->{TYPE} =~ /SERIAL/i) {
        # Clone the definition to not alter the original one.
        my %def = %$definition;
        # Oracle requires to define the column is several steps.
        my $pk = delete $def{PRIMARYKEY};
        my $notnull = delete $def{NOTNULL};
        @sql = $self->SUPER::get_add_column_ddl($table, $column, \%def, $init_value);
        push(@sql, $self->_get_create_seq_ddl($table, $column));
        push(@sql, "UPDATE $table SET $column = ${table}_${column}_SEQ.NEXTVAL");
        push(@sql, "ALTER TABLE $table MODIFY $column NOT NULL") if $notnull;
        push(@sql, "ALTER TABLE $table ADD PRIMARY KEY ($column)") if $pk;
    }
    else {
        @sql = $self->SUPER::get_add_column_ddl(@_);
        # Create triggers to deal with empty string. 
        if ($definition->{TYPE} =~ /varchar|TEXT/i && $definition->{NOTNULL}) {
            push(@sql, _get_notnull_trigger_ddl($table, $column));
        }
    }

    return @sql;
}

sub get_alter_column_ddl {
    my ($self, $table, $column, $new_def, $set_nulls_to) = @_;

    my @statements;
    my $old_def = $self->get_column_abstract($table, $column);
    my $specific = $self->{db_specific};

    # If the types have changed, we have to deal with that.
    if (uc(trim($old_def->{TYPE})) ne uc(trim($new_def->{TYPE}))) {
        push(@statements, $self->_get_alter_type_sql($table, $column, 
                                                     $new_def, $old_def));
    }

    my $default = $new_def->{DEFAULT};
    my $default_old = $old_def->{DEFAULT};

    if (defined $default) {
        $default = $specific->{$default} if exists $specific->{$default};
    }
    # This first condition prevents "uninitialized value" errors.
    if (!defined $default && !defined $default_old) {
        # Do Nothing
    }
    # If we went from having a default to not having one
    elsif (!defined $default && defined $default_old) {
        push(@statements, "ALTER TABLE $table MODIFY $column"
                        . " DEFAULT NULL");
    }
    # If we went from no default to a default, or we changed the default.
    elsif ( (defined $default && !defined $default_old) || 
            ($default ne $default_old) ) 
    {
        push(@statements, "ALTER TABLE $table MODIFY $column "
                         . " DEFAULT $default");
    }

    # If we went from NULL to NOT NULL.
    if (!$old_def->{NOTNULL} && $new_def->{NOTNULL}) {
        my $setdefault;
        # Handle any fields that were NULL before, if we have a default,
        $setdefault = $default if defined $default;
        # But if we have a set_nulls_to, that overrides the DEFAULT 
        # (although nobody would usually specify both a default and 
        # a set_nulls_to.)
        $setdefault = $set_nulls_to if defined $set_nulls_to;
        if (defined $setdefault) {
            push(@statements, "UPDATE $table SET $column = $setdefault"
                            . "  WHERE $column IS NULL");
        }
        push(@statements, "ALTER TABLE $table MODIFY $column"
                        . " NOT NULL");
        push (@statements, _get_notnull_trigger_ddl($table, $column))
                                   if $old_def->{TYPE} =~ /varchar|text/i
                                     && $new_def->{TYPE} =~ /varchar|text/i;
    }
    # If we went from NOT NULL to NULL
    elsif ($old_def->{NOTNULL} && !$new_def->{NOTNULL}) {
        push(@statements, "ALTER TABLE $table MODIFY $column"
                        . " NULL");
        push(@statements, "DROP TRIGGER ${table}_${column}")
                           if $new_def->{TYPE} =~ /varchar|text/i 
                             && $old_def->{TYPE} =~ /varchar|text/i;
    }

    # If we went from not being a PRIMARY KEY to being a PRIMARY KEY.
    if (!$old_def->{PRIMARYKEY} && $new_def->{PRIMARYKEY}) {
        push(@statements, "ALTER TABLE $table ADD PRIMARY KEY ($column)");
    }
    # If we went from being a PK to not being a PK
    elsif ( $old_def->{PRIMARYKEY} && !$new_def->{PRIMARYKEY} ) {
        push(@statements, "ALTER TABLE $table DROP PRIMARY KEY");
    }

    return @statements;
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

    if ( ($old_def->{TYPE} =~ /LONGTEXT/i && $new_def->{TYPE} !~ /LONGTEXT/i) 
         || ($old_def->{TYPE} !~ /LONGTEXT/i && $new_def->{TYPE} =~ /LONGTEXT/i)
       ) {
        # LONG to VARCHAR or VARCHAR to LONG is not allowed in Oracle, 
        # just a way to work around.
        # Determine whether column_temp is already exist.
        my $dbh=Bugzilla->dbh;
        my $column_exist = $dbh->selectcol_arrayref(
                          "SELECT CNAME FROM COL WHERE TNAME = UPPER(?) AND 
                             CNAME = UPPER(?)", undef,$table,$column . "_temp");
        if(!@$column_exist) {
        push(@statements, 
            "ALTER TABLE $table ADD ${column}_temp $type");  
        }
        push(@statements, "UPDATE $table SET ${column}_temp = $column");
        push(@statements, "COMMIT");
        push(@statements, "ALTER TABLE $table DROP COLUMN $column");
        push(@statements, 
            "ALTER TABLE $table RENAME COLUMN ${column}_temp TO $column");
    } else {  
        push(@statements, "ALTER TABLE $table MODIFY $column $type");
    }

    if ($new_def->{TYPE} =~ /serial/i && $old_def->{TYPE} !~ /serial/i) {
         push(@statements, _get_create_seq_ddl($table, $column));
    }

    # If this column is no longer SERIAL, we need to drop the sequence
    # that went along with it.
    if ($old_def->{TYPE} =~ /serial/i && $new_def->{TYPE} !~ /serial/i) {
        push(@statements, "DROP SEQUENCE ${table}_${column}_SEQ");
        push(@statements, "DROP TRIGGER ${table}_${column}_TR");
    }
    
    # If this column is changed to type TEXT/VARCHAR, we need to deal with
    # empty string.
    if ( $old_def->{TYPE} !~ /varchar|text/i 
            && $new_def->{TYPE} =~ /varchar|text/i 
            && $new_def->{NOTNULL} )
    {
             push (@statements, _get_notnull_trigger_ddl($table, $column));
    } 
    # If this column is no longer TEXT/VARCHAR, we need to drop the trigger
    # that went along with it.
    if ( $old_def->{TYPE} =~ /varchar|text/i
            && $old_def->{NOTNULL}
            && $new_def->{TYPE} !~ /varchar|text/i )
    {
        push(@statements, "DROP TRIGGER ${table}_${column}");
    } 
    return @statements;
}

sub get_rename_column_ddl {
    my ($self, $table, $old_name, $new_name) = @_;
    if (lc($old_name) eq lc($new_name)) {
        # if the only change is a case change, return an empty list.
        return ();
    }
    my @sql = ("ALTER TABLE $table RENAME COLUMN $old_name TO $new_name");
    my $def = $self->get_column_abstract($table, $old_name);
    if ($def->{TYPE} =~ /SERIAL/i) {
        # We have to rename the series also, and fix the default of the series.
        my $old_seq = "${table}_${old_name}_SEQ";
        my $new_seq = "${table}_${new_name}_SEQ";
        push(@sql, "RENAME $old_seq TO $new_seq");
        push(@sql, $self->_get_create_trigger_ddl($table, $new_name, $new_seq));
        push(@sql, "DROP TRIGGER ${table}_${old_name}_TR");
    }
    if ($def->{TYPE} =~ /varchar|text/i && $def->{NOTNULL} ) {
        push(@sql, _get_notnull_trigger_ddl($table,$new_name));
        push(@sql, "DROP TRIGGER ${table}_${old_name}");
    }
    return @sql;
}

sub get_drop_column_ddl {
    my $self = shift;
    my ($table, $column) = @_;
    my @sql;
    push(@sql, $self->SUPER::get_drop_column_ddl(@_));
    my $dbh=Bugzilla->dbh;
    my $trigger_name = uc($table . "_" . $column);
    my $exist_trigger = $dbh->selectcol_arrayref(
        "SELECT OBJECT_NAME FROM USER_OBJECTS
         WHERE OBJECT_NAME = ?", undef, $trigger_name);
    if(@$exist_trigger) {
        push(@sql, "DROP TRIGGER $trigger_name");
    }
    # If this column is of type SERIAL, we need to drop the sequence
    # and trigger that went along with it.
    my $def = $self->get_column_abstract($table, $column);
    if ($def->{TYPE} =~ /SERIAL/i) {
        push(@sql, "DROP SEQUENCE ${table}_${column}_SEQ");
        push(@sql, "DROP TRIGGER ${table}_${column}_TR");
    }
    return @sql;
}

sub get_rename_table_sql {
    my ($self, $old_name, $new_name) = @_;
    if (lc($old_name) eq lc($new_name)) {
        # if the only change is a case change, return an empty list.
        return ();
    }

    my @sql = ("ALTER TABLE $old_name RENAME TO $new_name");
    my @columns = $self->get_table_columns($old_name);
    foreach my $column (@columns) {
        my $def = $self->get_column_abstract($old_name, $column);
        if ($def->{TYPE} =~ /SERIAL/i) {
            # If there's a SERIAL column on this table, we also need
            # to rename the sequence.
            my $old_seq = "${old_name}_${column}_SEQ";
            my $new_seq = "${new_name}_${column}_SEQ";
            push(@sql, "RENAME $old_seq TO $new_seq");
            push(@sql, $self->_get_create_trigger_ddl($new_name, $column, $new_seq));
            push(@sql, "DROP TRIGGER ${old_name}_${column}_TR");
        }
        if ($def->{TYPE} =~ /varchar|text/i && $def->{NOTNULL}) {
            push(@sql, _get_notnull_trigger_ddl($new_name, $column));
            push(@sql, "DROP TRIGGER ${old_name}_${column}");
        }
    }

    return @sql;
}

sub get_drop_table_ddl {
    my ($self, $name) = @_;
    my @sql;

    my @columns = $self->get_table_columns($name);
    foreach my $column (@columns) {
        my $def = $self->get_column_abstract($name, $column);
        if ($def->{TYPE} =~ /SERIAL/i) {
            # If there's a SERIAL column on this table, we also need
            # to remove the sequence.
            push(@sql, "DROP SEQUENCE ${name}_${column}_SEQ");
        }
    }
    push(@sql, "DROP TABLE $name CASCADE CONSTRAINTS PURGE");

    return @sql;
}

sub _get_notnull_trigger_ddl {
      my ($table, $column) = @_;

      my $notnull_sql = "CREATE OR REPLACE TRIGGER "
                        . " ${table}_${column}"
                        . " BEFORE INSERT OR UPDATE ON ". $table
                        . " FOR EACH ROW"
                        . " BEGIN "
                        . " IF :NEW.". $column ." IS NULL THEN  "
                        . " SELECT '" . Bugzilla::DB::Oracle->EMPTY_STRING
                        . "' INTO :NEW.". $column ." FROM DUAL; "
                        . " END IF; "
                        . " END ".$table.";";
     return $notnull_sql;
}

sub _get_create_seq_ddl {
     my ($self, $table, $column, $start_with) = @_;
     $start_with ||= 1;
     my @ddl;
     my $seq_name = "${table}_${column}_SEQ";
     my $seq_sql = "CREATE SEQUENCE $seq_name "
                   . " INCREMENT BY 1 "
                   . " START WITH $start_with "
                   . " NOMAXVALUE "
                   . " NOCYCLE "
                   . " NOCACHE";
    push (@ddl, $seq_sql);
    push(@ddl, $self->_get_create_trigger_ddl($table, $column, $seq_name));

    return @ddl;
}

sub _get_create_trigger_ddl {
    my ($self, $table, $column, $seq_name) = @_;
    my $serial_sql = "CREATE OR REPLACE TRIGGER ${table}_${column}_TR "
                   . " BEFORE INSERT ON $table "
                   . " FOR EACH ROW "
                   . " BEGIN "
                   . "   SELECT ${seq_name}.NEXTVAL "
                   . "   INTO :NEW.$column FROM DUAL; "
                   . " END;";
    return $serial_sql;
}

sub get_set_serial_sql { 
    my ($self, $table, $column, $value) = @_; 
    my @sql;
    my $seq_name = "${table}_${column}_SEQ";
    push(@sql, "DROP SEQUENCE ${seq_name}");
    push(@sql, $self->_get_create_seq_ddl($table, $column, $value));       
    return @sql;
} 

1;

=head1 B<Methods in need of POD>

=over

=item get_rename_column_ddl

=item get_add_fks_sql

=item get_drop_index_ddl

=item get_rename_table_sql

=item get_add_column_ddl

=item get_set_serial_sql

=item get_drop_column_ddl

=item get_drop_table_ddl

=item get_drop_fk_sql

=item get_table_ddl

=item get_alter_column_ddl

=item get_fk_ddl

=back
