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
# The Initial Developer of the Original Code is Oracle Corporation. 
# Portions created by Oracle are Copyright (C) 2007 Oracle Corporation. 
# All Rights Reserved.
#
# Contributor(s): Lance Larsh <lance.larsh@oracle.com>
#                 Xiaoou Wu   <xiaoou.wu@oracle.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

=head1 NAME

Bugzilla::DB::Oracle - Bugzilla database compatibility layer for Oracle

=head1 DESCRIPTION

This module overrides methods of the Bugzilla::DB module with Oracle
specific implementation. It is instantiated by the Bugzilla::DB module
and should never be used directly.

For interface details see L<Bugzilla::DB> and L<DBI>.

=cut

package Bugzilla::DB::Oracle;

use strict;

use DBD::Oracle;
use DBD::Oracle qw(:ora_types);
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;
# This module extends the DB interface via inheritance
use base qw(Bugzilla::DB);

#####################################################################
# Constants
#####################################################################
use constant EMPTY_STRING  => '__BZ_EMPTY_STR__';
use constant ISOLATION_LEVEL => 'READ COMMITTED';
use constant BLOB_TYPE => { ora_type => ORA_BLOB };
use constant GROUPBY_REGEXP => '((CASE\s+WHEN.+END)|(TO_CHAR\(.+\))|(\(SCORE.+\))|(\(MATCH.+\))|(\w+(\.\w+)?))(\s+AS\s+)?(.*)?$';

sub new {
    my ($class, $user, $pass, $host, $dbname, $port) = @_;

    # You can never connect to Oracle without a DB name,
    # and there is no default DB.
    $dbname ||= Bugzilla->localconfig->{db_name};

    # Set the language enviroment
    $ENV{'NLS_LANG'} = '.AL32UTF8' if Bugzilla->params->{'utf8'};

    # construct the DSN from the parameters we got
    my $dsn = "DBI:Oracle:host=$host;sid=$dbname";
    $dsn .= ";port=$port" if $port;
    my $attrs = { FetchHashKeyName => 'NAME_lc',  
                  LongReadLen => ( Bugzilla->params->{'maxattachmentsize'}
                                     || 1000 ) * 1024, 
                };
    my $self = $class->db_new($dsn, $user, $pass, $attrs);

    bless ($self, $class);

    # Set the session's default date format to match MySQL
    $self->do("ALTER SESSION SET NLS_DATE_FORMAT='YYYY-MM-DD HH24:MI:SS'");
    $self->do("ALTER SESSION SET NLS_TIMESTAMP_FORMAT='YYYY-MM-DD HH24:MI:SS'");
    $self->do("ALTER SESSION SET NLS_LENGTH_SEMANTICS='CHAR'") 
        if Bugzilla->params->{'utf8'};
    # To allow case insensitive query.
    $self->do("ALTER SESSION SET NLS_COMP='ANSI'");
    $self->do("ALTER SESSION SET NLS_SORT='BINARY_AI'");
    return $self;
}

sub bz_last_key {
    my ($self, $table, $column) = @_;

    my $seq = $table . "_" . $column . "_SEQ";
    my ($last_insert_id) = $self->selectrow_array("SELECT $seq.CURRVAL "
                                                  . " FROM DUAL");
    return $last_insert_id;
}

sub sql_regexp {
    my ($self, $expr, $pattern) = @_;

    return "REGEXP_LIKE($expr, $pattern)";
}

sub sql_not_regexp {
    my ($self, $expr, $pattern) = @_;

    return "NOT REGEXP_LIKE($expr, $pattern)" 
}

sub sql_limit {
    my ($self, $limit, $offset) = @_;

    if(defined $offset) {
        return  "/* LIMIT $limit $offset */";
    }
    return "/* LIMIT $limit */";
}

sub sql_string_concat {
    my ($self, @params) = @_;

    return 'CONCAT(' . join(', ', @params) . ')';
}

