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
# Contributor(s): Dave Miller <davem00@aol.com>
#                 Gayathri Swaminath <gayathrik00@aol.com>
#                 Jeroen Ruigrok van der Werven <asmodai@wxs.nl>
#                 Dave Lawrence <dkl@redhat.com>
#                 Tomas Kopal <Tomas.Kopal@altap.cz>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Lance Larsh <lance.larsh@oracle.com>

=head1 NAME

Bugzilla::DB::Pg - Bugzilla database compatibility layer for PostgreSQL

=head1 DESCRIPTION

This module overrides methods of the Bugzilla::DB module with PostgreSQL
specific implementation. It is instantiated by the Bugzilla::DB module
and should never be used directly.

For interface details see L<Bugzilla::DB> and L<DBI>.

=cut

package Bugzilla::DB::Pg;

use strict;

use Bugzilla::Error;
use DBD::Pg;

# This module extends the DB interface via inheritance
use base qw(Bugzilla::DB);

use constant BLOB_TYPE => { pg_type => DBD::Pg::PG_BYTEA };

sub new {
    my ($class, $params) = @_;
    my ($user, $pass, $host, $dbname, $port) = 
        @$params{qw(db_user db_pass db_host db_name db_port)};

    # The default database name for PostgreSQL. We have
    # to connect to SOME database, even if we have
    # no $dbname parameter.
    $dbname ||= 'template1';

    # construct the DSN from the parameters we got
    my $dsn = "dbi:Pg:dbname=$dbname";
    $dsn .= ";host=$host" if $host;
    $dsn .= ";port=$port" if $port;

    # This stops Pg from printing out lots of "NOTICE" messages when
    # creating tables.
    $dsn .= ";options='-c client_min_messages=warning'";

    my $attrs = { pg_enable_utf8 => Bugzilla->params->{'utf8'} };

    my $self = $class->db_new({ dsn => $dsn, user => $user, 
                                pass => $pass, attrs => $attrs });

    # all class local variables stored in DBI derived class needs to have
    # a prefix 'private_'. See DBI documentation.
    $self->{private_bz_tables_locked} = "";
    # Needed by TheSchwartz
    $self->{private_bz_dsn} = $dsn;

    bless ($self, $class);

    return $self;
}

# if last_insert_id is supported on PostgreSQL by lowest DBI/DBD version
# supported by Bugzilla, this implementation can be removed.
sub bz_last_key {
    my ($self, $table, $column) = @_;

    my $seq = $table . "_" . $column . "_seq";
    my ($last_insert_id) = $self->selectrow_array("SELECT CURRVAL('$seq')");

    return $last_insert_id;
}

sub sql_group_concat {
    my ($self, $text, $separator, $sort) = @_;
    $sort = 1 if !defined $sort;
    $separator = $self->quote(', ') if !defined $separator;
    my $sql = "array_accum($text)";
    if ($sort) {
        $sql = "array_sort($sql)";
    }
    return "array_to_string($sql, $separator)";
}

sub sql_istring {
    my ($self, $string) = @_;

    return "LOWER(${string}::text)";
}

sub sql_position {
    my ($self, $fragment, $text) = @_;

    return "POSITION(${fragment}::text IN ${text}::text)";
}

sub sql_regexp {
    my ($self, $expr, $pattern, $nocheck, $real_pattern) = @_;
    $real_pattern ||= $pattern;

    $self->bz_check_regexp($real_pattern) if !$nocheck;

    return "${expr}::text ~* $pattern";
}

sub sql_not_regexp {
    my ($self, $expr, $pattern, $nocheck, $real_pattern) = @_;
    $real_pattern ||= $pattern;

    $self->bz_check_regexp($real_pattern) if !$nocheck;

    return "${expr}::text !~* $pattern" 
}

sub sql_limit {
    my ($self, $limit, $offset) = @_;

    if (defined($offset)) {
        return "LIMIT $limit OFFSET $offset";
    } else {
        return "LIMIT $limit";
    }
}

sub sql_from_days {
    my ($self, $days) = @_;

    return "TO_TIMESTAMP('$days', 'J')::date";
}

sub sql_to_days {
    my ($self, $date) = @_;

    return "TO_CHAR(${date}::date, 'J')::int";
}

