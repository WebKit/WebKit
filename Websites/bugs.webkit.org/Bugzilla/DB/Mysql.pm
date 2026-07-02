# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

=head1 NAME

Bugzilla::DB::Mysql - Bugzilla database compatibility layer for MySQL

=head1 DESCRIPTION

This module overrides methods of the Bugzilla::DB module with MySQL specific
implementation. It is instantiated by the Bugzilla::DB module and should never
be used directly.

For interface details see L<Bugzilla::DB> and L<DBI>.

=cut

package Bugzilla::DB::Mysql;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::DB);

use Bugzilla::Constants;
use Bugzilla::Install::Util qw(install_string);
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::DB::Schema::Mysql;

use List::Util qw(max);
use Text::ParseWords;

# This is how many comments of MAX_COMMENT_LENGTH we expect on a single bug.
# In reality, you could have a LOT more comments than this, because 
# MAX_COMMENT_LENGTH is big.
use constant MAX_COMMENTS => 50;

use constant FULLTEXT_OR => '|';

sub new {
    my ($class, $params) = @_;
    my ($user, $pass, $host, $dbname, $port, $sock) =
        @$params{qw(db_user db_pass db_host db_name db_port db_sock)};

    # construct the DSN from the parameters we got
    my $dsn = "dbi:mysql:host=$host;database=$dbname";
    $dsn .= ";port=$port" if $port;
    $dsn .= ";mysql_socket=$sock" if $sock;

    my %attrs = (
        mysql_enable_utf8 => Bugzilla->params->{'utf8'},
        # Needs to be explicitly specified for command-line processes.
        mysql_auto_reconnect => 1,
    );
    
    # MySQL SSL options
    my ($ssl_ca_file, $ssl_ca_path, $ssl_cert, $ssl_key) =
        @$params{qw(db_mysql_ssl_ca_file db_mysql_ssl_ca_path
                    db_mysql_ssl_client_cert db_mysql_ssl_client_key)};
    if ($ssl_ca_file || $ssl_ca_path || $ssl_cert || $ssl_key) {
        $attrs{'mysql_ssl'}             = 1;
        $attrs{'mysql_ssl_ca_file'}     = $ssl_ca_file if $ssl_ca_file;
        $attrs{'mysql_ssl_ca_path'}     = $ssl_ca_path if $ssl_ca_path;
        $attrs{'mysql_ssl_client_cert'} = $ssl_cert    if $ssl_cert;
        $attrs{'mysql_ssl_client_key'}  = $ssl_key     if $ssl_key;
    }

    my $self = $class->db_new({ dsn => $dsn, user => $user, 
                                pass => $pass, attrs => \%attrs });

    # This makes sure that if the tables are encoded as UTF-8, we
    # return their data correctly.
    $self->do("SET NAMES utf8") if Bugzilla->params->{'utf8'};

    # all class local variables stored in DBI derived class needs to have
    # a prefix 'private_'. See DBI documentation.
    $self->{private_bz_tables_locked} = "";

    # Needed by TheSchwartz
    $self->{private_bz_dsn} = $dsn;

    bless ($self, $class);

    # Check for MySQL modes.
    my ($var, $sql_mode) = $self->selectrow_array(
        "SHOW VARIABLES LIKE 'sql\\_mode'");

    # Disable ANSI and strict modes, else Bugzilla will crash.
    if ($sql_mode) {
        # STRICT_TRANS_TABLE or STRICT_ALL_TABLES enable MySQL strict mode,
        # causing bug 321645. TRADITIONAL sets these modes (among others) as
        # well, so it has to be stipped as well
        my $new_sql_mode =
            join(",", grep {$_ !~ /^(?:ANSI|STRICT_(?:TRANS|ALL)_TABLES|TRADITIONAL)$/}
                            split(/,/, $sql_mode));

        if ($sql_mode ne $new_sql_mode) {
            $self->do("SET SESSION sql_mode = ?", undef, $new_sql_mode);
        }
    }

    # Allow large GROUP_CONCATs (largely for inserting comments 
    # into bugs_fulltext).
    $self->do('SET SESSION group_concat_max_len = 128000000');

    # MySQL 5.5.2 and older have this variable set to true, which causes
    # trouble, see bug 870369.
    $self->do('SET SESSION sql_auto_is_null = 0');

    return $self;
}

# when last_insert_id() is supported on MySQL by lowest DBI/DBD version
# required by Bugzilla, this implementation can be removed.
sub bz_last_key {
    my ($self) = @_;

    my ($last_insert_id) = $self->selectrow_array('SELECT LAST_INSERT_ID()');

    return $last_insert_id;
}

sub sql_group_concat {
    my ($self, $column, $separator, $sort, $order_by) = @_;
    $separator = $self->quote(', ') if !defined $separator;
    $sort = 1 if !defined $sort;
    if ($order_by) {
        $column .= " ORDER BY $order_by";
    }
    elsif ($sort) {
        my $sort_order = $column;
        $sort_order =~ s/^DISTINCT\s+//i;
        $column = "$column ORDER BY $sort_order";
    }
    return "GROUP_CONCAT($column SEPARATOR $separator)";
}

