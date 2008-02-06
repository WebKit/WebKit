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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Christopher Aillon <christopher@aillon.com>
#                 Tomas Kopal <Tomas.Kopal@altap.cz>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::DB;

use strict;

use DBI;

# Inherit the DB class from DBI::db and Exporter
# Note that we inherit from Exporter just to allow the old, deprecated
# interface to work. If it gets removed, the Exporter class can be removed
# from this list.
use base qw(Exporter DBI::db);

%Bugzilla::DB::EXPORT_TAGS =
  (
   deprecated => [qw(SendSQL SqlQuote
                     MoreSQLData FetchSQLData FetchOneColumn
                     PushGlobalSQLState PopGlobalSQLState)
                 ],
);
Exporter::export_ok_tags('deprecated');

use Bugzilla::Config qw(:DEFAULT :db);
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::DB::Schema;
use Bugzilla::User;

#####################################################################
# Constants
#####################################################################

use constant BLOB_TYPE => DBI::SQL_BLOB;

#####################################################################
# Deprecated Functions
#####################################################################

# All this code is backwards compat fu. As such, its a bit ugly. Note the
# circular dependencies on Bugzilla.pm
# This is old cruft which will be removed, so theres not much use in
# having a separate package for it, or otherwise trying to avoid the circular
# dependency

# XXX - mod_perl
# These use |our| instead of |my| because they need to be cleared from
# Bugzilla.pm. See bug 192531 for details.
our $_current_sth;
our @SQLStateStack = ();

my $_fetchahead;

sub SendSQL {
    my ($str) = @_;

    $_current_sth = Bugzilla->dbh->prepare($str);

    $_current_sth->execute;

    # This is really really ugly, but its what we get for not doing
    # error checking for 5 years. See bug 189446 and bug 192531
    $_current_sth->{RaiseError} = 0;

    undef $_fetchahead;
}

# Its much much better to use bound params instead of this
sub SqlQuote {
    my ($str) = @_;

    # Backwards compat code
    return "''" if not defined $str;

    my $res = Bugzilla->dbh->quote($str);

    trick_taint($res);

    return $res;
}

sub MoreSQLData {
    return 1 if defined $_fetchahead;

    if ($_fetchahead = $_current_sth->fetchrow_arrayref()) {
        return 1;
    }
    return 0;
}

sub FetchSQLData {
    if (defined $_fetchahead) {
        my @result = @$_fetchahead;
        undef $_fetchahead;
        return @result;
    }

    return $_current_sth->fetchrow_array;
}

sub FetchOneColumn {
    my @row = FetchSQLData();
    return $row[0];
}

sub PushGlobalSQLState() {
    push @SQLStateStack, $_current_sth;
    push @SQLStateStack, $_fetchahead;
}

sub PopGlobalSQLState() {
    die ("PopGlobalSQLState: stack underflow") if ( scalar(@SQLStateStack) < 1 );
    $_fetchahead = pop @SQLStateStack;
    $_current_sth = pop @SQLStateStack;
}

# MODERN CODE BELOW

#####################################################################
# Connection Methods
#####################################################################

sub connect_shadow {
    die "Tried to connect to non-existent shadowdb" unless Param('shadowdb');

    return _connect($db_driver, Param("shadowdbhost"),
                    Param('shadowdb'), Param("shadowdbport"),
                    Param("shadowdbsock"), $db_user, $db_pass);
}

sub connect_main (;$) {
    my ($no_db_name) = @_;
    my $connect_to_db = $db_name;
    $connect_to_db = "" if $no_db_name;
    return _connect($db_driver, $db_host, $connect_to_db, $db_port,
                    $db_sock, $db_user, $db_pass);
}

sub _connect {
    my ($driver, $host, $dbname, $port, $sock, $user, $pass) = @_;

    # DB specific module have the same name as DB driver, here we
    # just make sure we are not case sensitive
    (my $db_module = $driver) =~ s/(\w+)/\u\L$1/g;
    my $pkg_module = "Bugzilla::DB::" . $db_module;

    # do the actual import
    eval ("require $pkg_module")
        || die ("'$db_module' is not a valid choice for \$db_driver in "
                . " localconfig: " . $@);

    # instantiate the correct DB specific module
    my $dbh = $pkg_module->new($user, $pass, $host, $dbname, $port, $sock);

    return $dbh;
}

sub _handle_error {
    require Carp;

    # Cut down the error string to a reasonable size
    $_[0] = substr($_[0], 0, 2000) . ' ... ' . substr($_[0], -2000)
        if length($_[0]) > 4000;
    $_[0] = Carp::longmess($_[0]);
    return 0; # Now let DBI handle raising the error
}

# List of abstract methods we are checking the derived class implements
our @_abstract_methods = qw(REQUIRED_VERSION PROGRAM_NAME DBD_VERSION
                            new sql_regexp sql_not_regexp sql_limit sql_to_days
                            sql_date_format sql_interval
                            bz_lock_tables bz_unlock_tables);

# This overriden import method will check implementation of inherited classes
# for missing implementation of abstract methods
# See http://perlmonks.thepen.com/44265.html
sub import {
    my $pkg = shift;

    # do not check this module
    if ($pkg ne __PACKAGE__) {
        # make sure all abstract methods are implemented
        foreach my $meth (@_abstract_methods) {
            $pkg->can($meth)
                or croak("Class $pkg does not define method $meth");
        }
    }

    # Now we want to call our superclass implementation.
    # If our superclass is Exporter, which is using caller() to find
    # a namespace to populate, we need to adjust for this extra call.
    # All this can go when we stop using deprecated functions.
    my $is_exporter = $pkg->isa('Exporter');
    $Exporter::ExportLevel++ if $is_exporter;
    $pkg->SUPER::import(@_);
    $Exporter::ExportLevel-- if $is_exporter;
}

