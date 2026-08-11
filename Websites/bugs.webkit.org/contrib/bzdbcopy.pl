#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib);
use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::DB;
use Bugzilla::Install::Util qw(indicate_progress);
use Bugzilla::Util;

#####################################################################
# User-Configurable Settings
#####################################################################

# Settings for the 'Source' DB that you are copying from.
use constant SOURCE_DB_TYPE => 'Mysql';
use constant SOURCE_DB_NAME => 'bugs';
use constant SOURCE_DB_USER => 'bugs';
use constant SOURCE_DB_PASSWORD => '';
use constant SOURCE_DB_HOST => 'localhost';

# Settings for the 'Target' DB that you are copying to.
use constant TARGET_DB_TYPE => 'Pg';
use constant TARGET_DB_NAME => 'bugs';
use constant TARGET_DB_USER => 'bugs';
use constant TARGET_DB_PASSWORD => '';
use constant TARGET_DB_HOST => 'localhost';

#####################################################################
# MAIN SCRIPT
#####################################################################

print "Connecting to the '" . SOURCE_DB_NAME . "' source database on " 
      . SOURCE_DB_TYPE . "...\n";
my $source_db = Bugzilla::DB::_connect({
    db_driver => SOURCE_DB_TYPE, 
    db_host   => SOURCE_DB_HOST, 
    db_name   => SOURCE_DB_NAME, 
    db_user   => SOURCE_DB_USER, 
    db_pass   => SOURCE_DB_PASSWORD,
});
# Don't read entire tables into memory.
if (SOURCE_DB_TYPE eq 'Mysql') {
    $source_db->{'mysql_use_result'} = 1;

    # MySQL cannot have two queries running at the same time. Ensure the schema
    # is loaded from the database so bz_column_info will not execute a query
    $source_db->_bz_real_schema;
}

print "Connecting to the '" . TARGET_DB_NAME . "' target database on "
      . TARGET_DB_TYPE . "...\n";
my $target_db = Bugzilla::DB::_connect({
    db_driver => TARGET_DB_TYPE,
    db_host   => TARGET_DB_HOST, 
    db_name   => TARGET_DB_NAME, 
    db_user   => TARGET_DB_USER,
    db_pass   => TARGET_DB_PASSWORD,
});
my $ident_char = $target_db->get_info( 29 ); # SQL_IDENTIFIER_QUOTE_CHAR

# We use the table list from the target DB, because if somebody
# has customized their source DB, we still want the script to work,
# and it may otherwise fail in that situation (that is, the tables
# may not exist in the target DB).
#
# We don't want to copy over the bz_schema table's contents, though.
my @table_list = grep { $_ ne 'bz_schema' } $target_db->bz_table_list_real();