sub sql_regexp {
    my ($self, $expr, $pattern, $nocheck, $real_pattern) = @_;
    $real_pattern ||= $pattern;

    $self->bz_check_regexp($real_pattern) if !$nocheck;

    return "$expr REGEXP $pattern";
}

sub sql_not_regexp {
    my ($self, $expr, $pattern, $nocheck, $real_pattern) = @_;
    $real_pattern ||= $pattern;

    $self->bz_check_regexp($real_pattern) if !$nocheck;

    return "$expr NOT REGEXP $pattern";
}

sub sql_limit {
    my ($self, $limit, $offset) = @_;

    if (defined($offset)) {
        return "LIMIT $offset, $limit";
    } else {
        return "LIMIT $limit";
    }
}

sub sql_string_concat {
    my ($self, @params) = @_;
    
    return 'CONCAT(' . join(', ', @params) . ')';
}

sub sql_fulltext_search {
    my ($self, $column, $text) = @_;

    # Add the boolean mode modifier if the search string contains
    # boolean operators at the start or end of a word.
    my $mode = '';
    if ($text =~ /(?:^|\W)[+\-<>~"()]/ || $text =~ /[()"*](?:$|\W)/) {
        $mode = 'IN BOOLEAN MODE';

        my @terms = split(quotemeta(FULLTEXT_OR), $text);
        foreach my $term (@terms) {
            # quote un-quoted compound words
            my @words = quotewords('[\s()]+', 'delimiters', $term);
            foreach my $word (@words) {
                # match words that have non-word chars in the middle of them
                if ($word =~ /\w\W+\w/ && $word !~ m/"/) {
                    $word = '"' . $word . '"';
                }
            }
            $term = join('', @words);
        }
        $text = join(FULLTEXT_OR, @terms);
    }

    # quote the text for use in the MATCH AGAINST expression
    $text = $self->quote($text);

    # untaint the text, since it's safe to use now that we've quoted it
    trick_taint($text);

    return "MATCH($column) AGAINST($text $mode)";
}

sub sql_istring {
    my ($self, $string) = @_;
    
    return $string;
}

sub sql_from_days {
    my ($self, $days) = @_;

    return "FROM_DAYS($days)";
}

sub sql_to_days {
    my ($self, $date) = @_;

    return "TO_DAYS($date)";
}

sub sql_date_format {
    my ($self, $date, $format) = @_;

    $format = "%Y.%m.%d %H:%i:%s" if !$format;
    
    return "DATE_FORMAT($date, " . $self->quote($format) . ")";
}

sub sql_date_math {
    my ($self, $date, $operator, $interval, $units) = @_;
    
    return "$date $operator INTERVAL $interval $units";
}

sub sql_iposition {
    my ($self, $fragment, $text) = @_;
    return "INSTR($text, $fragment)";
}

sub sql_position {
    my ($self, $fragment, $text) = @_;

    return "INSTR(CAST($text AS BINARY), CAST($fragment AS BINARY))";
}

sub sql_group_by {
    my ($self, $needed_columns, $optional_columns) = @_;

    # MySQL allows you to specify the minimal subset of columns to get
    # a unique result. While it does allow specifying all columns as
    # ANSI SQL requires, according to MySQL documentation, the fewer
    # columns you specify, the faster the query runs.
    return "GROUP BY $needed_columns";
}

sub bz_explain {
    my ($self, $sql) = @_;
    my $sth  = $self->prepare("EXPLAIN $sql");
    $sth->execute();
    my $columns = $sth->{'NAME'};
    my $lengths = $sth->{'mysql_max_length'};
    my $format_string = '|';
    my $i = 0;
    foreach my $column (@$columns) {
        # Sometimes the column name is longer than the contents.
        my $length = max($lengths->[$i], length($column));
        $format_string .= ' %-' . $length . 's |';
        $i++;
    }

    my $first_row = sprintf($format_string, @$columns);
    my @explain_rows = ($first_row, '-' x length($first_row));
    while (my $row = $sth->fetchrow_arrayref) {
        my @fixed = map { defined $_ ? $_ : 'NULL' } @$row;
        push(@explain_rows, sprintf($format_string, @fixed));
    }

    return join("\n", @explain_rows);
}

sub _bz_get_initial_schema {
    my ($self) = @_;
    return $self->_bz_build_schema_from_disk();
}

#####################################################################
# Database Setup
#####################################################################

sub bz_check_server_version {
    my $self = shift;

    my $lc = Bugzilla->localconfig;
    if (lc(Bugzilla->localconfig->{db_name}) eq 'mysql') {
        die "It is not safe to run Bugzilla inside a database named 'mysql'.\n"
            . " Please pick a different value for \$db_name in localconfig.\n";
    }

    $self->SUPER::bz_check_server_version(@_);
}