sub sql_date_format {
    my ($self, $date, $format) = @_;
    
    $format = "%Y.%m.%d %H:%i:%s" if !$format;

    $format =~ s/\%Y/YYYY/g;
    $format =~ s/\%y/YY/g;
    $format =~ s/\%m/MM/g;
    $format =~ s/\%d/DD/g;
    $format =~ s/\%a/Dy/g;
    $format =~ s/\%H/HH24/g;
    $format =~ s/\%i/MI/g;
    $format =~ s/\%s/SS/g;

    return "TO_CHAR($date, " . $self->quote($format) . ")";
}

sub sql_date_math {
    my ($self, $date, $operator, $interval, $units) = @_;
    
    return "$date $operator $interval * INTERVAL '1 $units'";
}

sub sql_string_concat {
    my ($self, @params) = @_;
    
    # Postgres 7.3 does not support concatenating of different types, so we
    # need to cast both parameters to text. Version 7.4 seems to handle this
    # properly, so when we stop support 7.3, this can be removed.
    return '(CAST(' . join(' AS text) || CAST(', @params) . ' AS text))';
}

# Tell us whether or not a particular sequence exists in the DB.
sub bz_sequence_exists {
    my ($self, $seq_name) = @_;
    my $exists = $self->selectrow_array(
        'SELECT 1 FROM pg_statio_user_sequences WHERE relname = ?',
        undef, $seq_name);
    return $exists || 0;
}

sub bz_explain {
    my ($self, $sql) = @_;
    my $explain = $self->selectcol_arrayref("EXPLAIN ANALYZE $sql");
    return join("\n", @$explain);
}

#####################################################################
# Custom Database Setup
#####################################################################

sub bz_check_server_version {
    my $self = shift;
    my ($db) = @_;
    my $server_version = $self->SUPER::bz_check_server_version(@_);
    my ($major_version) = $server_version =~ /^(\d+)/;
    # Pg 9 requires DBD::Pg 2.17.2 in order to properly read bytea values.
    if ($major_version >= 9) {
        local $db->{dbd}->{version} = '2.17.2';
        local $db->{name} = $db->{name} . ' 9+';
        Bugzilla::DB::_bz_check_dbd(@_);
    }
}