# Instead of figuring out some fancy algorithm to insert data in the right
# order and not break FK integrity, we just drop them all.
$target_db->bz_drop_foreign_keys();
# We start a transaction on the target DB, which helps when we're doing
# so many inserts.
$target_db->bz_start_transaction();
foreach my $table (@table_list) {
    my @serial_cols;
    print "Reading data from the source '$table' table on " 
          . SOURCE_DB_TYPE . "...\n";
    my @table_columns = $target_db->bz_table_columns_real($table);
    # The column names could be quoted using the quote identifier char
    # Remove these chars as different databases use different quote chars
    @table_columns = map { s/^\Q$ident_char\E?(.*?)\Q$ident_char\E?$/$1/; $_ }
                         @table_columns;

    my ($total) = $source_db->selectrow_array("SELECT COUNT(*) FROM $table");
    my $select_query = "SELECT " . join(',', @table_columns) . " FROM $table";
    my $select_sth = $source_db->prepare($select_query);
    $select_sth->execute();

    my $insert_query = "INSERT INTO $table ( " . join(',', @table_columns) 
                       . " ) VALUES (";
    $insert_query .= '?,' foreach (@table_columns);
    # Remove the last comma.
    chop($insert_query);
    $insert_query .= ")";
    my $insert_sth = $target_db->prepare($insert_query);

    print "Clearing out the target '$table' table on " 
          . TARGET_DB_TYPE . "...\n";
    $target_db->do("DELETE FROM $table");

    # Oracle doesn't like us manually inserting into tables that have
    # auto-increment PKs set, because of the way we made auto-increment
    # fields work.
    if ($target_db->isa('Bugzilla::DB::Oracle')) {
        foreach my $column (@table_columns) {
            my $col_info = $source_db->bz_column_info($table, $column);
            if ($col_info && $col_info->{TYPE} =~ /SERIAL/i) {
                print "Dropping the sequence + trigger on $table.$column...\n";
                $target_db->do("DROP TRIGGER ${table}_${column}_TR");
                $target_db->do("DROP SEQUENCE ${table}_${column}_SEQ");
            }
        }
    }
    
    print "Writing data to the target '$table' table on " 
          . TARGET_DB_TYPE . "...\n";
    my $count = 0;
    while (my $row = $select_sth->fetchrow_arrayref) {
        # Each column needs to be bound separately, because
        # many columns need to be dealt with specially.
        my $colnum = 0;
        foreach my $column (@table_columns) {
            # bind_param args start at 1, but arrays start at 0.
            my $param_num = $colnum + 1;
            my $already_bound;

            # Certain types of columns need special handling.
            my $col_info = $source_db->bz_column_info($table, $column);
            if ($col_info && $col_info->{TYPE} eq 'LONGBLOB') {
                $insert_sth->bind_param($param_num, 
                    $row->[$colnum], $target_db->BLOB_TYPE);
                $already_bound = 1;
            }
            elsif ($col_info && $col_info->{TYPE} =~ /decimal/) {
                # In MySQL, decimal cols can be too long.
                my $col_type = $col_info->{TYPE};
                $col_type =~ /decimal\((\d+),(\d+)\)/;
                my ($precision, $decimals) = ($1, $2);
                # If it's longer than precision + decimal point
                if ( length($row->[$colnum]) > ($precision + 1) ) {
                    # Truncate it to the highest allowed value.
                    my $orig_value = $row->[$colnum];
                    $row->[$colnum] = '';
                    my $non_decimal = $precision - $decimals;
                    $row->[$colnum] .= '9' while ($non_decimal--);
                    $row->[$colnum] .= '.';
                    $row->[$colnum] .= '9' while ($decimals--);
                    print "Truncated value $orig_value to " . $row->[$colnum] 
                         . " for $table.$column.\n";
                }
            }
            elsif ($col_info && $col_info->{TYPE} =~ /DATETIME/i) {
                my $date = $row->[$colnum];
                # MySQL can have strange invalid values for Datetimes.
                $row->[$colnum] = '1901-01-01 00:00:00'
                    if $date && $date eq '0000-00-00 00:00:00';
            }

            $insert_sth->bind_param($param_num, $row->[$colnum])
                unless $already_bound;
            $colnum++;
        }

        $insert_sth->execute();
        $count++;
        indicate_progress({ current => $count, total => $total, every => 100 });
    }

    # For some DBs, we have to do clever things with auto-increment fields.
    foreach my $column (@table_columns) {
        next if $target_db->isa('Bugzilla::DB::Mysql');
        my $col_info = $source_db->bz_column_info($table, $column);
        if ($col_info && $col_info->{TYPE} =~ /SERIAL/i) {
            my ($max_val) = $target_db->selectrow_array(
                    "SELECT MAX($column) FROM $table");
            # Set the sequence to the current max value + 1.
            $max_val = 0 if !defined $max_val;
            $max_val++;
            print "\nSetting the next value for $table.$column to $max_val.";
            if ($target_db->isa('Bugzilla::DB::Pg')) {
                # PostgreSQL doesn't like it when you insert values into
                # a serial field; it doesn't increment the counter 
                # automatically.
                $target_db->bz_set_next_serial_value($table, $column);
            }
            elsif ($target_db->isa('Bugzilla::DB::Oracle')) {
                # Oracle increments the counter on every insert, and *always*
                # sets the field, even if you gave it a value. So if there
                # were already rows in the target DB (like the default rows
                # created by checksetup), you'll get crazy values in your
                # id columns. So we just dropped the sequences above and
                # we re-create them here, starting with the right number.
                my @sql = $target_db->_bz_real_schema->_get_create_seq_ddl(
                    $table, $column, $max_val);
                $target_db->do($_) foreach @sql;
            }
        }
    }

    print "\n\n";
}

print "Committing changes to the target database...\n";
$target_db->bz_commit_transaction();
$target_db->bz_setup_foreign_keys();

print "All done! Make sure to run checksetup.pl on the new DB.\n";
$source_db->disconnect;
$target_db->disconnect;

1;

__END__

=head1 NAME

bzdbcopy.pl - Copies data from one Bugzilla database to another. 

=head1 DESCRIPTION

The intended use of this script is to copy data from an installation
running on one DB platform to an installation running on another
DB platform.

It must be run from the directory containing your Bugzilla installation.
That means if this script is in the contrib/ directory, you should
be running it as: C<./contrib/bzdbcopy.pl>

Note: Both schemas must already exist and be B<IDENTICAL>. (That is, 
they must have both been created/updated by the same version of 
checksetup.pl.) This script will B<DESTROY ALL CURRENT DATA> in the 
target database.

Both Schemas must be at least from Bugzilla 2.19.3, but if you're
running a Bugzilla from before 2.20rc2, you'll need the patch at:
L<http://bugzilla.mozilla.org/show_bug.cgi?id=300311> in order to
be able to run this script.

Before you using it, you have to correctly set all the variables
in the "User-Configurable Settings" section at the top of the script. 
The C<SOURCE> settings are for the database you're copying from, and 
the C<TARGET> settings are for the database you're copying to. The 
C<DB_TYPE> is the name of a DB driver from the F<Bugzilla/DB/> directory.