sub sql_istrcmp {
    my ($self, $left, $right, $op) = @_;
    $op ||= "=";

    return $self->sql_istring($left) . " $op " . $self->sql_istring($right);
}

sub sql_istring {
    my ($self, $string) = @_;

    return "LOWER($string)";
}

sub sql_position {
    my ($self, $fragment, $text) = @_;

    return "POSITION($fragment IN $text)";
}

sub sql_group_by {
    my ($self, $needed_columns, $optional_columns) = @_;

    my $expression = "GROUP BY $needed_columns";
    $expression .= ", " . $optional_columns if defined($optional_columns);
    
    return $expression;
}

sub sql_string_concat {
    my ($self, @params) = @_;
    
    return '(' . join(' || ', @params) . ')';
}

sub sql_fulltext_search {
    my ($self, $column, $text) = @_;

    # This is as close as we can get to doing full text search using
    # standard ANSI SQL, without real full text search support. DB specific
    # modules shoud override this, as this will be always much slower.

    # the text is already sql-quoted, so we need to remove the quotes first
    my $quote = substr($self->quote(''), 0, 1);
    $text = $1 if ($text =~ /^$quote(.*)$quote$/);

    # make the string lowercase to do case insensitive search
    my $lower_text = lc($text);

    # split the text we search for to separate words
    my @words = split(/\s+/, $lower_text);

    # search for occurence of all specified words in the column
    return "CASE WHEN (LOWER($column) LIKE ${quote}%" .
           join("%${quote} AND LOWER($column) LIKE ${quote}%", @words) .
           "%${quote}) THEN 1 ELSE 0 END";
}

#####################################################################
# General Info Methods
#####################################################################

# XXX - Needs to be documented.
sub bz_server_version {
    my ($self) = @_;
    return $self->get_info(18); # SQL_DBMS_VER
}

sub bz_last_key {
    my ($self, $table, $column) = @_;

    return $self->last_insert_id($db_name, undef, $table, $column);
}