sub bz_setup_database {
    my ($self) = @_;

    # The "comments" field of the bugs_fulltext table could easily exceed
    # MySQL's default max_allowed_packet. Also, MySQL should never have
    # a max_allowed_packet smaller than our max_attachment_size. So, we
    # warn the user here if max_allowed_packet is too small.
    my $min_max_allowed = MAX_COMMENTS * MAX_COMMENT_LENGTH;
    my (undef, $current_max_allowed) = $self->selectrow_array(
        q{SHOW VARIABLES LIKE 'max\_allowed\_packet'});
    # This parameter is not yet defined when the DB is being built for
    # the very first time. The code below still works properly, however,
    # because the default maxattachmentsize is smaller than $min_max_allowed.
    my $max_attachment = (Bugzilla->params->{'maxattachmentsize'} || 0) * 1024;
    my $needed_max_allowed = max($min_max_allowed, $max_attachment);
    if ($current_max_allowed < $needed_max_allowed) {
        warn install_string('max_allowed_packet',
                            { current => $current_max_allowed,
                              needed  => $needed_max_allowed }) . "\n";
    }

    # Make sure the installation has InnoDB turned on, or we're going to be
    # doing silly things like making foreign keys on MyISAM tables, which is
    # hard to fix later. We do this up here because none of the code below
    # works if InnoDB is off. (Particularly if we've already converted the
    # tables to InnoDB.)
    my %engines = @{$self->selectcol_arrayref('SHOW ENGINES', {Columns => [1,2]})};
    if (!$engines{InnoDB} || $engines{InnoDB} !~ /^(YES|DEFAULT)$/) {
        die install_string('mysql_innodb_disabled');
    }


    my ($sd_index_deleted, $longdescs_index_deleted);
    my @tables = $self->bz_table_list_real();
    # We want to convert tables to InnoDB, but it's possible that they have 
    # fulltext indexes on them, and conversion will fail unless we remove
    # the indexes.
    if (grep($_ eq 'bugs', @tables)
        and !grep($_ eq 'bugs_fulltext', @tables))
    {
        if ($self->bz_index_info_real('bugs', 'short_desc')) {
            $self->bz_drop_index_raw('bugs', 'short_desc');
        }
        if ($self->bz_index_info_real('bugs', 'bugs_short_desc_idx')) {
            $self->bz_drop_index_raw('bugs', 'bugs_short_desc_idx');
            $sd_index_deleted = 1; # Used for later schema cleanup.
        }
    }
    if (grep($_ eq 'longdescs', @tables)
        and !grep($_ eq 'bugs_fulltext', @tables))
    {
        if ($self->bz_index_info_real('longdescs', 'thetext')) {
            $self->bz_drop_index_raw('longdescs', 'thetext');
        }
        if ($self->bz_index_info_real('longdescs', 'longdescs_thetext_idx')) {
            $self->bz_drop_index_raw('longdescs', 'longdescs_thetext_idx');
            $longdescs_index_deleted = 1; # For later schema cleanup.
        }
    }

    # Upgrade tables from MyISAM to InnoDB
    my $db_name = Bugzilla->localconfig->{db_name};
    my $myisam_tables = $self->selectcol_arrayref(
        'SELECT TABLE_NAME FROM information_schema.TABLES 
          WHERE TABLE_SCHEMA = ? AND ENGINE = ?',
        undef, $db_name, 'MyISAM');
    foreach my $should_be_myisam (Bugzilla::DB::Schema::Mysql::MYISAM_TABLES) {
        @$myisam_tables = grep { $_ ne $should_be_myisam } @$myisam_tables;
    }

    if (scalar @$myisam_tables) {
        print "Bugzilla now uses the InnoDB storage engine in MySQL for",
              " most tables.\nConverting tables to InnoDB:\n";
        foreach my $table (@$myisam_tables) {
            print "Converting table $table... ";
            $self->do("ALTER TABLE $table ENGINE = InnoDB");
            print "done.\n";
        }
    }
    
    # Versions of Bugzilla before the existence of Bugzilla::DB::Schema did 
    # not provide explicit names for the table indexes. This means
    # that our upgrades will not be reliable, because we look for the name
    # of the index, not what fields it is on, when doing upgrades.
    # (using the name is much better for cross-database compatibility 
    # and general reliability). It's also very important that our
    # Schema object be consistent with what is on the disk.
    #
    # While we're at it, we also fix some inconsistent index naming
    # from the original checkin of Bugzilla::DB::Schema.

    # We check for the existence of a particular "short name" index that
    # has existed at least since Bugzilla 2.8, and probably earlier.
    # For fixing the inconsistent naming of Schema indexes,
    # we also check for one of those inconsistently-named indexes.
    if (grep($_ eq 'bugs', @tables)
        && ($self->bz_index_info_real('bugs', 'assigned_to')
            || $self->bz_index_info_real('flags', 'flags_bidattid_idx')) )
    {

        # This is a check unrelated to the indexes, to see if people are
        # upgrading from 2.18 or below, but somehow have a bz_schema table
        # already. This only happens if they have done a mysqldump into
        # a database without doing a DROP DATABASE first.
        # We just do the check here since this check is a reliable way
        # of telling that we are upgrading from a version pre-2.20.
        if (grep($_ eq 'bz_schema', $self->bz_table_list_real())) {
            die install_string('bz_schema_exists_before_220');
        }

        my $bug_count = $self->selectrow_array("SELECT COUNT(*) FROM bugs");
        # We estimate one minute for each 3000 bugs, plus 3 minutes just
        # to handle basic MySQL stuff.
        my $rename_time = int($bug_count / 3000) + 3;
        # And 45 minutes for every 15,000 attachments, per some experiments.
        my ($attachment_count) = 
            $self->selectrow_array("SELECT COUNT(*) FROM attachments");
        $rename_time += int(($attachment_count * 45) / 15000);
        # If we're going to take longer than 5 minutes, we let the user know
        # and allow them to abort.
        if ($rename_time > 5) {
            print "\n", install_string('mysql_index_renaming',
                                       { minutes => $rename_time });
            # Wait 45 seconds for them to respond.
            sleep(45) unless Bugzilla->installation_answers->{NO_PAUSE};
        }
        print "Renaming indexes...\n";

        # We can't be interrupted, because of how the "if"
        # works above.
        local $SIG{INT}  = 'IGNORE';
        local $SIG{TERM} = 'IGNORE';
        local $SIG{PIPE} = 'IGNORE';

        # Certain indexes had names in Schema that did not easily conform
        # to a standard. We store those names here, so that they
        # can be properly renamed.
        # Also, sometimes an old mysqldump would incorrectly rename
        # unique indexes to "PRIMARY", so we address that here, also.
        my $bad_names = {
            # 'when' is a possible leftover from Bugzillas before 2.8
            bugs_activity => ['when', 'bugs_activity_bugid_idx',
                'bugs_activity_bugwhen_idx'],
            cc => ['PRIMARY'],
            longdescs => ['longdescs_bugid_idx',
               'longdescs_bugwhen_idx'],
            flags => ['flags_bidattid_idx'],
            flaginclusions => ['flaginclusions_tpcid_idx'],
            flagexclusions => ['flagexclusions_tpc_id_idx'],
            keywords => ['PRIMARY'],
            milestones => ['PRIMARY'],
            profiles_activity => ['profiles_activity_when_idx'],
            group_control_map => ['group_control_map_gid_idx', 'PRIMARY'],
            user_group_map => ['PRIMARY'],
            group_group_map => ['PRIMARY'],
            email_setting => ['PRIMARY'],
            bug_group_map => ['PRIMARY'],
            category_group_map => ['PRIMARY'],
            watch => ['PRIMARY'],
            namedqueries => ['PRIMARY'],
            series_data => ['PRIMARY'],
            # series_categories is dealt with below, not here.
        };

        # The series table is broken and needs to have one index
        # dropped before we begin the renaming, because it had a
        # useless index on it that would cause a naming conflict here.
        if (grep($_ eq 'series', @tables)) {
            my $dropname;
            # This is what the bad index was called before Schema.
            if ($self->bz_index_info_real('series', 'creator_2')) {
                $dropname = 'creator_2';
            }
            # This is what the bad index is called in Schema.
            elsif ($self->bz_index_info_real('series', 'series_creator_idx')) {
                    $dropname = 'series_creator_idx';
            }
            $self->bz_drop_index_raw('series', $dropname) if $dropname;
        }

        # The email_setting table also had the same problem.
        if( grep($_ eq 'email_setting', @tables) 
            && $self->bz_index_info_real('email_setting', 
                                         'email_settings_user_id_idx') ) 
        {
            $self->bz_drop_index_raw('email_setting', 
                                     'email_settings_user_id_idx');
        }

        # Go through all the tables.
        foreach my $table (@tables) {
            # Will contain the names of old indexes as keys, and the 
            # definition of the new indexes as a value. The values
            # include an extra hash key, NAME, with the new name of 
            # the index.
            my %rename_indexes;
            # And go through all the columns on each table.
            my @columns = $self->bz_table_columns_real($table);

            # We also want to fix the silly naming of unique indexes
            # that happened when we first checked-in Bugzilla::DB::Schema.
            if ($table eq 'series_categories') {
                # The series_categories index had a nonstandard name.
                push(@columns, 'series_cats_unique_idx');
            } 
            elsif ($table eq 'email_setting') { 
                # The email_setting table had a similar problem.
                push(@columns, 'email_settings_unique_idx');
            }
            else {
                push(@columns, "${table}_unique_idx");
            }
            # And this is how we fix the other inconsistent Schema naming.
            push(@columns, @{$bad_names->{$table}})
                if (exists $bad_names->{$table});
            foreach my $column (@columns) {
                # If we have an index named after this column, it's an 
                # old-style-name index.
                if (my $index = $self->bz_index_info_real($table, $column)) {
                    # Fix the name to fit in with the new naming scheme.
                    $index->{NAME} = $table . "_" .
                                     $index->{FIELDS}->[0] . "_idx";
                    print "Renaming index $column to " 
                          . $index->{NAME} . "...\n";
                    $rename_indexes{$column} = $index;
                } # if
            } # foreach column

            my @rename_sql = $self->_bz_schema->get_rename_indexes_ddl(
                $table, %rename_indexes);
            $self->do($_) foreach (@rename_sql);

        } # foreach table
    } # if old-name indexes

    # If there are no tables, but the DB isn't utf8 and it should be,
    # then we should alter the database to be utf8. We know it should be
    # if the utf8 parameter is true or there are no params at all.
    # This kind of situation happens when people create the database
    # themselves, and if we don't do this they will get the big
    # scary WARNING statement about conversion to UTF8.
    if ( !$self->bz_db_is_utf8 && !@tables 
         && (Bugzilla->params->{'utf8'} || !scalar keys %{Bugzilla->params}) )
    {
        $self->_alter_db_charset_to_utf8();
    }

    # And now we create the tables and the Schema object.
    $self->SUPER::bz_setup_database();

    if ($sd_index_deleted) {
        $self->_bz_real_schema->delete_index('bugs', 'bugs_short_desc_idx');
        $self->_bz_store_real_schema;
    }
    if ($longdescs_index_deleted) {
        $self->_bz_real_schema->delete_index('longdescs', 
                                             'longdescs_thetext_idx');
        $self->_bz_store_real_schema;
    }

    # The old timestamp fields need to be adjusted here instead of in
    # checksetup. Otherwise the UPDATE statements inside of bz_add_column
    # will cause accidental timestamp updates.
    # The code that does this was moved here from checksetup.

    # 2002-08-14 - bbaetz@student.usyd.edu.au - bug 153578
    # attachments creation time needs to be a datetime, not a timestamp
    my $attach_creation = 
        $self->bz_column_info("attachments", "creation_ts");
    if ($attach_creation && $attach_creation->{TYPE} =~ /^TIMESTAMP/i) {
        print "Fixing creation time on attachments...\n";

        my $sth = $self->prepare("SELECT COUNT(attach_id) FROM attachments");
        $sth->execute();
        my ($attach_count) = $sth->fetchrow_array();

        if ($attach_count > 1000) {
            print "This may take a while...\n";
        }
        my $i = 0;

        # This isn't just as simple as changing the field type, because
        # the creation_ts was previously updated when an attachment was made
        # obsolete from the attachment creation screen. So we have to go
        # and recreate these times from the comments..
        $sth = $self->prepare("SELECT bug_id, attach_id, submitter_id " .
                               "FROM attachments");
        $sth->execute();

        # Restrict this as much as possible in order to avoid false 
        # positives, and keep the db search time down
        my $sth2 = $self->prepare("SELECT bug_when FROM longdescs 
                                    WHERE bug_id=? AND who=? 
                                          AND thetext LIKE ?
                                 ORDER BY bug_when " . $self->sql_limit(1));
        while (my ($bug_id, $attach_id, $submitter_id) 
                  = $sth->fetchrow_array()) 
        {
            $sth2->execute($bug_id, $submitter_id, 
                "Created an attachment (id=$attach_id)%");
            my ($when) = $sth2->fetchrow_array();
            if ($when) {
                $self->do("UPDATE attachments " .
                             "SET creation_ts='$when' " .
                           "WHERE attach_id=$attach_id");
            } else {
                print "Warning - could not determine correct creation"
                      . " time for attachment $attach_id on bug $bug_id\n";
            }
            ++$i;
            print "Converted $i of $attach_count attachments\n" if !($i % 1000);
        }
        print "Done - converted $i attachments\n";

        $self->bz_alter_column("attachments", "creation_ts", 
                               {TYPE => 'DATETIME', NOTNULL => 1});
    }

    # 2004-08-29 - Tomas.Kopal@altap.cz, bug 257303
    # Change logincookies.lastused type from timestamp to datetime
    my $login_lastused = $self->bz_column_info("logincookies", "lastused");
    if ($login_lastused && $login_lastused->{TYPE} =~ /^TIMESTAMP/i) {
        $self->bz_alter_column('logincookies', 'lastused', 
                               { TYPE => 'DATETIME',  NOTNULL => 1});
    }

    # 2005-01-17 - Tomas.Kopal@altap.cz, bug 257315
    # Change bugs.delta_ts type from timestamp to datetime 
    my $bugs_deltats = $self->bz_column_info("bugs", "delta_ts");
    if ($bugs_deltats && $bugs_deltats->{TYPE} =~ /^TIMESTAMP/i) {
        $self->bz_alter_column('bugs', 'delta_ts', 
                               {TYPE => 'DATETIME', NOTNULL => 1});
    }

    # 2005-09-24 - bugreport@peshkin.net, bug 307602
    # Make sure that default 4G table limit is overridden
    my $attach_data_create = $self->selectrow_array(
        'SELECT CREATE_OPTIONS FROM information_schema.TABLES 
          WHERE TABLE_SCHEMA = ? AND TABLE_NAME = ?',
        undef, $db_name, 'attach_data');
    if ($attach_data_create !~ /MAX_ROWS/i) {
        print "Converting attach_data maximum size to 100G...\n";
        $self->do("ALTER TABLE attach_data
                   AVG_ROW_LENGTH=1000000,
                   MAX_ROWS=100000");
    }

    # Convert the database to UTF-8 if the utf8 parameter is on.
    # We check if any table isn't utf8, because lots of crazy
    # partial-conversion situations can happen, and this handles anything
    # that could come up (including having the DB charset be utf8 but not
    # the table charsets.
    #
    # TABLE_COLLATION IS NOT NULL prevents us from trying to convert views.
    my $non_utf8_tables = $self->selectrow_array(
        "SELECT 1 FROM information_schema.TABLES 
          WHERE TABLE_SCHEMA = ? AND TABLE_COLLATION IS NOT NULL 
                AND TABLE_COLLATION NOT LIKE 'utf8%' 
          LIMIT 1", undef, $db_name);
    
    if (Bugzilla->params->{'utf8'} && $non_utf8_tables) {
        print "\n", install_string('mysql_utf8_conversion');

        if (!Bugzilla->installation_answers->{NO_PAUSE}) {
            if (Bugzilla->installation_mode == 
                INSTALLATION_MODE_NON_INTERACTIVE) 
            {
                die install_string('continue_without_answers'), "\n";
            }
            else {
                print "\n         " . install_string('enter_or_ctrl_c');
                getc;
            }
        }

        print "Converting table storage format to UTF-8. This may take a",
              " while.\n";
        foreach my $table ($self->bz_table_list_real) {
            my $info_sth = $self->prepare("SHOW FULL COLUMNS FROM $table");
            $info_sth->execute();
            my (@binary_sql, @utf8_sql);
            while (my $column = $info_sth->fetchrow_hashref) {
                # Our conversion code doesn't work on enum fields, but they
                # all go away later in checksetup anyway.
                next if $column->{Type} =~ /enum/i;

                # If this particular column isn't stored in utf-8
                if ($column->{Collation}
                    && $column->{Collation} ne 'NULL' 
                    && $column->{Collation} !~ /utf8/) 
                {
                    my $name = $column->{Field};

                    print "$table.$name needs to be converted to UTF-8...\n";

                    # These will be automatically re-created at the end
                    # of checksetup.
                    $self->bz_drop_related_fks($table, $name);

                    my $col_info =
                        $self->bz_column_info_real($table, $name);
                    # CHANGE COLUMN doesn't take PRIMARY KEY
                    delete $col_info->{PRIMARYKEY};
                    my $sql_def = $self->_bz_schema->get_type_ddl($col_info);
                    # We don't want MySQL to actually try to *convert*
                    # from our current charset to UTF-8, we just want to
                    # transfer the bytes directly. This is how we do that.

                    # The CHARACTER SET part of the definition has to come
                    # right after the type, which will always come first.
                    my ($binary, $utf8) = ($sql_def, $sql_def);
                    my $type = $self->_bz_schema->convert_type($col_info->{TYPE});
                    $binary =~ s/(\Q$type\E)/$1 CHARACTER SET binary/;
                    $utf8   =~ s/(\Q$type\E)/$1 CHARACTER SET utf8/;
                    push(@binary_sql, "MODIFY COLUMN $name $binary");
                    push(@utf8_sql, "MODIFY COLUMN $name $utf8");
                }
            } # foreach column

            if (@binary_sql) {
                my %indexes = %{ $self->bz_table_indexes($table) };
                foreach my $index_name (keys %indexes) {
                    my $index = $indexes{$index_name};
                    if ($index->{TYPE} and $index->{TYPE} eq 'FULLTEXT') {
                        $self->bz_drop_index($table, $index_name);
                    }
                    else {
                        delete $indexes{$index_name};
                    }
                }

                print "Converting the $table table to UTF-8...\n";
                my $bin = "ALTER TABLE $table " . join(', ', @binary_sql);
                my $utf = "ALTER TABLE $table " . join(', ', @utf8_sql,
                          'DEFAULT CHARACTER SET utf8');
                $self->do($bin);
                $self->do($utf);

                # Re-add any removed FULLTEXT indexes.
                foreach my $index (keys %indexes) {
                    $self->bz_add_index($table, $index, $indexes{$index});
                }
            }
            else {
                $self->do("ALTER TABLE $table DEFAULT CHARACTER SET utf8");
            }

        } # foreach my $table (@tables)
    }

    # Sometimes you can have a situation where all the tables are utf8,
    # but the database isn't. (This tends to happen when you've done
    # a mysqldump.) So we have this change outside of the above block,
    # so that it just happens silently if no actual *table* conversion
    # needs to happen.
    if (Bugzilla->params->{'utf8'} && !$self->bz_db_is_utf8) {
        $self->_alter_db_charset_to_utf8();
    }

     $self->_fix_defaults();

    # Bug 451735 highlighted a bug in bz_drop_index() which didn't
    # check for FKs before trying to delete an index. Consequently,
    # the series_creator_idx index was considered to be deleted
    # despite it was still present in the DB. That's why we have to
    # force the deletion, bypassing the DB schema.
    if (!$self->bz_index_info('series', 'series_category_idx')) {
        if (!$self->bz_index_info('series', 'series_creator_idx')
            && $self->bz_index_info_real('series', 'series_creator_idx'))
        {
            foreach my $column (qw(creator category subcategory name)) {
                $self->bz_drop_related_fks('series', $column);
            }
            $self->bz_drop_index_raw('series', 'series_creator_idx');
        }
    }
}

# When you import a MySQL 3/4 mysqldump into MySQL 5, columns that
# aren't supposed to have defaults will have defaults. This is only
# a minor issue, but it makes our tests fail, and it's good to keep
# the DB actually consistent with what DB::Schema thinks the database
# looks like. So we remove defaults from columns that aren't supposed
# to have them
sub _fix_defaults {
    my $self = shift;
    my $maj_version = substr($self->bz_server_version, 0, 1);
    return if $maj_version < 5;

    # The oldest column that could have this problem is bugs.assigned_to,
    # so if it doesn't have the problem, we just skip doing this entirely.
    my $assi_def = $self->_bz_raw_column_info('bugs', 'assigned_to');
    my $assi_default = $assi_def->{COLUMN_DEF};
    # This "ne ''" thing is necessary because _raw_column_info seems to
    # return COLUMN_DEF as an empty string for columns that don't have
    # a default.
    return unless (defined $assi_default && $assi_default ne '');

    my %fix_columns;
    foreach my $table ($self->_bz_real_schema->get_table_list()) {
        foreach my $column ($self->bz_table_columns($table)) {
            my $abs_def = $self->bz_column_info($table, $column);
            # BLOB/TEXT columns never have defaults
            next if $abs_def->{TYPE} =~ /BLOB|TEXT/i;
            if (!defined $abs_def->{DEFAULT}) {
                # Get the exact default from the database without any
                # "fixing" by bz_column_info_real.
                my $raw_info = $self->_bz_raw_column_info($table, $column);
                my $raw_default = $raw_info->{COLUMN_DEF};
                if (defined $raw_default) {
                    if ($raw_default eq '') {
                        # Only (var)char columns can have empty strings as 
                        # defaults, so if we got an empty string for some
                        # other default type, then it's bogus.
                        next unless $abs_def->{TYPE} =~ /char/i;
                        $raw_default = "''";
                    }
                    $fix_columns{$table} ||= [];
                    push(@{ $fix_columns{$table} }, $column);
                    print "$table.$column has incorrect DB default: $raw_default\n";
                }
            }
        } # foreach $column
    } # foreach $table

    print "Fixing defaults...\n";
    foreach my $table (reverse sort keys %fix_columns) {
        my @alters = map("ALTER COLUMN $_ DROP DEFAULT", 
                         @{ $fix_columns{$table} });
        my $sql = "ALTER TABLE $table " . join(',', @alters);
        $self->do($sql);
    }
}

sub _alter_db_charset_to_utf8 {
    my $self = shift;
    my $db_name = Bugzilla->localconfig->{db_name};
    $self->do("ALTER DATABASE $db_name CHARACTER SET utf8"); 
}

sub bz_db_is_utf8 {
    my $self = shift;
    my $db_collation = $self->selectrow_arrayref(
        "SHOW VARIABLES LIKE 'character_set_database'");
    # First column holds the variable name, second column holds the value.
    return $db_collation->[1] =~ /utf8/ ? 1 : 0;
}


sub bz_enum_initial_values {
    my ($self) = @_;
    my %enum_values = %{$self->ENUM_DEFAULTS};
    # Get a complete description of the 'bugs' table; with DBD::MySQL
    # there isn't a column-by-column way of doing this.  Could use
    # $dbh->column_info, but it would go slower and we would have to
    # use the undocumented mysql_type_name accessor to get the type
    # of each row.
    my $sth = $self->prepare("DESCRIBE bugs");
    $sth->execute();
    # Look for the particular columns we are interested in.
    while (my ($thiscol, $thistype) = $sth->fetchrow_array()) {
        if (defined $enum_values{$thiscol}) {
            # this is a column of interest.
            my @value_list;
            if ($thistype and ($thistype =~ /^enum\(/)) {
                # it has an enum type; get the set of values.
                while ($thistype =~ /'([^']*)'(.*)/) {
                    push(@value_list, $1);
                    $thistype = $2;
                }
            }
            if (@value_list) {
                # record the enum values found.
                $enum_values{$thiscol} = \@value_list;
            }
        }
    }

    return \%enum_values;
}

#####################################################################
# MySQL-specific Database-Reading Methods
#####################################################################

=begin private

=head1 MYSQL-SPECIFIC DATABASE-READING METHODS

These methods read information about the database from the disk,
instead of from a Schema object. They are only reliable for MySQL 
(see bug 285111 for the reasons why not all DBs use/have functions
like this), but that's OK because we only need them for 
backwards-compatibility anyway, for versions of Bugzilla before 2.20.

=over 4

=item C<bz_column_info_real($table, $column)>

 Description: Returns an abstract column definition for a column
              as it actually exists on disk in the database.
 Params:      $table - The name of the table the column is on.
              $column - The name of the column you want info about.
 Returns:     An abstract column definition.
              If the column does not exist, returns undef.

=cut

sub bz_column_info_real {
    my ($self, $table, $column) = @_;
    my $col_data = $self->_bz_raw_column_info($table, $column);
    return $self->_bz_schema->column_info_to_column($col_data);
}

sub _bz_raw_column_info {
    my ($self, $table, $column) = @_;

    # DBD::mysql does not support selecting a specific column,
    # so we have to get all the columns on the table and find 
    # the one we want.
    my $info_sth = $self->column_info(undef, undef, $table, '%');

    # Don't use fetchall_hashref as there's a Win32 DBI bug (292821)
    my $col_data;
    while ($col_data = $info_sth->fetchrow_hashref) {
        last if $col_data->{'COLUMN_NAME'} eq $column;
    }

    if (!defined $col_data) {
        return undef;
    }
    return $col_data;
}

=item C<bz_index_info_real($table, $index)>

 Description: Returns information about an index on a table in the database.
 Params:      $table = name of table containing the index
              $index = name of an index
 Returns:     An abstract index definition, always in hashref format.
              If the index does not exist, the function returns undef.

=cut

sub bz_index_info_real {
    my ($self, $table, $index) = @_;

    my $sth = $self->prepare("SHOW INDEX FROM $table");
    $sth->execute;

    my @fields;
    my $index_type;
    # $raw_def will be an arrayref containing the following information:
    # 0 = name of the table that the index is on
    # 1 = 0 if unique, 1 if not unique
    # 2 = name of the index
    # 3 = seq_in_index (The order of the current field in the index).
    # 4 = Name of ONE column that the index is on
    # 5 = 'Collation' of the index. Usually 'A'.
    # 6 = Cardinality. Either a number or undef.
    # 7 = sub_part. Usually undef. Sometimes 1.
    # 8 = "packed". Usually undef.
    # 9 = Null. Sometimes undef, sometimes 'YES'.
    # 10 = Index_type. The type of the index. Usually either 'BTREE' or 'FULLTEXT'
    # 11 = 'Comment.' Usually undef.
    while (my $raw_def = $sth->fetchrow_arrayref) {
        if ($raw_def->[2] eq $index) {
            push(@fields, $raw_def->[4]);
            # No index can be both UNIQUE and FULLTEXT, that's why
            # this is written this way.
            $index_type = $raw_def->[1] ? '' : 'UNIQUE';
            $index_type = $raw_def->[10] eq 'FULLTEXT'
                ? 'FULLTEXT' : $index_type;
        }
    }

    my $retval;
    if (scalar(@fields)) {
        $retval = {FIELDS => \@fields, TYPE => $index_type};
    }
    return $retval;
}

=item C<bz_index_list_real($table)>

 Description: Returns a list of index names on a table in 
              the database, as it actually exists on disk.
 Params:      $table - The name of the table you want info about.
 Returns:     An array of index names.

=cut

sub bz_index_list_real {
    my ($self, $table) = @_;
    my $sth = $self->prepare("SHOW INDEX FROM $table");
    # Column 3 of a SHOW INDEX statement contains the name of the index.
    return @{ $self->selectcol_arrayref($sth, {Columns => [3]}) };
}

#####################################################################
# MySQL-Specific "Schema Builder"
#####################################################################

=back

=head1 MYSQL-SPECIFIC "SCHEMA BUILDER"

MySQL needs to be able to read in a legacy database (from before 
Schema existed) and create a Schema object out of it. That's what
this code does.

=end private

=cut

# This sub itself is actually written generically, but the subroutines
# that it depends on are database-specific. In particular, the
# bz_column_info_real function would be very difficult to create
# properly for any other DB besides MySQL.
sub _bz_build_schema_from_disk {
    my ($self) = @_;

    my $schema = $self->_bz_schema->get_empty_schema();

    my @tables = $self->bz_table_list_real();
    if (@tables) {
        print "Building Schema object from database...\n"; 
    }
    foreach my $table (@tables) {
        $schema->add_table($table);
        my @columns = $self->bz_table_columns_real($table);
        foreach my $column (@columns) {
            my $type_info = $self->bz_column_info_real($table, $column);
            $schema->set_column($table, $column, $type_info);
        }

        my @indexes = $self->bz_index_list_real($table);
        foreach my $index (@indexes) {
            unless ($index eq 'PRIMARY') {
                my $index_info = $self->bz_index_info_real($table, $index);
                ($index_info = $index_info->{FIELDS}) 
                    if (!$index_info->{TYPE});
                $schema->set_index($table, $index, $index_info);
            }
        }
    }

    return $schema;
}

1;

=head1 B<Methods in need of POD>

=over

=item sql_date_format

=item bz_explain

=item bz_last_key

=item sql_position

=item sql_fulltext_search

=item sql_iposition

=item bz_enum_initial_values

=item sql_group_by

=item sql_limit

=item sql_not_regexp

=item sql_string_concat

=item sql_date_math

=item sql_to_days

=item bz_check_server_version

=item sql_from_days

=item sql_regexp

=item sql_istring

=item sql_group_concat

=item bz_setup_database

=item bz_db_is_utf8

=back
