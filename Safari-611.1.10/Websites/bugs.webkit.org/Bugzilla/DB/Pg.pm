# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

=head1 NAME

Bugzilla::DB::Pg - Bugzilla database compatibility layer for PostgreSQL

=head1 DESCRIPTION

This module overrides methods of the Bugzilla::DB module with PostgreSQL
specific implementation. It is instantiated by the Bugzilla::DB module
and should never be used directly.

For interface details see L<Bugzilla::DB> and L<DBI>.

=cut

package Bugzilla::DB::Pg;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Error;
use Bugzilla::Version;
use DBD::Pg;

# This module extends the DB interface via inheritance
use parent qw(Bugzilla::DB);

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
    my ($self, $text, $separator, $sort, $order_by) = @_;
    $sort = 1 if !defined $sort;
    $separator = $self->quote(', ') if !defined $separator;

    # PostgreSQL 8.x doesn't support STRING_AGG
    if (vers_cmp($self->bz_server_version, 9) < 0) {
        my $sql = "ARRAY_ACCUM($text)";
        if ($sort) {
            $sql = "ARRAY_SORT($sql)";
        }
        return "ARRAY_TO_STRING($sql, $separator)";
    }

    if ($order_by && $text =~ /^DISTINCT\s*(.+)$/i) {
        # Since Postgres (quite rightly) doesn't support "SELECT DISTINCT x
        # ORDER BY y", we need to sort the list, and then get the unique
        # values
        return "ARRAY_TO_STRING(ANYARRAY_UNIQ(ARRAY_AGG($1 ORDER BY $order_by)), $separator)";
    }

    # Determine the ORDER BY clause (if any)
    if ($order_by) {
        $order_by = " ORDER BY $order_by";
    }
    elsif ($sort) {
        # We don't include the DISTINCT keyword in an order by
        $text =~ /^(?:DISTINCT\s*)?(.+)$/i;
        $order_by = " ORDER BY $1";
    }

    return "STRING_AGG(${text}::text, $separator${order_by}::text)"
}

sub sql_istring {
    my ($self, $string) = @_;

    return "LOWER(${string}::text)";
}

sub sql_position {
    my ($self, $fragment, $text) = @_;

    return "POSITION(${fragment}::text IN ${text}::text)";
}

sub sql_like {
    my ($self, $fragment, $column, $not) = @_;
    $not //= '';

    return "${column}::text $not LIKE " . $self->sql_like_escape($fragment) . " ESCAPE '|'";
}

sub sql_ilike {
    my ($self, $fragment, $column, $not) = @_;
    $not //= '';

    return "${column}::text $not ILIKE " . $self->sql_like_escape($fragment) . " ESCAPE '|'";
}

sub sql_not_ilike {
    return shift->sql_ilike(@_, 'NOT');
}

# Escapes any % or _ characters which are special in a LIKE match.
# Also performs a $dbh->quote to escape any quote characters.
sub sql_like_escape {
    my ($self, $fragment) = @_;

    $fragment =~ s/\|/\|\|/g;  # escape the escape character if it appears
    $fragment =~ s/%/\|%/g;    # percent and underscore are the special match
    $fragment =~ s/_/\|_/g;    # characters in SQL.

    return $self->quote("%$fragment%");
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
    my ($major_version, $minor_version) = $server_version =~ /^0*(\d+)\.0*(\d+)/;
    # Pg 9.0 requires DBD::Pg 2.17.2 in order to properly read bytea values.
    # Pg 9.2 requires DBD::Pg 2.19.3 as spclocation no longer exists.
    if ($major_version >= 9) {
        local $db->{dbd}->{version} = ($minor_version >= 2) ? '2.19.3' : '2.17.2';
        local $db->{name} = $db->{name} . " ${major_version}.$minor_version";
        Bugzilla::DB::_bz_check_dbd(@_);
    }
}

sub bz_setup_database {
    my $self = shift;
    $self->SUPER::bz_setup_database(@_);

    my ($has_plpgsql) = $self->selectrow_array("SELECT COUNT(*) FROM pg_language WHERE lanname = 'plpgsql'");
    $self->do('CREATE LANGUAGE plpgsql') unless $has_plpgsql;

    if (vers_cmp($self->bz_server_version, 9) < 0) {
        # Custom Functions for Postgres 8
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
    }
    else {
        # Custom functions for Postgres 9.0+

        # -Copyright © 2013 Joshua D. Burns (JDBurnZ) and Message In Action LLC
        # JDBurnZ: https://github.com/JDBurnZ
        # Message In Action: https://www.messageinaction.com
        #
        #Permission is hereby granted, free of charge, to any person obtaining a copy of
        #this software and associated documentation files (the "Software"), to deal in
        #the Software without restriction, including without limitation the rights to
        #use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
        #the Software, and to permit persons to whom the Software is furnished to do so,
        #subject to the following conditions:
        #
        #The above copyright notice and this permission notice shall be included in all
        #copies or substantial portions of the Software.
        #
        #THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
        #IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
        #FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
        #COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
        #IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
        #CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
        $self->do(q|
            DROP FUNCTION IF EXISTS anyarray_uniq(anyarray);
            CREATE OR REPLACE FUNCTION anyarray_uniq(with_array anyarray)
            RETURNS anyarray AS $BODY$
            DECLARE
                -- The variable used to track iteration over "with_array".
                loop_offset integer;

                -- The array to be returned by this function.
                return_array with_array%TYPE := '{}';
            BEGIN
                IF with_array IS NULL THEN
                    return NULL;
                END IF;

                -- Iterate over each element in "concat_array".
                FOR loop_offset IN ARRAY_LOWER(with_array, 1)..ARRAY_UPPER(with_array, 1) LOOP
                    IF NOT with_array[loop_offset] = ANY(return_array) THEN
                        return_array = ARRAY_APPEND(return_array, with_array[loop_offset]);
                    END IF;
                END LOOP;

                RETURN return_array;
            END;
            $BODY$ LANGUAGE plpgsql;
        |);
    }

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

=head2 Functions

=over

=item C<sql_like_escape>

=over

=item B<Description>

The postgres versions of the sql_like methods use the ANSI SQL LIKE
statements to perform substring searching.  To prevent issues with
users attempting to search for strings containing special characters
associated with LIKE, we escape them out so they don't affect the search
terms.

=item B<Params>

=over

=item C<$fragment> - The string fragment in need of escaping and quoting

=back

=item B<Returns>

The fragment with any pre existing %,_,| characters escaped out, wrapped in
percent characters and quoted.

=back

=back

=head1 B<Methods in need of POD>

=over

=item sql_date_format

=item bz_explain

=item bz_sequence_exists

=item bz_last_key

=item sql_position

=item sql_like

=item sql_ilike

=item sql_not_ilike

=item sql_limit

=item sql_not_regexp

=item sql_string_concat

=item sql_date_math

=item sql_to_days

=item bz_check_server_version

=item sql_from_days

=item bz_table_list_real

=item sql_regexp

=item sql_istring

=item sql_group_concat

=item bz_setup_database

=back