sub bz_setup_database {
    my $self = shift;
    $self->SUPER::bz_setup_database(@_);

    # Custom Functions
    my $function = 'array_accum';
    my $array_accum = $self->selectrow_array(
        'SELECT 1 FROM pg_proc WHERE proname = ?', undef, $function);
    if (!$array_accum) {
        print "Creating function $function...\n";
        $self->do("CREATE AGGREGATE array_accum (
                       SFUNC = array_append,
                       BASETYPE = anyelement,
                       STYPE = anyarray,
                       INITCOND = '{}' 
                   )");
    }

   $self->do(<<'END');
CREATE OR REPLACE FUNCTION array_sort(ANYARRAY)
RETURNS ANYARRAY LANGUAGE SQL
IMMUTABLE STRICT
AS $$
SELECT ARRAY(
    SELECT $1[s.i] AS each_item
    FROM
        generate_series(array_lower($1,1), array_upper($1,1)) AS s(i)
    ORDER BY each_item
);
$$;
END

    # PostgreSQL doesn't like having *any* index on the thetext
    # field, because it can't have index data longer than 2770
    # characters on that field.
    $self->bz_drop_index('longdescs', 'longdescs_thetext_idx');
    # Same for all the comments fields in the fulltext table.
    $self->bz_drop_index('bugs_fulltext', 'bugs_fulltext_comments_idx');
    $self->bz_drop_index('bugs_fulltext', 
                         'bugs_fulltext_comments_noprivate_idx');

    # PostgreSQL also wants an index for calling LOWER on
    # login_name, which we do with sql_istrcmp all over the place.
    $self->bz_add_index('profiles', 'profiles_login_name_lower_idx', 
        {FIELDS => ['LOWER(login_name)'], TYPE => 'UNIQUE'});

    # Now that Bugzilla::Object uses sql_istrcmp, other tables
    # also need a LOWER() index.
    _fix_case_differences('fielddefs', 'name');
    $self->bz_add_index('fielddefs', 'fielddefs_name_lower_idx',
        {FIELDS => ['LOWER(name)'], TYPE => 'UNIQUE'});
    _fix_case_differences('keyworddefs', 'name');
    $self->bz_add_index('keyworddefs', 'keyworddefs_name_lower_idx',
        {FIELDS => ['LOWER(name)'], TYPE => 'UNIQUE'});
    _fix_case_differences('products', 'name');
    $self->bz_add_index('products', 'products_name_lower_idx',
        {FIELDS => ['LOWER(name)'], TYPE => 'UNIQUE'});

    # bz_rename_column and bz_rename_table didn't correctly rename
    # the sequence.
    $self->_fix_bad_sequence('fielddefs', 'id', 'fielddefs_fieldid_seq', 'fielddefs_id_seq');
    # If the 'tags' table still exists, then bz_rename_table()
    # will fix the sequence for us.
    if (!$self->bz_table_info('tags')) {
        my $res = $self->_fix_bad_sequence('tag', 'id', 'tags_id_seq', 'tag_id_seq');
        # If $res is true, then the sequence has been renamed, meaning that
        # the primary key must be renamed too.
        if ($res) {
            $self->do('ALTER INDEX tags_pkey RENAME TO tag_pkey');
        }
    }

    # Certain sequences got upgraded before we required Pg 8.3, and
    # so they were not properly associated with their columns.
    my @tables = $self->bz_table_list_real;
    foreach my $table (@tables) {
        my @columns = $self->bz_table_columns_real($table);
        foreach my $column (@columns) {
            # All our SERIAL pks have "id" in their name at the end.
            next unless $column =~ /id$/;
            my $sequence = "${table}_${column}_seq";
            if ($self->bz_sequence_exists($sequence)) {
                my $is_associated = $self->selectrow_array(
                    'SELECT pg_get_serial_sequence(?,?)',
                    undef, $table, $column);
                next if $is_associated;
                print "Fixing $sequence to be associated"
                      . " with $table.$column...\n";
                $self->do("ALTER SEQUENCE $sequence OWNED BY $table.$column");
                # In order to produce an exactly identical schema to what
                # a brand-new checksetup.pl run would produce, we also need
                # to re-set the default on this column.
                $self->do("ALTER TABLE $table
                          ALTER COLUMN $column
                           SET DEFAULT nextval('$sequence')");
            }
        }
    }
}

sub _fix_bad_sequence {
    my ($self, $table, $column, $old_seq, $new_seq) = @_;
    if ($self->bz_column_info($table, $column)
        && $self->bz_sequence_exists($old_seq))
    {
        print "Fixing $old_seq sequence...\n";
        $self->do("ALTER SEQUENCE $old_seq RENAME TO $new_seq");
        $self->do("ALTER TABLE $table ALTER COLUMN $column
                    SET DEFAULT NEXTVAL('$new_seq')");
        return 1;
    }
    return 0;
}

# Renames things that differ only in case.
sub _fix_case_differences {
    my ($table, $field) = @_;
    my $dbh = Bugzilla->dbh;

    my $duplicates = $dbh->selectcol_arrayref(
          "SELECT DISTINCT LOWER($field) FROM $table 
        GROUP BY LOWER($field) HAVING COUNT(LOWER($field)) > 1");

    foreach my $name (@$duplicates) {
        my $dups = $dbh->selectcol_arrayref(
            "SELECT $field FROM $table WHERE LOWER($field) = ?",
            undef, $name);
        my $primary = shift @$dups;
        foreach my $dup (@$dups) {
            my $new_name = "${dup}_";
            # Make sure the new name isn't *also* a duplicate.
            while (1) {
                last if (!$dbh->selectrow_array(
                             "SELECT 1 FROM $table WHERE LOWER($field) = ?",
                              undef, lc($new_name)));
                $new_name .= "_";
            }
            print "$table '$primary' and '$dup' have names that differ",
                  " only in case.\nRenaming '$dup' to '$new_name'...\n";
            $dbh->do("UPDATE $table SET $field = ? WHERE $field = ?",
                     undef, $new_name, $dup);
        }
    }
}

#####################################################################
# Custom Schema Information Functions
#####################################################################

# Pg includes the PostgreSQL system tables in table_list_real, so 
# we need to remove those.
sub bz_table_list_real {
    my $self = shift;

    my @full_table_list = $self->SUPER::bz_table_list_real(@_);
    # All PostgreSQL system tables start with "pg_" or "sql_"
    my @table_list = grep(!/(^pg_)|(^sql_)/, @full_table_list);
    return @table_list;
}

1;