sub sql_to_days {
    my ($self, $date) = @_;

    return " TO_CHAR(TO_DATE($date),'J') ";
}
sub sql_from_days{
    my ($self, $date) = @_;

    return " TO_DATE($date,'J') ";
}
sub sql_fulltext_search {
    my ($self, $column, $text, $label) = @_;
    $text = $self->quote($text);
    trick_taint($text);
    return "CONTAINS($column,$text,$label)", "SCORE($label)";
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

sub sql_interval {
    my ($self, $interval, $units) = @_;
    if ($units =~ /YEAR|MONTH/i) {
        return "NUMTOYMINTERVAL($interval,'$units')";
    } else{
        return "NUMTODSINTERVAL($interval,'$units')";
    }
}

sub sql_position {
    my ($self, $fragment, $text) = @_;
    return "INSTR($text, $fragment)";
}

sub sql_in {
    my ($self, $column_name, $in_list_ref) = @_;
    my @in_list = @$in_list_ref;
    return $self->SUPER::sql_in($column_name, $in_list_ref) if $#in_list < 1000;
    my @in_str;
    while (@in_list) {
        my $length = $#in_list + 1;
        my $splice = $length > 1000 ? 1000 : $length;
        my @sub_in_list = splice(@in_list, 0, $splice);
        push(@in_str, 
             $self->SUPER::sql_in($column_name, \@sub_in_list)); 
    }
    return "( " . join(" OR ", @in_str) . " )";
}

sub bz_drop_table {
     my ($self, $name) = @_;
     my $table_exists = $self->bz_table_info($name);
     if ($table_exists) {
         $self->_bz_drop_fks($name);
         $self->SUPER::bz_drop_table($name);
     }
}

# Dropping all FKs for a specified table. 
sub _bz_drop_fks {
    my ($self, $table) = @_;
    my @columns = $self->_bz_real_schema->get_table_columns($table);
    foreach my $column (@columns) {
        $self->bz_drop_fk($table, $column);
    }
}

sub _fix_empty {
    my ($string) = @_;
    $string = '' if $string eq EMPTY_STRING;
    return $string;
}

sub _fix_arrayref {
    my ($row) = @_;
    return undef if !defined $row;
    foreach my $field (@$row) {
        $field = _fix_empty($field) if defined $field;
    }
    return $row;
}

sub _fix_hashref {
     my ($row) = @_;
     return undef if !defined $row;
     foreach my $value (values %$row) {
       $value = _fix_empty($value) if defined $value;
     }
     return $row;
}

sub adjust_statement {
    my ($sql) = @_;

    # We can't just assume any occurrence of "''" in $sql is an empty
    # string, since "''" can occur inside a string literal as a way of
    # escaping a single "'" in the literal.  Therefore we must be trickier...

    # split the statement into parts by single-quotes.  The negative value
    # at the end to the split operator from dropping trailing empty strings
    # (e.g., when $sql ends in "''")
    my @parts = split /'/, $sql, -1;

    if( !(@parts % 2) ) {
        # Either the string is empty or the quotes are mismatched
        # Returning input unmodified.
        return $sql;
    }

    # We already verified that we have an odd number of parts.  If we take
    # the first part off now, we know we're entering the loop with an even
    # number of parts
    my @result;
    my $part = shift @parts;
    
    # Oracle requires a FROM clause in all SELECT statements, so append
    # "FROM dual" to queries without one (e.g., "SELECT NOW()")
    my $is_select = ($part =~ m/^\s*SELECT\b/io);
    my $has_from =  ($part =~ m/\bFROM\b/io) if $is_select;

    # Oracle recognizes CURRENT_DATE, but not CURRENT_DATE()
    $part =~ s/\bCURRENT_DATE\b\(\)/CURRENT_DATE/io;
    
    # Oracle use SUBSTR instead of SUBSTRING
    $part =~ s/\bSUBSTRING\b/SUBSTR/io;
   
    # Oracle need no 'AS'
    $part =~ s/\bAS\b//ig;
    
    # Oracle doesn't have LIMIT, so if we find the LIMIT comment, wrap the
    # query with "SELECT * FROM (...) WHERE rownum < $limit"
    my ($limit,$offset) = ($part =~ m{/\* LIMIT (\d*) (\d*) \*/}o);
    
    push @result, $part;
    while( @parts ) {
        my $string = shift @parts;
        my $nonstring = shift @parts;
    
        # if the non-string part is zero-length and there are more parts left,
        # then this is an escaped quote inside a string literal   
        while( !(length $nonstring) && @parts  ) {
            # we know it's safe to remove two parts at a time, since we
            # entered the loop with an even number of parts
            $string .= "''" . shift @parts;
            $nonstring = shift @parts;
        }

        # Look for a FROM if this is a SELECT and we haven't found one yet
        $has_from = ($nonstring =~ m/\bFROM\b/io) 
                    if ($is_select and !$has_from);

        # Oracle recognizes CURRENT_DATE, but not CURRENT_DATE()
        $nonstring =~ s/\bCURRENT_DATE\b\(\)/CURRENT_DATE/io;

        # Oracle use SUBSTR instead of SUBSTRING
        $nonstring =~ s/\bSUBSTRING\b/SUBSTR/io;
        
        # Oracle need no 'AS'
        $nonstring =~ s/\bAS\b//ig;

        # Look for a LIMIT clause
        ($limit) = ($nonstring =~ m(/\* LIMIT (\d*) \*/)o);

        if(!length($string)){
           push @result, EMPTY_STRING;
           push @result, $nonstring;
        } else {
           push @result, $string;
           push @result, $nonstring;
        }
    }

    my $new_sql = join "'", @result;

    # Append "FROM dual" if this is a SELECT without a FROM clause
    $new_sql .= " FROM DUAL" if ($is_select and !$has_from);

    # Wrap the query with a "WHERE rownum <= ..." if we found LIMIT
    
    if (defined($limit)) {
        if ($new_sql !~ /\bWHERE\b/) {
            $new_sql = $new_sql." WHERE 1=1";
        }
         my ($before_where, $after_where) = split /\bWHERE\b/i,$new_sql;
         if (defined($offset)) {
             if ($new_sql =~ /(.*\s+)FROM(\s+.*)/i) { 
                 my ($before_from,$after_from) = ($1,$2);
                 $before_where = "$before_from FROM ($before_from,"
                             . " ROW_NUMBER() OVER (ORDER BY 1) R "
                             . " FROM $after_from ) "; 
                 $after_where = " R BETWEEN $offset+1 AND $limit+$offset";
             }
         } else {
                 $after_where = " rownum <=$limit AND ".$after_where;
         }

         $new_sql = $before_where." WHERE ".$after_where;
    }
    return $new_sql;
}

sub do {
    my $self = shift;
    my $sql  = shift;
    $sql = adjust_statement($sql);
    unshift @_, $sql;
    return $self->SUPER::do(@_);
}

sub selectrow_array {
    my $self = shift;
    my $stmt = shift;
    my $new_stmt = (ref $stmt) ? $stmt : adjust_statement($stmt);
    unshift @_, $new_stmt;
    if ( wantarray ) {
        my @row = $self->SUPER::selectrow_array(@_);
        _fix_arrayref(\@row);
        return @row;
    } else {
        my $row = $self->SUPER::selectrow_array(@_);
        $row = _fix_empty($row) if defined $row;
        return $row;
    }
}

sub selectrow_arrayref {
    my $self = shift;
    my $stmt = shift;
    my $new_stmt = (ref $stmt) ? $stmt : adjust_statement($stmt);
    unshift @_, $new_stmt;
    my $ref = $self->SUPER::selectrow_arrayref(@_);
    return undef if !defined $ref;

    _fix_arrayref($ref);
    return $ref;
}

sub selectrow_hashref {
    my $self = shift;
    my $stmt = shift;
    my $new_stmt = (ref $stmt) ? $stmt : adjust_statement($stmt);
    unshift @_, $new_stmt;
    my $ref = $self->SUPER::selectrow_hashref(@_);
    return undef if !defined $ref;

    _fix_hashref($ref);
    return $ref;
}

sub selectall_arrayref {
    my $self = shift;
    my $stmt = shift;
    my $new_stmt = (ref $stmt) ? $stmt : adjust_statement($stmt);
    unshift @_, $new_stmt;
    my $ref = $self->SUPER::selectall_arrayref(@_);
    return undef if !defined $ref;
    
    foreach my $row (@$ref) {
       if (ref($row) eq 'ARRAY') {
            _fix_arrayref($row);
       }
       elsif (ref($row) eq 'HASH') {
            _fix_hashref($row);
       }
    }

    return $ref;
}

sub selectall_hashref {
    my $self = shift;
    my $stmt = shift;
    my $new_stmt = (ref $stmt) ? $stmt : adjust_statement($stmt);
    unshift @_, $new_stmt;
    my $rows = $self->SUPER::selectall_hashref(@_);
    return undef if !defined $rows;
    foreach my $row (values %$rows) { 
          _fix_hashref($row);
    }
    return $rows;
}

sub selectcol_arrayref {
    my $self = shift;
    my $stmt = shift;
    my $new_stmt = (ref $stmt) ? $stmt : adjust_statement($stmt);
    unshift @_, $new_stmt;
    my $ref = $self->SUPER::selectcol_arrayref(@_);
    return undef if !defined $ref;
    _fix_arrayref($ref);
    return $ref;
}

sub prepare {
    my $self = shift;
    my $sql  = shift;
    my $new_sql = adjust_statement($sql);
    unshift @_, $new_sql;
    return bless $self->SUPER::prepare(@_),
                        'Bugzilla::DB::Oracle::st';
}

sub prepare_cached {
    my $self = shift;
    my $sql  = shift;
    my $new_sql = adjust_statement($sql);
    unshift @_, $new_sql;
    return bless $self->SUPER::prepare_cached(@_),
                      'Bugzilla::DB::Oracle::st';
}

sub quote_identifier {
     my ($self,$id) = @_;
     return $id;
}

#####################################################################
# Protected "Real Database" Schema Information Methods
#####################################################################

sub bz_table_columns_real {
    my ($self, $table) = @_;
    $table = uc($table);
    my $cols = $self->selectcol_arrayref(
        "SELECT LOWER(COLUMN_NAME) FROM USER_TAB_COLUMNS WHERE 
         TABLE_NAME = ?  ORDER BY COLUMN_NAME", undef, $table);
    return @$cols;
}

sub bz_table_list_real {
    my ($self) = @_;
    my $tables = $self->selectcol_arrayref(
        "SELECT LOWER(TABLE_NAME) FROM USER_TABLES WHERE 
        TABLE_NAME NOT LIKE ?  ORDER BY TABLE_NAME", undef, 'DR$%');
    return @$tables;
}

#####################################################################
# Custom Database Setup
#####################################################################

sub bz_setup_database {
    my $self = shift;
    
    # Create a function that returns SYSDATE to emulate MySQL's "NOW()".
    # Function NOW() is used widely in Bugzilla SQLs, but Oracle does not 
    # have that function, So we have to create one ourself. 
    $self->do("CREATE OR REPLACE FUNCTION NOW "
              . " RETURN DATE IS BEGIN RETURN SYSDATE; END;");
    $self->do("CREATE OR REPLACE FUNCTION CHAR_LENGTH(COLUMN_NAME VARCHAR2)" 
              . " RETURN NUMBER IS BEGIN RETURN LENGTH(COLUMN_NAME); END;");
    # Create a WORLD_LEXER named BZ_LEX for multilingual fulltext search
    my $lexer = $self->selectcol_arrayref(
       "SELECT pre_name FROM CTXSYS.CTX_PREFERENCES WHERE pre_name = ? AND
          pre_owner = ?",
          undef,'BZ_LEX',uc(Bugzilla->localconfig->{db_user}));
    if(!@$lexer) {
       $self->do("BEGIN CTX_DDL.CREATE_PREFERENCE
                        ('BZ_LEX', 'WORLD_LEXER'); END;");
    }

    $self->SUPER::bz_setup_database(@_);

    my @tables = $self->bz_table_list_real();
    foreach my $table (@tables) {
        my @columns = $self->bz_table_columns_real($table);
        foreach my $column (@columns) {
            my $def = $self->bz_column_info($table, $column);
            if ($def->{REFERENCES}) {
                my $references = $def->{REFERENCES};
                my $update = $references->{UPDATE} || 'CASCADE';
                my $to_table  = $references->{TABLE};
                my $to_column = $references->{COLUMN};
                my $fk_name = $self->_bz_schema->_get_fk_name($table,
                                                              $column,
                                                              $references);
                if ( $update =~ /CASCADE/i ){
                     my $trigger_name = uc($fk_name . "_UC");
                     my $exist_trigger = $self->selectcol_arrayref(
                         "SELECT OBJECT_NAME FROM USER_OBJECTS 
                          WHERE OBJECT_NAME = ?", undef, $trigger_name);
                    if(@$exist_trigger) {
                        $self->do("DROP TRIGGER $trigger_name");
                    }
  
                     my $tr_str = "CREATE OR REPLACE TRIGGER $trigger_name"
                         . " AFTER  UPDATE  ON ". $to_table
                         . " REFERENCING "
                         . " NEW AS NEW "
                         . " OLD AS OLD "
                         . " FOR EACH ROW "
                         . " BEGIN "
                         . "     UPDATE $table"
                         . "        SET $column = :NEW.$to_column"
                         . "      WHERE $column = :OLD.$to_column;"
                         . " END $trigger_name;";
                         $self->do($tr_str);
               }
         }
     }
   }

}

package Bugzilla::DB::Oracle::st;
use base qw(DBI::st);
 
sub fetchrow_arrayref {
    my $self = shift;
    my $ref = $self->SUPER::fetchrow_arrayref(@_);
    return undef if !defined $ref;
    Bugzilla::DB::Oracle::_fix_arrayref($ref);
    return $ref;
}

sub fetchrow_array {
    my $self = shift;
    if ( wantarray ) {
        my @row = $self->SUPER::fetchrow_array(@_);
        Bugzilla::DB::Oracle::_fix_arrayref(\@row);
        return @row;
    } else {
        my $row = $self->SUPER::fetchrow_array(@_);
        $row = Bugzilla::DB::Oracle::_fix_empty($row) if defined $row;
        return $row;
    }
}

sub fetchrow_hashref {
    my $self = shift;
    my $ref = $self->SUPER::fetchrow_hashref(@_);
    return undef if !defined $ref;
    Bugzilla::DB::Oracle::_fix_hashref($ref);
    return $ref;
}

sub fetchall_arrayref {
    my $self = shift;
    my $ref = $self->SUPER::fetchall_arrayref(@_);
    return undef if !defined $ref;
    foreach my $row (@$ref) {
        if (ref($row) eq 'ARRAY') {
             Bugzilla::DB::Oracle::_fix_arrayref($row);
        }
        elsif (ref($row) eq 'HASH') {
            Bugzilla::DB::Oracle::_fix_hashref($row);
        }
    }
    return $ref;
}

sub fetchall_hashref {
    my $self = shift;
    my $ref = $self->SUPER::fetchall_hashref(@_);
    return undef if !defined $ref;
    foreach my $row (values %$ref) {
         Bugzilla::DB::Oracle::_fix_hashref($row);
    }
     return $ref;
}

sub fetch {
    my $self = shift;
    my $row = $self->SUPER::fetch(@_);
    if ($row) {
      Bugzilla::DB::Oracle::_fix_arrayref($row);
    }
   return $row;
}
1;