sub bz_get_field_defs {
    my ($self) = @_;

    my $extra = "";
    if (!UserInGroup(Param('timetrackinggroup'))) {
        $extra = "AND name NOT IN ('estimated_time', 'remaining_time', " .
                 "'work_time', 'percentage_complete', 'deadline')";
    }

    my @fields;
    my $sth = $self->prepare("SELECT name, description FROM fielddefs
                              WHERE obsolete = 0 $extra
                              ORDER BY sortkey");
    $sth->execute();
    while (my $field_ref = $sth->fetchrow_hashref()) {
        push(@fields, $field_ref);
    }
    return(@fields);
}

#####################################################################
# Database Setup
#####################################################################

sub bz_setup_database {
    my ($self) = @_;

    # If we haven't ever stored a serialized schema,
    # set up the bz_schema table and store it.
    $self->_bz_init_schema_storage();
    
    my @desired_tables = $self->_bz_schema->get_table_list();

    foreach my $table_name (@desired_tables) {
        $self->bz_add_table($table_name);
    }
}

# The defauly implementation just returns what you passed-in. This function
# really exists just to be overridden in Bugzilla::DB::Mysql.
sub bz_enum_initial_values {
    my ($self, $enum_defaults) = @_;
    return $enum_defaults;
}

#####################################################################
# Schema Modification Methods
#####################################################################

sub bz_add_column {
    my ($self, $table, $name, $new_def, $init_value) = @_;

    # You can't add a NOT NULL column to a table with
    # no DEFAULT statement, unless you have an init_value.
    # SERIAL types are an exception, though, because they can
    # auto-populate.
    if ( $new_def->{NOTNULL} && !exists $new_def->{DEFAULT} 
         && !defined $init_value && $new_def->{TYPE} !~ /SERIAL/)
    {
        die "Failed adding the column ${table}.${name}:\n  You cannot add"
            . " a NOT NULL column with no default to an existing table,\n"
            . "  unless you specify something for \$init_value." 
    }

    my $current_def = $self->bz_column_info($table, $name);

    if (!$current_def) {
        my @statements = $self->_bz_real_schema->get_add_column_ddl(
            $table, $name, $new_def, 
            defined $init_value ? $self->quote($init_value) : undef);
        print "Adding new column $name to table $table ...\n";
        foreach my $sql (@statements) {
            $self->do($sql);
        }
        $self->_bz_real_schema->set_column($table, $name, $new_def);
        $self->_bz_store_real_schema;
    }
}

sub bz_alter_column {
    my ($self, $table, $name, $new_def, $set_nulls_to) = @_;

    my $current_def = $self->bz_column_info($table, $name);

    if (!$self->_bz_schema->columns_equal($current_def, $new_def)) {
        # You can't change a column to be NOT NULL if you have no DEFAULT
        # and no value for $set_nulls_to, if there are any NULL values 
        # in that column.
        if ($new_def->{NOTNULL} && 
            !exists $new_def->{DEFAULT} && !defined $set_nulls_to)
        {
            # Check for NULLs
            my $any_nulls = $self->selectrow_array(
                "SELECT 1 FROM $table WHERE $name IS NULL");
            if ($any_nulls) {
                die "You cannot alter the ${table}.${name} column to be"
                    . " NOT NULL without\nspecifying a default or"
                    . " something for \$set_nulls_to, because"
                    . " there are\nNULL values currently in it.";
            }
        }
        $self->bz_alter_column_raw($table, $name, $new_def, $current_def,
                                   $set_nulls_to);
        $self->_bz_real_schema->set_column($table, $name, $new_def);
        $self->_bz_store_real_schema;
    }
}


# bz_alter_column_raw($table, $name, $new_def, $current_def)
#
# Description: A helper function for bz_alter_column.
#              Alters a column in the database
#              without updating any Schema object. Generally
#              should only be called by bz_alter_column.
#              Used when either: (1) You don't yet have a Schema
#              object but you need to alter a column, for some reason.
#              (2) You need to alter a column for some database-specific
#              reason.
# Params:      $table   - The name of the table the column is on.
#              $name    - The name of the column you're changing.
#              $new_def - The abstract definition that you are changing
#                         this column to.
#              $current_def - (optional) The current definition of the
#                             column. Will be used in the output message,
#                             if given.
#              $set_nulls_to - The same as the param of the same name
#                              from bz_alter_column.
# Returns:     nothing
#
sub bz_alter_column_raw {
    my ($self, $table, $name, $new_def, $current_def, $set_nulls_to) = @_;
    my @statements = $self->_bz_real_schema->get_alter_column_ddl(
        $table, $name, $new_def,
        defined $set_nulls_to ? $self->quote($set_nulls_to) : undef);
    my $new_ddl = $self->_bz_schema->get_type_ddl($new_def);
    print "Updating column $name in table $table ...\n";
    if (defined $current_def) {
        my $old_ddl = $self->_bz_schema->get_type_ddl($current_def);
        print "Old: $old_ddl\n";
    }
    print "New: $new_ddl\n";
    $self->do($_) foreach (@statements);
}

sub bz_add_index {
    my ($self, $table, $name, $definition) = @_;

    my $index_exists = $self->bz_index_info($table, $name);

    if (!$index_exists) {
        $self->bz_add_index_raw($table, $name, $definition);
        $self->_bz_real_schema->set_index($table, $name, $definition);
        $self->_bz_store_real_schema;
    }
}

# bz_add_index_raw($table, $name, $silent)
#
# Description: A helper function for bz_add_index.
#              Adds an index to the database
#              without updating any Schema object. Generally
#              should only be called by bz_add_index.
#              Used when you don't yet have a Schema
#              object but you need to add an index, for some reason.
# Params:      $table  - The name of the table the index is on.
#              $name   - The name of the index you're adding.
#              $definition - The abstract index definition, in hashref
#                            or arrayref format.
#              $silent - (optional) If specified and true, don't output
#                        any message about this change.
# Returns:     nothing
#
sub bz_add_index_raw {
    my ($self, $table, $name, $definition, $silent) = @_;
    my @statements = $self->_bz_schema->get_add_index_ddl(
        $table, $name, $definition);
    print "Adding new index '$name' to the $table table ...\n" unless $silent;
    $self->do($_) foreach (@statements);
}

sub bz_add_table {
    my ($self, $name) = @_;

    my $table_exists = $self->bz_table_info($name);

    if (!$table_exists) {
        $self->_bz_add_table_raw($name);
        $self->_bz_real_schema->add_table($name,
            $self->_bz_schema->get_table_abstract($name));
        $self->_bz_store_real_schema;
    }
}

# _bz_add_table_raw($name) - Private
#
# Description: A helper function for bz_add_table.
#              Creates a table in the database without
#              updating any Schema object. Generally
#              should only be called by bz_add_table and by
#              _bz_init_schema_storage. Used when you don't
#              yet have a Schema object but you need to
#              add a table, for some reason.
# Params:      $name - The name of the table you're creating. 
#                  The definition for the table is pulled from 
#                  _bz_schema.
# Returns:     nothing
#
sub _bz_add_table_raw {
    my ($self, $name) = @_;
    my @statements = $self->_bz_schema->get_table_ddl($name);
    print "Adding new table $name ...\n";
    $self->do($_) foreach (@statements);
}

sub bz_drop_column {
    my ($self, $table, $column) = @_;

    my $current_def = $self->bz_column_info($table, $column);

    if ($current_def) {
        my @statements = $self->_bz_real_schema->get_drop_column_ddl(
            $table, $column);
        print "Deleting unused column $column from table $table ...\n";
        foreach my $sql (@statements) {
            # Because this is a deletion, we don't want to die hard if
            # we fail because of some local customization. If something
            # is already gone, that's fine with us!
            eval { $self->do($sql); } or warn "Failed SQL: [$sql] Error: $@";
        }
        $self->_bz_real_schema->delete_column($table, $column);
        $self->_bz_store_real_schema;
    }
}

sub bz_drop_index {
    my ($self, $table, $name) = @_;

    my $index_exists = $self->bz_index_info($table, $name);

    if ($index_exists) {
        $self->bz_drop_index_raw($table, $name);
        $self->_bz_real_schema->delete_index($table, $name);
        $self->_bz_store_real_schema;        
    }
}

# bz_drop_index_raw($table, $name, $silent)
#
# Description: A helper function for bz_drop_index.
#              Drops an index from the database
#              without updating any Schema object. Generally
#              should only be called by bz_drop_index.
#              Used when either: (1) You don't yet have a Schema 
#              object but you need to drop an index, for some reason.
#              (2) You need to drop an index that somehow got into the
#              database but doesn't exist in Schema.
# Params:      $table  - The name of the table the index is on.
#              $name   - The name of the index you're dropping.
#              $silent - (optional) If specified and true, don't output
#                        any message about this change.
# Returns:     nothing
#
sub bz_drop_index_raw {
    my ($self, $table, $name, $silent) = @_;
    my @statements = $self->_bz_schema->get_drop_index_ddl(
        $table, $name);
    print "Removing index '$name' from the $table table...\n" unless $silent;
    foreach my $sql (@statements) {
        # Because this is a deletion, we don't want to die hard if
        # we fail because of some local customization. If something
        # is already gone, that's fine with us!
        eval { $self->do($sql) } or warn "Failed SQL: [$sql] Error: $@";
    }
}

sub bz_drop_table {
    my ($self, $name) = @_;

    my $table_exists = $self->bz_table_info($name);

    if ($table_exists) {
        my @statements = $self->_bz_schema->get_drop_table_ddl($name);
        print "Dropping table $name...\n";
        foreach my $sql (@statements) {
            # Because this is a deletion, we don't want to die hard if
            # we fail because of some local customization. If something
            # is already gone, that's fine with us!
            eval { $self->do($sql); } or warn "Failed SQL: [$sql] Error: $@";
        }
        $self->_bz_real_schema->delete_table($name);
        $self->_bz_store_real_schema;
    }
}

sub bz_rename_column {
    my ($self, $table, $old_name, $new_name) = @_;

    my $old_col_exists  = $self->bz_column_info($table, $old_name);

    if ($old_col_exists) {
        my $already_renamed = $self->bz_column_info($table, $new_name);
        die "Name conflict: Cannot rename ${table}.${old_name} to"
            . " ${table}.${new_name},\nbecause ${table}.${new_name}"
            . " already exists." if $already_renamed;
        my @statements = $self->_bz_real_schema->get_rename_column_ddl(
            $table, $old_name, $new_name);
        print "Changing column $old_name in table $table to"
              . " be named $new_name...\n";
        foreach my $sql (@statements) {
            $self->do($sql);
        }
        $self->_bz_real_schema->rename_column($table, $old_name, $new_name);
        $self->_bz_store_real_schema;
    }
}

#####################################################################
# Schema Information Methods
#####################################################################

sub _bz_schema {
    my ($self) = @_;
    return $self->{private_bz_schema} if exists $self->{private_bz_schema};
    $self->{private_bz_schema} = 
        Bugzilla::DB::Schema->new($self->MODULE_NAME);
    return $self->{private_bz_schema};
}

# _bz_get_initial_schema()
#
# Description: A protected method, intended for use only by Bugzilla::DB
#              and subclasses. Used to get the initial Schema that will
#              be wirtten to disk for _bz_init_schema_storage. You probably
#              want to use _bz_schema or _bz_real_schema instead of this
#              method.
# Params:      none
# Returns:     A Schema object that can be serialized and written to disk
#              for _bz_init_schema_storage.
sub _bz_get_initial_schema {
    my ($self) = @_;
    return $self->_bz_schema->get_empty_schema();
}

sub bz_column_info {
    my ($self, $table, $column) = @_;
    return $self->_bz_real_schema->get_column_abstract($table, $column);
}

sub bz_index_info {
    my ($self, $table, $index) = @_;
    my $index_def =
        $self->_bz_real_schema->get_index_abstract($table, $index);
    if (ref($index_def) eq 'ARRAY') {
        $index_def = {FIELDS => $index_def, TYPE => ''};
    }
    return $index_def;
}

sub bz_table_info {
    my ($self, $table) = @_;
    return $self->_bz_real_schema->get_table_abstract($table);
}


sub bz_table_columns {
    my ($self, $table) = @_;
    return $self->_bz_schema->get_table_columns($table);
}

#####################################################################
# Protected "Real Database" Schema Information Methods
#####################################################################

# Only Bugzilla::DB and subclasses should use these methods.
# If you need a method that does the same thing as one of these
# methods, use the version without _real on the end.

# bz_table_columns_real($table)
#
# Description: Returns a list of columns on a given table
#              as the table actually is, on the disk.
# Params:      $table - Name of the table.
# Returns:     An array of column names.
#
sub bz_table_columns_real {
    my ($self, $table) = @_;
    my $sth = $self->column_info(undef, undef, $table, '%');
    return @{ $self->selectcol_arrayref($sth, {Columns => [4]}) };
}

# bz_table_list_real()
#
# Description: Gets a list of tables in the current
#              database, directly from the disk.
# Params:      none
# Returns:     An array containing table names.
sub bz_table_list_real {
    my ($self) = @_;
    my $table_sth = $self->table_info(undef, undef, undef, "TABLE");
    return @{$self->selectcol_arrayref($table_sth, { Columns => [3] })};
}

#####################################################################
# Transaction Methods
#####################################################################

sub bz_start_transaction {
    my ($self) = @_;

    if ($self->{private_bz_in_transaction}) {
        ThrowCodeError("nested_transaction");
    } else {
        # Turn AutoCommit off and start a new transaction
        $self->begin_work();
        $self->{private_bz_in_transaction} = 1;
    }
}

sub bz_commit_transaction {
    my ($self) = @_;

    if (!$self->{private_bz_in_transaction}) {
        ThrowCodeError("not_in_transaction");
    } else {
        $self->commit();

        $self->{private_bz_in_transaction} = 0;
    }
}

sub bz_rollback_transaction {
    my ($self) = @_;

    if (!$self->{private_bz_in_transaction}) {
        ThrowCodeError("not_in_transaction");
    } else {
        $self->rollback();

        $self->{private_bz_in_transaction} = 0;
    }
}

#####################################################################
# Subclass Helpers
#####################################################################

sub db_new {
    my ($class, $dsn, $user, $pass, $attributes) = @_;

    # set up default attributes used to connect to the database
    # (if not defined by DB specific implementation)
    $attributes = { RaiseError => 0,
                    AutoCommit => 1,
                    PrintError => 0,
                    ShowErrorStatement => 1,
                    HandleError => \&_handle_error,
                    TaintIn => 1,
                    FetchHashKeyName => 'NAME',  
                    # Note: NAME_lc causes crash on ActiveState Perl
                    # 5.8.4 (see Bug 253696)
                    # XXX - This will likely cause problems in DB
                    # back ends that twiddle column case (Oracle?)
                  } if (!defined($attributes));

    # connect using our known info to the specified db
    # Apache::DBI will cache this when using mod_perl
    my $self = DBI->connect($dsn, $user, $pass, $attributes)
        or die "\nCan't connect to the database.\nError: $DBI::errstr\n"
        . "  Is your database installed and up and running?\n  Do you have"
        . " the correct username and password selected in localconfig?\n\n";

    # RaiseError was only set to 0 so that we could catch the 
    # above "die" condition.
    $self->{RaiseError} = 1;

    # class variables
    $self->{private_bz_in_transaction} = 0;

    bless ($self, $class);

    return $self;
}

#####################################################################
# Private Methods
#####################################################################

=begin private

=head1 PRIVATE METHODS

These methods really are private. Do not override them in subclasses.

=over 4

=item C<_init_bz_schema_storage>

 Description: Initializes the bz_schema table if it contains nothing.
 Params:      none
 Returns:     nothing

=cut

sub _bz_init_schema_storage {
    my ($self) = @_;

    my $table_size;
    eval {
        $table_size = 
            $self->selectrow_array("SELECT COUNT(*) FROM bz_schema");
    };

    if (!$table_size) {
        my $init_schema = $self->_bz_get_initial_schema;
        my $store_me = $init_schema->serialize_abstract();
        my $schema_version = $init_schema->SCHEMA_VERSION;

        # If table_size is not defined, then we hit an error reading the
        # bz_schema table, which means it probably doesn't exist yet. So,
        # we have to create it. If we failed above for some other reason,
        # we'll see the failure here.
        # However, we must create the table after we do get_initial_schema,
        # because some versions of get_initial_schema read that the table
        # exists and then add it to the Schema, where other versions don't.
        if (!defined $table_size) {
            $self->_bz_add_table_raw('bz_schema');
        }

        print "Initializing the new Schema storage...\n";
        my $sth = $self->prepare("INSERT INTO bz_schema "
                                 ." (schema_data, version) VALUES (?,?)");
        $sth->bind_param(1, $store_me, $self->BLOB_TYPE);
        $sth->bind_param(2, $schema_version);
        $sth->execute();

        # And now we have to update the on-disk schema to hold the bz_schema
        # table, if the bz_schema table didn't exist when we were called.
        if (!defined $table_size) {
            $self->_bz_real_schema->add_table('bz_schema',
                $self->_bz_schema->get_table_abstract('bz_schema'));
            $self->_bz_store_real_schema;
        }
    } 
    # Sanity check
    elsif ($table_size > 1) {
        # We tell them to delete the newer one. Better to have checksetup
        # run migration code too many times than to have it not run the
        # correct migration code at all.
        die "Attempted to initialize the schema but there are already "
            . " $table_size copies of it stored.\nThis should never happen.\n"
            . " Compare the rows of the bz_schema table and delete the "
            . "newer one(s).";
    }
}

=item C<_bz_real_schema()>

 Description: Returns a Schema object representing the database
              that is being used in the current installation.
 Params:      none
 Returns:     A C<Bugzilla::DB::Schema> object representing the database
              as it exists on the disk.

=cut

sub _bz_real_schema {
    my ($self) = @_;
    return $self->{private_real_schema} if exists $self->{private_real_schema};

    my ($data, $version) = $self->selectrow_array(
        "SELECT schema_data, version FROM bz_schema");

    (die "_bz_real_schema tried to read the bz_schema table but it's empty!")
        if !$data;

    $self->{private_real_schema} = 
        $self->_bz_schema->deserialize_abstract($data, $version);

    return $self->{private_real_schema};
}

=item C<_bz_store_real_schema()>

 Description: Stores the _bz_real_schema structures in the database
              for later recovery. Call this function whenever you make
              a change to the _bz_real_schema.
 Params:      none
 Returns:     nothing

 Precondition: $self->{_bz_real_schema} must exist.

=back

=end private

=cut

sub _bz_store_real_schema {
    my ($self) = @_;

    # Make sure that there's a schema to update
    my $table_size = $self->selectrow_array("SELECT COUNT(*) FROM bz_schema");

    die "Attempted to update the bz_schema table but there's nothing "
        . "there to update. Run checksetup." unless $table_size;

    # We want to store the current object, not one
    # that we read from the database. So we use the actual hash
    # member instead of the subroutine call. If the hash
    # member is not defined, we will (and should) fail.
    my $update_schema = $self->{private_real_schema};
    my $store_me = $update_schema->serialize_abstract();
    my $schema_version = $update_schema->SCHEMA_VERSION;
    my $sth = $self->prepare("UPDATE bz_schema 
                                 SET schema_data = ?, version = ?");
    $sth->bind_param(1, $store_me, $self->BLOB_TYPE);
    $sth->bind_param(2, $schema_version);
    $sth->execute();
}

1;

__END__

=head1 NAME

Bugzilla::DB - Database access routines, using L<DBI>

=head1 SYNOPSIS

  # Obtain db handle
  use Bugzilla::DB;
  my $dbh = Bugzilla->dbh;

  # prepare a query using DB methods
  my $sth = $dbh->prepare("SELECT " .
                          $dbh->sql_date_format("creation_ts", "%Y%m%d") .
                          " FROM bugs WHERE bug_status != 'RESOLVED' " .
                          $dbh->sql_limit(1));

  # Execute the query
  $sth->execute;

  # Get the results
  my @result = $sth->fetchrow_array;

  # Schema Modification
  $dbh->bz_add_column($table, $name, \%definition, $init_value);
  $dbh->bz_add_index($table, $name, $definition);
  $dbh->bz_add_table($name);
  $dbh->bz_drop_index($table, $name);
  $dbh->bz_drop_table($name);
  $dbh->bz_alter_column($table, $name, \%new_def, $set_nulls_to);
  $dbh->bz_drop_column($table, $column);
  $dbh->bz_rename_column($table, $old_name, $new_name);

  # Schema Information
  my $column = $dbh->bz_column_info($table, $column);
  my $index  = $dbh->bz_index_info($table, $index);

  # General Information
  my @fields    = $dbh->bz_get_field_defs();

=head1 DESCRIPTION

Functions in this module allows creation of a database handle to connect
to the Bugzilla database. This should never be done directly; all users
should use the L<Bugzilla> module to access the current C<dbh> instead.

This module also contains methods extending the returned handle with
functionality which is different between databases allowing for easy
customization for particular database via inheritance. These methods
should be always preffered over hard-coding SQL commands.

Access to the old SendSQL-based database routines are also provided by
importing the C<:deprecated> tag. These routines should not be used in new
code.

=head1 CONSTANTS

Subclasses of Bugzilla::DB are required to define certain constants. These
constants are required to be subroutines or "use constant" variables.

=over 4

=item C<BLOB_TYPE>

The C<\%attr> argument that must be passed to bind_param in order to 
correctly escape a C<LONGBLOB> type.

=item C<REQUIRED_VERSION>

This is the minimum required version of the database server that the
Bugzilla::DB subclass requires. It should be in a format suitable for
passing to vers_cmp during installation.

=item C<PROGRAM_NAME>

The human-readable name of this database. For example, for MySQL, this
would be 'MySQL.' You should not depend on this variable to know what
type of database you are running on; this is intended merely for displaying
to the admin to let them know what DB they're running.

=item C<MODULE_NAME>

The name of the Bugzilla::DB module that we are. For example, for the MySQL
Bugzilla::DB module, this would be "Mysql." For PostgreSQL it would be "Pg."

=item C<DBD_VERSION>

The minimum version of the DBD module that we require for this database.

=back


=head1 CONNECTION

A new database handle to the required database can be created using this
module. This is normally done by the L<Bugzilla> module, and so these routines
should not be called from anywhere else.

=head2 Functions

=over 4

=item C<connect_main>

 Description: Function to connect to the main database, returning a new
              database handle.
 Params:      $no_db_name (optional) - If true, Connect to the database 
                  server, but don't connect to a specific database. This 
                  is only used when creating a database. After you create
                  the database, you should re-create a new Bugzilla::DB object
                  without using this parameter. 
 Returns:     new instance of the DB class

=item C<connect_shadow>

 Description: Function to connect to the shadow database, returning a new
              database handle.
              This routine C<die>s if no shadow database is configured.
 Params:      none
 Returns:     new instance of the DB class

=item C<_connect>

 Description: Internal function, creates and returns a new, connected
              instance of the correct DB class.
              This routine C<die>s if no driver is specified.
 Params:      $driver = name of the database driver to use
              $host = host running the database we are connecting to
              $dbname = name of the database to connect to
              $port = port the database is listening on
              $sock = socket the database is listening on
              $user = username used to log in to the database
              $pass = password used to log in to the database
 Returns:     new instance of the DB class

=item C<_handle_error>

 Description: Function passed to the DBI::connect call for error handling.
              It shortens the error for printing.

=item C<import>

 Description: Overrides the standard import method to check that derived class
              implements all required abstract methods. Also calls original
              implementation in its super class.

=back


=head1 ABSTRACT METHODS

Note: Methods which can be implemented generically for all DBs are implemented in
this module. If needed, they can be overriden with DB specific code.
Methods which do not have standard implementation are abstract and must
be implemented for all supported databases separately.
To avoid confusion with standard DBI methods, all methods returning string with
formatted SQL command have prefix C<sql_>. All other methods have prefix C<bz_>.

=over 4

=item C<new>

 Description: Constructor
              Abstract method, should be overriden by database specific code.
 Params:      $user = username used to log in to the database
              $pass = password used to log in to the database
              $host = host running the database we are connecting to
              $dbname = name of the database to connect to
              $port = port the database is listening on
              $sock = socket the database is listening on
 Returns:     new instance of the DB class
 Note:        The constructor should create a DSN from the parameters provided and
              then call C<db_new()> method of its super class to create a new
              class instance. See C<db_new> description in this module. As per
              DBI documentation, all class variables must be prefixed with
              "private_". See L<DBI>.

=item C<sql_regexp>

 Description: Outputs SQL regular expression operator for POSIX regex
              searches (case insensitive) in format suitable for a given
              database.
              Abstract method, should be overriden by database specific code.
 Params:      none
 Returns:     formatted SQL for regular expression search (e.g. REGEXP)
              (scalar)

=item C<sql_not_regexp>

 Description: Outputs SQL regular expression operator for negative POSIX
              regex searches (case insensitive) in format suitable for a given
              database.
              Abstract method, should be overriden by database specific code.
 Params:      none
 Returns:     formatted SQL for negative regular expression search
              (e.g. NOT REGEXP) (scalar)

=item C<sql_limit>

 Description: Returns SQL syntax for limiting results to some number of rows
              with optional offset if not starting from the begining.
              Abstract method, should be overriden by database specific code.
 Params:      $limit = number of rows to return from query (scalar)
              $offset = number of rows to skip prior counting (scalar)
 Returns:     formatted SQL for limiting number of rows returned from query
              with optional offset (e.g. LIMIT 1, 1) (scalar)

=item C<sql_from_days>

 Description: Outputs SQL syntax for converting Julian days to date.
              Abstract method, should be overriden by database specific code.
 Params:      $days = days to convert to date
 Returns:     formatted SQL for returning Julian days in dates. (scalar)

=item C<sql_to_days>

 Description: Outputs SQL syntax for converting date to Julian days.
              Abstract method, should be overriden by database specific code.
 Params:      $date = date to convert to days
 Returns:     formatted SQL for returning date fields in Julian days. (scalar)

=item C<sql_date_format>

 Description: Outputs SQL syntax for formatting dates.
              Abstract method, should be overriden by database specific code.
 Params:      $date = date or name of date type column (scalar)
              $format = format string for date output (scalar)
              (%Y = year, four digits, %y = year, two digits, %m = month,
               %d = day, %a = weekday name, 3 letters, %H = hour 00-23,
               %i = minute, %s = second)
 Returns:     formatted SQL for date formatting (scalar)

=item C<sql_interval>

 Description: Outputs proper SQL syntax for a time interval function.
              Abstract method, should be overriden by database specific code.
 Params:      $interval - the time interval requested (e.g. '30') (integer)
              $units    - the units the interval is in (e.g. 'MINUTE') (string)
 Returns:     formatted SQL for interval function (scalar)

=item C<sql_position>

 Description: Outputs proper SQL syntax determinig position of a substring
              (fragment) withing a string (text). Note: if the substring or
              text are string constants, they must be properly quoted
              (e.g. "'pattern'").
 Params:      $fragment = the string fragment we are searching for (scalar)
              $text = the text to search (scalar)
 Returns:     formatted SQL for substring search (scalar)

=item C<sql_group_by>

 Description: Outputs proper SQL syntax for grouping the result of a query.
              For ANSI SQL databases, we need to group by all columns we are
              querying for (except for columns used in aggregate functions).
              Some databases require (or even allow) to specify only one
              or few columns if the result is uniquely defined. For those
              databases, the default implementation needs to be overloaded.
 Params:      $needed_columns = string with comma separated list of columns
              we need to group by to get expected result (scalar)
              $optional_columns = string with comma separated list of all
              other columns we are querying for, but which are not in the
              required list.
 Returns:     formatted SQL for row grouping (scalar)

=item C<sql_string_concat>

 Description: Returns SQL syntax for concatenating multiple strings (constants
              or values from table columns) together.
 Params:      @params = array of column names or strings to concatenate
 Returns:     formatted SQL for concatenating specified strings

=item C<sql_fulltext_search>

 Description: Returns SQL syntax for performing a full text search for
              specified text on a given column.
              There is a ANSI SQL version of this method implemented using
              LIKE operator, but it's not a real full text search. DB specific
              modules shoud override this, as this generic implementation will
              be always much slower. This generic implementation returns
              'relevance' as 0 for no match, or 1 for a match.
 Params:      $column = name of column to search (scalar)
              $text = text to search for (scalar)
 Returns:     formatted SQL for for full text search

=item C<sql_istrcmp>

 Description: Returns SQL for a case-insensitive string comparison.
 Params:      $left - What should be on the left-hand-side of the
                      operation.
              $right - What should be on the right-hand-side of the
                       operation.
              $op (optional) - What the operation is. Should be a 
                  valid ANSI SQL comparison operator, like "=", "<", 
                  "LIKE", etc. Defaults to "=" if not specified.
 Returns:     A SQL statement that will run the comparison in 
              a case-insensitive fashion.
 Note:        Uses sql_istring, so it has the same performance concerns.
              Try to avoid using this function unless absolutely necessary.
              Subclass Implementors: Override sql_istring instead of this
              function, most of the time (this function uses sql_istring).

=item C<sql_istring>

 Description: Returns SQL syntax "preparing" a string or text column for
              case-insensitive comparison. 
 Params:      $string - string to convert (scalar)
 Returns:     formatted SQL making the string case insensitive
 Note:        The default implementation simply calls LOWER on the parameter.
              If this is used to search on a text column with index, the index
              will not be usually used unless it was created as LOWER(column).

=item C<bz_lock_tables>

 Description: Performs a table lock operation on specified tables.
              If the underlying database supports transactions, it should also
              implicitly start a new transaction.
              Abstract method, should be overriden by database specific code.
 Params:      @tables = list of names of tables to lock in MySQL
              notation (ex. 'bugs AS bugs2 READ', 'logincookies WRITE')
 Returns:     none

=item C<bz_unlock_tables>

 Description: Performs a table unlock operation
              If the underlying database supports transactions, it should also
              implicitly commit or rollback the transaction.
              Also, this function should allow to be called with the abort flag
              set even without locking tables first without raising an error
              to simplify error handling.
              Abstract method, should be overriden by database specific code.
 Params:      $abort = UNLOCK_ABORT (true, 1) if the operation on locked tables
              failed (if transactions are supported, the action will be rolled
              back). False (0) or no param if the operation succeeded.
 Returns:     none

=back


=head1 IMPLEMENTED METHODS

These methods are implemented in Bugzilla::DB, and only need
to be implemented in subclasses if you need to override them for 
database-compatibility reasons.

=head2 General Information Methods

These methods return information about data in the database.

=over 4

=item C<bz_last_key>

 Description: Returns the last serial number, usually from a previous INSERT.
              Must be executed directly following the relevant INSERT.
              This base implementation uses DBI->last_insert_id. If the
              DBD supports it, it is the preffered way to obtain the last
              serial index. If it is not supported, the DB specific code
              needs to override it with DB specific code.
 Params:      $table = name of table containing serial column (scalar)
              $column = name of column containing serial data type (scalar)
 Returns:     Last inserted ID (scalar)

=back


=head2 Database Setup Methods

These methods are used by the Bugzilla installation programs to set up
the database.

=over 4

=item C<bz_enum_initial_values(\%enum_defaults)>

 Description: For an upgrade or an initial installation, provides
              what the values should be for the "enum"-type fields,
              such as version, op_sys, rep_platform, etc.
 Params:      \%enum_defaults - The default initial list of values for
                                each enum field. A hash, with the field
                                names pointing to an arrayref of values.
 Returns:     A hashref with the correct initial values for the enum fields.

=back


=head2 Schema Modification Methods

These methods modify the current Bugzilla Schema.

Where a parameter says "Abstract index/column definition", it returns/takes
information in the formats defined for indexes and columns in
C<Bugzilla::DB::Schema::ABSTRACT_SCHEMA>.

=over 4

=item C<bz_add_column($table, $name, \%definition, $init_value)>

 Description: Adds a new column to a table in the database. Prints out
              a brief statement that it did so, to stdout.
              Note that you cannot add a NOT NULL column that has no
              default -- the database won't know what to set all
              the NOT NULL values to.
 Params:      $table = the table where the column is being added
              $name  = the name of the new column
              \%definition = Abstract column definition for the new column
              $init_value = (optional) An initial value to set the column
                            to. Required if your column is NOT NULL and has
                            no DEFAULT set.
 Returns:     nothing

=item C<bz_add_index($table, $name, $definition)>

 Description: Adds a new index to a table in the database. Prints
              out a brief statement that it did so, to stdout.
              If the index already exists, we will do nothing.
 Params:      $table - The table the new index is on.
              $name  - A name for the new index.
              $definition - An abstract index definition. 
                            Either a hashref or an arrayref.
 Returns:     nothing

=item C<bz_add_table($name)>

 Description: Creates a new table in the database, based on the
              definition for that table in the abstract schema.
              Note that unlike the other 'add' functions, this does
              not take a definition, but always creates the table
              as it exists in the ABSTRACT_SCHEMA.
              If a table with that name already exists, then this
              function returns silently.
 Params:      $name - The name of the table you want to create.
 Returns:     nothing

=item C<bz_drop_index($table, $name)>

 Description: Removes an index from the database. Prints out a brief
              statement that it did so, to stdout. If the index
              doesn't exist, we do nothing.
 Params:      $table - The table that the index is on.
              $name  - The name of the index that you want to drop.
 Returns:     nothing

=item C<bz_drop_table($name)>

 Description: Drops a table from the database. If the table
              doesn't exist, we just return silently.
 Params:      $name - The name of the table to drop.
 Returns:     nothing

=item C<bz_alter_column($table, $name, \%new_def, $set_nulls_to)>

 Description: Changes the data type of a column in a table. Prints out
              the changes being made to stdout. If the new type is the
              same as the old type, the function returns without changing
              anything.
 Params:      $table   = the table where the column is
              $name    = the name of the column you want to change
              $new_def = An abstract column definition for the new 
                         data type of the columm
              $set_nulls_to = (Optional) If you are changing the column
                              to be NOT NULL, you probably also want to
                              set any existing NULL columns to a particular
                              value. Specify that value here. 
                              NOTE: The value should not already be SQL-quoted.
 Returns:     nothing

=item C<bz_drop_column($table, $column)>

 Description: Removes a column from a database table. If the column
              doesn't exist, we return without doing anything. If we do
              anything, we print a short message to stdout about the change.
 Params:      $table  = The table where the column is
              $column = The name of the column you want to drop
 Returns:     none

=item C<bz_rename_column($table, $old_name, $new_name)>

 Description: Renames a column in a database table. If the C<$old_name>
              column doesn't exist, we return without doing anything.
              If C<$old_name> and C<$new_name> both already exist in the
              table specified, we fail.
 Params:      $table    = The table containing the column 
                          that you want to rename
              $old_name = The current name of the column that 
                          you want to rename
              $new_name = The new name of the column
 Returns:     nothing

=back


=head2 Schema Information Methods

These methods return information about the current Bugzilla database
schema, as it currently exists on the disk. 

Where a parameter says "Abstract index/column definition", it returns/takes
information in the formats defined for indexes and columns in
C<Bugzilla::DB::Schema::ABSTRACT_SCHEMA>.

=over 4

=item C<bz_column_info($table, $column)>

 Description: Get abstract column definition.
 Params:      $table - The name of the table the column is in.
              $column - The name of the column.
 Returns:     An abstract column definition for that column.
              If the table or column does not exist, we return undef.

=item C<bz_index_info($table, $index)>

 Description: Get abstract index definition.
 Params:      $table - The table the index is on.
              $index - The name of the index.
 Returns:     An abstract index definition for that index,
              always in hashref format. The hashref will
              always contain the TYPE element, but it will
              be an empty string if it's just a normal index.
              If the index does not exist, we return undef.

=back


=head2 Deprecated Schema Information Methods

These methods return info about the current Bugzilla database, for
MySQL only.

=over 4

=item C<bz_get_field_defs>

 Description: Returns a list of all the "bug" fields in Bugzilla. The list
              contains hashes, with a 'name' key and a 'description' key.
 Params:      none
 Returns:     List of all the "bug" fields

=back


=head2 Transaction Methods

These methods deal with the starting and stopping of transactions 
in the database.

=over 4

=item C<bz_start_transaction>

 Description: Starts a transaction if supported by the database being used
 Params:      none
 Returns:     none

=item C<bz_commit_transaction>

 Description: Ends a transaction, commiting all changes, if supported by
              the database being used
 Params:      none
 Returns:     none

=item C<bz_rollback_transaction>

 Description: Ends a transaction, rolling back all changes, if supported by
              the database being used
 Params:      none
 Returns:     none

=back


=head1 SUBCLASS HELPERS

Methods in this class are intended to be used by subclasses to help them
with their functions.

=over 4

=item C<db_new>

 Description: Constructor
 Params:      $dsn = database connection string
              $user = username used to log in to the database
              $pass = password used to log in to the database
              $attributes = set of attributes for DB connection (optional)
 Returns:     new instance of the DB class
 Note:        the name of this constructor is not new, as that would make
              our check for implementation of new() by derived class useles.

=back


=head1 DEPRECATED ROUTINES

Several database routines are deprecated. They should not be used in new code,
and so are not documented.

=over 4

=item *

SendSQL

=item *

SqlQuote

=item *

MoreSQLData

=item *

FetchSQLData

=item *

FetchOneColumn

=item *

PushGlobalSQLState

=item *

PopGlobalSQLState

=back


=head1 SEE ALSO

L<DBI>

=cut
