# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::DB;

use 5.10.1;
use strict;
use warnings;

use DBI;

# Inherit the DB class from DBI::db.
use parent -norequire, qw(DBI::db);

use Bugzilla::Constants;
use Bugzilla::Mailer;
use Bugzilla::Install::Requirements;
use Bugzilla::Install::Util qw(install_string);
use Bugzilla::Install::Localconfig;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::DB::Schema;
use Bugzilla::Version;

use List::Util qw(max);
use Storable qw(dclone);

#####################################################################
# Constants
#####################################################################

use constant BLOB_TYPE => DBI::SQL_BLOB;
use constant ISOLATION_LEVEL => 'REPEATABLE READ';

# Set default values for what used to be the enum types.  These values
# are no longer stored in localconfig.  If we are upgrading from a
# Bugzilla with enums to a Bugzilla without enums, we use the
# enum values.
#
# The values that you see here are ONLY DEFAULTS. They are only used
# the FIRST time you run checksetup.pl, IF you are NOT upgrading from a
# Bugzilla with enums. After that, they are either controlled through
# the Bugzilla UI or through the DB.
use constant ENUM_DEFAULTS => {
    bug_severity  => ['blocker', 'critical', 'major', 'normal',
                      'minor', 'trivial', 'enhancement'],
    priority     => ["Highest", "High", "Normal", "Low", "Lowest", "---"],
    op_sys       => ["All","Windows","Mac OS","Linux","Other"],
    rep_platform => ["All","PC","Macintosh","Other"],
    bug_status   => ["UNCONFIRMED","CONFIRMED","IN_PROGRESS","RESOLVED",
                     "VERIFIED"],
    resolution   => ["","FIXED","INVALID","WONTFIX", "DUPLICATE","WORKSFORME"],
};

# The character that means "OR" in a boolean fulltext search. If empty,
# the database doesn't support OR searches in fulltext searches.
# Used by Bugzilla::Bug::possible_duplicates.
use constant FULLTEXT_OR => '';

# These are used in regular expressions to mean "the start or end of a word".
#
# We don't use [[:<:]] and [[:>:]], even though they mean
# "start and end of a word" and are supported by both MySQL and PostgreSQL,
# because they don't work if your search starts or ends with a non-alphanumeric
# character, and there's a fair chance somebody will want to use the "word"
# search to search flags for something like "review+".
#
# We do use [:almum:] because it is supported by at least MySQL and
# PostgreSQL, and hopefully will get us as much Unicode support as possible,
# depending on how well the regexp engines of the various databases support
# Unicode.
use constant WORD_START => '(^|[^[:alnum:]])';
use constant WORD_END   => '($|[^[:alnum:]])';

# On most databases, in order to drop an index, you have to first drop
# the foreign keys that use that index. However, on some databases,
# dropping the FK immediately before dropping the index causes problems
# and doesn't need to be done anyway, so those DBs set this to 0.
use constant INDEX_DROPS_REQUIRE_FK_DROPS => 1;

#####################################################################
# Overridden Superclass Methods 
#####################################################################

sub quote {
    my $self = shift;
    my $retval = $self->SUPER::quote(@_);
    trick_taint($retval) if defined $retval;
    return $retval;
}

#####################################################################
# Connection Methods
#####################################################################

sub connect_shadow {
    my $params = Bugzilla->params;
    die "Tried to connect to non-existent shadowdb" 
        unless $params->{'shadowdb'};

    # Instead of just passing in a new hashref, we locally modify the
    # values of "localconfig", because some drivers access it while
    # connecting.
    my %connect_params = %{ Bugzilla->localconfig };
    $connect_params{db_host} = $params->{'shadowdbhost'};
    $connect_params{db_name} = $params->{'shadowdb'};
    $connect_params{db_port} = $params->{'shadowdbport'};
    $connect_params{db_sock} = $params->{'shadowdbsock'};

    return _connect(\%connect_params);
}

sub connect_main {
    my $lc = Bugzilla->localconfig;
    return _connect(Bugzilla->localconfig); 
}

sub _connect {
    my ($params) = @_;

    my $driver = $params->{db_driver};
    my $pkg_module = DB_MODULE->{lc($driver)}->{db};

    # do the actual import
    eval ("require $pkg_module")
        || die ("'$driver' is not a valid choice for \$db_driver in "
                . " localconfig: " . $@);

    # instantiate the correct DB specific module
    my $dbh = $pkg_module->new($params);

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

sub bz_check_requirements {
    my ($output) = @_;

    my $lc = Bugzilla->localconfig;
    my $db = DB_MODULE->{lc($lc->{db_driver})};

    # Only certain values are allowed for $db_driver.
    if (!defined $db) {
        die "$lc->{db_driver} is not a valid choice for \$db_driver in"
            . bz_locations()->{'localconfig'};
    }

    # Check the existence and version of the DBD that we need.
    my $dbd = $db->{dbd};
    _bz_check_dbd($db, $output);

    # We don't try to connect to the actual database if $db_check is
    # disabled.
    unless ($lc->{db_check}) {
        print "\n" if $output;
        return;
    }

    # And now check the version of the database server itself.
    my $dbh = _get_no_db_connection();
    $dbh->bz_check_server_version($db, $output);

    print "\n" if $output;
}

sub _bz_check_dbd {
    my ($db, $output) = @_;

    my $dbd = $db->{dbd};
    unless (have_vers($dbd, $output)) {
        my $sql_server = $db->{name};
        my $command = install_command($dbd);
        my $root    = ROOT_USER;
        my $dbd_mod = $dbd->{module};
        my $dbd_ver = $dbd->{version};
        die <<EOT;

For $sql_server, Bugzilla requires that perl's $dbd_mod $dbd_ver or later be
installed. To install this module, run the following command (as $root):

    $command

EOT
    }
}

sub bz_check_server_version {
    my ($self, $db, $output) = @_;

    my $sql_vers = $self->bz_server_version;
    $self->disconnect;

    my $sql_want = $db->{db_version};
    my $version_ok = vers_cmp($sql_vers, $sql_want) > -1 ? 1 : 0;

    my $sql_server = $db->{name};
    if ($output) {
        Bugzilla::Install::Requirements::_checking_for({
            package => $sql_server, wanted => $sql_want,
            found   => $sql_vers, ok => $version_ok });
    }

    # Check what version of the database server is installed and let
    # the user know if the version is too old to be used with Bugzilla.
    if (!$version_ok) {
        die <<EOT;

Your $sql_server v$sql_vers is too old. Bugzilla requires version
$sql_want or later of $sql_server. Please download and install a
newer version.

EOT
    }

    # This is used by subclasses.
    return $sql_vers;
}

# Note that this function requires that localconfig exist and
# be valid.
sub bz_create_database {
    my $dbh;
    # See if we can connect to the actual Bugzilla database.
    my $conn_success = eval { $dbh = connect_main() };
    my $db_name = Bugzilla->localconfig->{db_name};

    if (!$conn_success) {
        $dbh = _get_no_db_connection();
        say "Creating database $db_name...";

        # Try to create the DB, and if we fail print a friendly error.
        my $success  = eval {
            my @sql = $dbh->_bz_schema->get_create_database_sql($db_name);
            # This ends with 1 because this particular do doesn't always
            # return something.
            $dbh->do($_) foreach @sql; 1;
        };
        if (!$success) {
            my $error = $dbh->errstr || $@;
            chomp($error);
            die "The '$db_name' database could not be created.",
                " The error returned was:\n\n    $error\n\n",
                _bz_connect_error_reasons();
        }
    }

    $dbh->disconnect;
}

# A helper for bz_create_database and bz_check_requirements.
sub _get_no_db_connection {
    my ($sql_server) = @_;
    my $dbh;
    my %connect_params = %{ Bugzilla->localconfig };
    $connect_params{db_name} = '';
    my $conn_success = eval {
        $dbh = _connect(\%connect_params);
    };
    if (!$conn_success) {
        my $driver = $connect_params{db_driver};
        my $sql_server = DB_MODULE->{lc($driver)}->{name};
        # Can't use $dbh->errstr because $dbh is undef.
        my $error = $DBI::errstr || $@;
        chomp($error);
        die "There was an error connecting to $sql_server:\n\n",
            "    $error\n\n", _bz_connect_error_reasons(), "\n";
    }
    return $dbh;    
}

# Just a helper because we have to re-use this text.
# We don't use this in db_new because it gives away the database
# username, and db_new errors can show up on CGIs.
sub _bz_connect_error_reasons {
    my $lc_file = bz_locations()->{'localconfig'};
    my $lc      = Bugzilla->localconfig;
    my $db      = DB_MODULE->{lc($lc->{db_driver})};
    my $server  = $db->{name};

return <<EOT;
This might have several reasons:

* $server is not running.
* $server is running, but there is a problem either in the
  server configuration or the database access rights. Read the Bugzilla
  Guide in the doc directory. The section about database configuration
  should help.
* Your password for the '$lc->{db_user}' user, specified in \$db_pass, is 
  incorrect, in '$lc_file'.
* There is a subtle problem with Perl, DBI, or $server. Make
  sure all settings in '$lc_file' are correct. If all else fails, set
  '\$db_check' to 0.

EOT
}

# List of abstract methods we are checking the derived class implements
our @_abstract_methods = qw(new sql_regexp sql_not_regexp sql_limit sql_to_days
                            sql_date_format sql_date_math bz_explain
                            sql_group_concat);

# This overridden import method will check implementation of inherited classes
# for missing implementation of abstract methods
# See http://perlmonks.thepen.com/44265.html
sub import {
    my $pkg = shift;

    # do not check this module
    if ($pkg ne __PACKAGE__) {
        # make sure all abstract methods are implemented
        foreach my $meth (@_abstract_methods) {
            $pkg->can($meth)
                or die("Class $pkg does not define method $meth");
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

sub sql_iposition {
    my ($self, $fragment, $text) = @_;
    $fragment = $self->sql_istring($fragment);
    $text = $self->sql_istring($text);
    return $self->sql_position($fragment, $text);
}

sub sql_position {
    my ($self, $fragment, $text) = @_;

    return "POSITION($fragment IN $text)";
}

sub sql_like {
    my ($self, $fragment, $column) = @_;

    my $quoted = $self->quote($fragment);

    return $self->sql_position($quoted, $column) . " > 0";
}

sub sql_ilike {
    my ($self, $fragment, $column) = @_;

    my $quoted = $self->quote($fragment);

    return $self->sql_iposition($quoted, $column) . " > 0";
}

sub sql_not_ilike {
    my ($self, $fragment, $column) = @_;

    my $quoted = $self->quote($fragment);

    return $self->sql_iposition($quoted, $column) . " = 0";
}


sub sql_group_by {
    my ($self, $needed_columns, $optional_columns) = @_;

    my $expression = "GROUP BY $needed_columns";
    $expression .= ", " . $optional_columns if $optional_columns;
    
    return $expression;
}

sub sql_string_concat {
    my ($self, @params) = @_;
    
    return '(' . join(' || ', @params) . ')';
}

sub sql_string_until {
    my ($self, $string, $substring) = @_;

    my $position = $self->sql_position($substring, $string);
    return "CASE WHEN $position != 0"
             . " THEN SUBSTR($string, 1, $position - 1)"
             . " ELSE $string END";
}

sub sql_in {
    my ($self, $column_name, $in_list_ref, $negate) = @_;
    return " $column_name "
             . ($negate ? "NOT " : "")
             . "IN (" . join(',', @$in_list_ref) . ") ";
}

sub sql_fulltext_search {
    my ($self, $column, $text) = @_;

    # This is as close as we can get to doing full text search using
    # standard ANSI SQL, without real full text search support. DB specific
    # modules should override this, as this will be always much slower.

    # make the string lowercase to do case insensitive search
    my $lower_text = lc($text);

    # split the text we're searching for into separate words. As a hack
    # to allow quicksearch to work, if the field starts and ends with
    # a double-quote, then we don't split it into words. We can't use
    # Text::ParseWords here because it gets very confused by unbalanced
    # quotes, which breaks searches like "don't try this" (because of the
    # unbalanced single-quote in "don't").
    my @words;
    if ($lower_text =~ /^"/ and $lower_text =~ /"$/) {
        $lower_text =~ s/^"//;
        $lower_text =~ s/"$//;
        @words = ($lower_text);
    }
    else {
        @words = split(/\s+/, $lower_text);
    }

    # surround the words with wildcards and SQL quotes so we can use them
    # in LIKE search clauses
    @words = map($self->quote("\%$_\%"), @words);

    # untaint words, since they are safe to use now that we've quoted them
    trick_taint($_) foreach @words;

    # turn the words into a set of LIKE search clauses
    @words = map("LOWER($column) LIKE $_", @words);

    # search for occurrences of all specified words in the column
    return join (" AND ", @words), "CASE WHEN (" . join(" AND ", @words) . ") THEN 1 ELSE 0 END";
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

    return $self->last_insert_id(Bugzilla->localconfig->{db_name}, undef, 
                                 $table, $column);
}

sub bz_check_regexp {
    my ($self, $pattern) = @_;

    eval { $self->do("SELECT " . $self->sql_regexp($self->quote("a"), $pattern, 1)) };

    $@ && ThrowUserError('illegal_regexp', 
        { value => $pattern, dberror => $self->errstr }); 
}

#####################################################################
# Database Setup
#####################################################################

sub bz_setup_database {
    my ($self) = @_;

    # If we haven't ever stored a serialized schema,
    # set up the bz_schema table and store it.
    $self->_bz_init_schema_storage();
   
    # We don't use bz_table_list here, because that uses _bz_real_schema.
    # We actually want the table list from the ABSTRACT_SCHEMA in
    # Bugzilla::DB::Schema.
    my @desired_tables = $self->_bz_schema->get_table_list();
    my $bugs_exists = $self->bz_table_info('bugs');
    if (!$bugs_exists) {
        say install_string('db_table_setup');
    }

    foreach my $table_name (@desired_tables) {
        $self->bz_add_table($table_name, { silently => !$bugs_exists });
    }
}

# This really just exists to get overridden in Bugzilla::DB::Mysql.
sub bz_enum_initial_values {
    return ENUM_DEFAULTS;
}

sub bz_populate_enum_tables {
    my ($self) = @_; 

    my $any_severities = $self->selectrow_array(
        'SELECT 1 FROM bug_severity ' . $self->sql_limit(1));
    print install_string('db_enum_setup'), "\n  " if !$any_severities;

    my $enum_values = $self->bz_enum_initial_values();
    while (my ($table, $values) = each %$enum_values) {
        $self->_bz_populate_enum_table($table, $values);
    }

    print "\n" if !$any_severities;
}

sub bz_setup_foreign_keys {
    my ($self) = @_;

    # profiles_activity was the first table to get foreign keys,
    # so if it doesn't have them, then we're setting up FKs
    # for the first time, and should be quieter about it.
    my $activity_fk = $self->bz_fk_info('profiles_activity', 'userid');
    my $any_fks = $activity_fk && $activity_fk->{created};
    if (!$any_fks) {
        say get_text('install_fk_setup');
    }

    my @tables = $self->bz_table_list();
    foreach my $table (@tables) {
        my @columns = $self->bz_table_columns($table);
        my %add_fks;
        foreach my $column (@columns) {
            # First we check for any FKs that have created => 0,
            # in the _bz_real_schema. This also picks up FKs with
            # created => 1, but bz_add_fks will ignore those.
            my $fk = $self->bz_fk_info($table, $column);
            # Then we check the abstract schema to see if there
            # should be an FK on this column, but one wasn't set in the
            # _bz_real_schema for some reason. We do this to handle
            # various problems caused by upgrading from versions
            # prior to 4.2, and also to handle problems caused
            # by enabling an extension pre-4.2, disabling it for
            # the 4.2 upgrade, and then re-enabling it later.
            unless ($fk && $fk->{created}) {
                my $standard_def = 
                    $self->_bz_schema->get_column_abstract($table, $column);
                if (exists $standard_def->{REFERENCES}) {
                    $fk = dclone($standard_def->{REFERENCES});
                }
            }

            $add_fks{$column} = $fk if $fk;
        }
        $self->bz_add_fks($table, \%add_fks, { silently => !$any_fks });
    }
}

# This is used by contrib/bzdbcopy.pl, mostly.
sub bz_drop_foreign_keys {
    my ($self) = @_;

    my @tables = $self->bz_table_list();
    foreach my $table (@tables) {
        my @columns = $self->bz_table_columns($table);
        foreach my $column (@columns) {
            $self->bz_drop_fk($table, $column);
        }
    }
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
        ThrowCodeError('column_not_null_without_default',
                       { name => "$table.$name" });
    }

    my $current_def = $self->bz_column_info($table, $name);

    if (!$current_def) {
        # REFERENCES need to happen later and not be created right away
        my $trimmed_def = dclone($new_def);
        delete $trimmed_def->{REFERENCES};
        my @statements = $self->_bz_real_schema->get_add_column_ddl(
            $table, $name, $trimmed_def,
            defined $init_value ? $self->quote($init_value) : undef);
        print get_text('install_column_add',
                       { column => $name, table => $table }) . "\n"
            if Bugzilla->usage_mode == USAGE_MODE_CMDLINE;
        foreach my $sql (@statements) {
            $self->do($sql);
        }

        # To make things easier for callers, if they don't specify
        # a REFERENCES item, we pull it from the _bz_schema if the
        # column exists there and has a REFERENCES item.
        # bz_setup_foreign_keys will then add this FK at the end of
        # Install::DB.
        my $col_abstract =
            $self->_bz_schema->get_column_abstract($table, $name);
        if (exists $col_abstract->{REFERENCES}) {
            my $new_fk = dclone($col_abstract->{REFERENCES});
            $new_fk->{created} = 0;
            $new_def->{REFERENCES} = $new_fk;
        }

        $self->_bz_real_schema->set_column($table, $name, $new_def);
        $self->_bz_store_real_schema;
    }
}

sub bz_add_fk {
    my ($self, $table, $column, $def) = @_;
    $self->bz_add_fks($table, { $column => $def });
}

sub bz_add_fks {
    my ($self, $table, $column_fks, $options) = @_;

    my %add_these;
    foreach my $column (keys %$column_fks) {
        my $current_fk = $self->bz_fk_info($table, $column);
        next if ($current_fk and $current_fk->{created});
        my $new_fk = $column_fks->{$column};
        $self->_check_references($table, $column, $new_fk);
        $add_these{$column} = $new_fk;
        if (Bugzilla->usage_mode == USAGE_MODE_CMDLINE 
            and !$options->{silently}) 
        {
            print get_text('install_fk_add',
                           { table => $table, column => $column, 
                             fk    => $new_fk }), "\n";
        }
    }

    return if !scalar(keys %add_these);

    my @sql = $self->_bz_real_schema->get_add_fks_sql($table, \%add_these);
    $self->do($_) foreach @sql;

    foreach my $column (keys %add_these) {
        my $fk_def = $add_these{$column};
        $fk_def->{created} = 1;
        $self->_bz_real_schema->set_fk($table, $column, $fk_def);
    }

    $self->_bz_store_real_schema();
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
            ThrowCodeError('column_not_null_no_default_alter', 
                           { name => "$table.$name" }) if ($any_nulls);
        }
        # Preserve foreign key definitions in the Schema object when altering
        # types.
        if (my $fk = $self->bz_fk_info($table, $name)) {
            $new_def->{REFERENCES} = $fk;
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
    say "Updating column $name in table $table ...";
    if (defined $current_def) {
        my $old_ddl = $self->_bz_schema->get_type_ddl($current_def);
        say "Old: $old_ddl";
    }
    say "New: $new_ddl";
    $self->do($_) foreach (@statements);
}

sub bz_alter_fk {
    my ($self, $table, $column, $fk_def) = @_;
    my $current_fk = $self->bz_fk_info($table, $column);
    ThrowCodeError('column_alter_nonexistent_fk',
                   { table => $table, column => $column }) if !$current_fk;
    $self->bz_drop_fk($table, $column);
    $self->bz_add_fk($table, $column, $fk_def);
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
    my ($self, $name, $options) = @_;

    my $table_exists = $self->bz_table_info($name);

    if (!$table_exists) {
        $self->_bz_add_table_raw($name, $options);
        my $table_def = dclone($self->_bz_schema->get_table_abstract($name));

        my %fields = @{$table_def->{FIELDS}};
        foreach my $col (keys %fields) {
            # Foreign Key references have to be added by Install::DB after
            # initial table creation, because column names have changed
            # over history and it's impossible to keep track of that info
            # in ABSTRACT_SCHEMA.
            next unless exists $fields{$col}->{REFERENCES};
            $fields{$col}->{REFERENCES}->{created} =
                $self->_bz_real_schema->FK_ON_CREATE;
        }
        
        $self->_bz_real_schema->add_table($name, $table_def);
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
    my ($self, $name, $options) = @_;
    my @statements = $self->_bz_schema->get_table_ddl($name);
    if (Bugzilla->usage_mode == USAGE_MODE_CMDLINE
        and !$options->{silently})
    {
        say install_string('db_table_new', { table => $name });
    }
    $self->do($_) foreach (@statements);
}

sub _bz_add_field_table {
    my ($self, $name, $schema_ref) = @_;
    # We do nothing if the table already exists.
    return if $self->bz_table_info($name);

    # Copy this so that we're not modifying the passed reference.
    # (This avoids modifying a constant in Bugzilla::DB::Schema.)
    my %table_schema = %$schema_ref;
    my %indexes = @{ $table_schema{INDEXES} };
    my %fixed_indexes;
    foreach my $key (keys %indexes) {
        $fixed_indexes{$name . "_" . $key} = $indexes{$key};
    }
    # INDEXES is supposed to be an arrayref, so we have to convert back.
    my @indexes_array = %fixed_indexes;
    $table_schema{INDEXES} = \@indexes_array;
    # We add this to the abstract schema so that bz_add_table can find it.
    $self->_bz_schema->add_table($name, \%table_schema);
    $self->bz_add_table($name);
}

sub bz_add_field_tables {
    my ($self, $field) = @_;
    
    $self->_bz_add_field_table($field->name,
                               $self->_bz_schema->FIELD_TABLE_SCHEMA, $field->type);
    if ($field->type == FIELD_TYPE_MULTI_SELECT) {
        my $ms_table = "bug_" . $field->name;
        $self->_bz_add_field_table($ms_table,
            $self->_bz_schema->MULTI_SELECT_VALUE_TABLE);

        $self->bz_add_fks($ms_table, 
            { bug_id => {TABLE => 'bugs', COLUMN => 'bug_id',
                         DELETE => 'CASCADE'},

              value  => {TABLE  => $field->name, COLUMN => 'value'} });
    }
}

sub bz_drop_field_tables {
    my ($self, $field) = @_;
    if ($field->type == FIELD_TYPE_MULTI_SELECT) {
        $self->bz_drop_table('bug_' . $field->name);
    }
    $self->bz_drop_table($field->name);
}

sub bz_drop_column {
    my ($self, $table, $column) = @_;

    my $current_def = $self->bz_column_info($table, $column);

    if ($current_def) {
        my @statements = $self->_bz_real_schema->get_drop_column_ddl(
            $table, $column);
        print get_text('install_column_drop', 
                       { table => $table, column => $column }) . "\n"
            if Bugzilla->usage_mode == USAGE_MODE_CMDLINE;
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

sub bz_drop_fk {
    my ($self, $table, $column) = @_;

    my $fk_def = $self->bz_fk_info($table, $column);
    if ($fk_def and $fk_def->{created}) {
        print get_text('install_fk_drop',
                       { table => $table, column => $column, fk => $fk_def })
            . "\n" if Bugzilla->usage_mode == USAGE_MODE_CMDLINE;
        my @statements = 
            $self->_bz_real_schema->get_drop_fk_sql($table, $column, $fk_def);
        foreach my $sql (@statements) {
            # Because this is a deletion, we don't want to die hard if
            # we fail because of some local customization. If something
            # is already gone, that's fine with us!
            eval { $self->do($sql); } or warn "Failed SQL: [$sql] Error: $@";
        }
        # Under normal circumstances, we don't permanently drop the fk--
        # we want checksetup to re-create it again later. The only
        # time that FKs get permanently dropped is if the column gets
        # dropped.
        $fk_def->{created} = 0;
        $self->_bz_real_schema->set_fk($table, $column, $fk_def);
        $self->_bz_store_real_schema;
    }

}

sub bz_get_related_fks {
    my ($self, $table, $column) = @_;
    my @tables = $self->_bz_real_schema->get_table_list();
    my @related;
    foreach my $check_table (@tables) {
        my @columns = $self->bz_table_columns($check_table);
        foreach my $check_column (@columns) {
            my $fk = $self->bz_fk_info($check_table, $check_column);
            if ($fk 
                and (($fk->{TABLE} eq $table and $fk->{COLUMN} eq $column)
                     or ($check_column eq $column and $check_table eq $table)))
            {
                push(@related, [$check_table, $check_column, $fk]);
            }
        } # foreach $column
    } # foreach $table

    return \@related;
}

sub bz_drop_related_fks {
    my $self = shift;
    my $related = $self->bz_get_related_fks(@_);
    foreach my $item (@$related) {
        my ($table, $column) = @$item;
        $self->bz_drop_fk($table, $column);
    }
    return $related;
}

sub bz_drop_index {
    my ($self, $table, $name) = @_;

    my $index_exists = $self->bz_index_info($table, $name);

    if ($index_exists) {
        if ($self->INDEX_DROPS_REQUIRE_FK_DROPS) {
            # We cannot delete an index used by a FK.
            foreach my $column (@{$index_exists->{FIELDS}}) {
                $self->bz_drop_related_fks($table, $column);
            }
        }
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
        print get_text('install_table_drop', { name => $name }) . "\n"
            if Bugzilla->usage_mode == USAGE_MODE_CMDLINE;
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

sub bz_fk_info {
    my ($self, $table, $column) = @_;
    my $col_info = $self->bz_column_info($table, $column);
    return undef if !$col_info;
    my $fk = $col_info->{REFERENCES};
    return $fk;
}

sub bz_rename_column {
    my ($self, $table, $old_name, $new_name) = @_;

    my $old_col_exists  = $self->bz_column_info($table, $old_name);

    if ($old_col_exists) {
        my $already_renamed = $self->bz_column_info($table, $new_name);
            ThrowCodeError('db_rename_conflict',
                           { old => "$table.$old_name", 
                             new => "$table.$new_name" }) if $already_renamed;
        my @statements = $self->_bz_real_schema->get_rename_column_ddl(
            $table, $old_name, $new_name);

        print get_text('install_column_rename', 
                       { old => "$table.$old_name", new => "$table.$new_name" })
               . "\n" if Bugzilla->usage_mode == USAGE_MODE_CMDLINE;

        foreach my $sql (@statements) {
            $self->do($sql);
        }
        $self->_bz_real_schema->rename_column($table, $old_name, $new_name);
        $self->_bz_store_real_schema;
    }
}

sub bz_rename_table {
    my ($self, $old_name, $new_name) = @_;
    my $old_table = $self->bz_table_info($old_name);
    return if !$old_table;

    my $new = $self->bz_table_info($new_name);
    ThrowCodeError('db_rename_conflict', { old => $old_name,
                                           new => $new_name }) if $new;

    # FKs will all have the wrong names unless we drop and then let them
    # be re-created later. Under normal circumstances, checksetup.pl will
    # automatically re-create these dropped FKs at the end of its DB upgrade
    # run, so we don't need to re-create them in this method.
    my @columns = $self->bz_table_columns($old_name);
    foreach my $column (@columns) {
        # these just return silently if there's no FK to drop
        $self->bz_drop_fk($old_name, $column);
        $self->bz_drop_related_fks($old_name, $column);
    }

    my @sql = $self->_bz_real_schema->get_rename_table_sql($old_name, $new_name);
    print get_text('install_table_rename', 
                   { old => $old_name, new => $new_name }) . "\n"
        if Bugzilla->usage_mode == USAGE_MODE_CMDLINE;
    $self->do($_) foreach @sql;
    $self->_bz_real_schema->rename_table($old_name, $new_name);
    $self->_bz_store_real_schema;
}

sub bz_set_next_serial_value {
    my ($self, $table, $column, $value) = @_;
    if (!$value) {
        $value = $self->selectrow_array("SELECT MAX($column) FROM $table") || 0;
        $value++;
    }
    my @sql = $self->_bz_real_schema->get_set_serial_sql($table, $column, $value);
    $self->do($_) foreach @sql;
}

#####################################################################
# Schema Information Methods
#####################################################################

sub _bz_schema {
    my ($self) = @_;
    return $self->{private_bz_schema} if exists $self->{private_bz_schema};
    my @module_parts = split('::', ref $self);
    my $module_name  = pop @module_parts;
    $self->{private_bz_schema} = Bugzilla::DB::Schema->new($module_name);
    return $self->{private_bz_schema};
}

# _bz_get_initial_schema()
#
# Description: A protected method, intended for use only by Bugzilla::DB
#              and subclasses. Used to get the initial Schema that will
#              be written to disk for _bz_init_schema_storage. You probably
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
    my $def = $self->_bz_real_schema->get_column_abstract($table, $column);
    # We dclone it so callers can't modify the Schema.
    $def = dclone($def) if defined $def;
    return $def;
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
    return $self->_bz_real_schema->get_table_columns($table);
}

sub bz_table_indexes {
    my ($self, $table) = @_;
    my $indexes = $self->_bz_real_schema->get_table_indexes_abstract($table);
    my %return_indexes;
    # We do this so that they're always hashes.
    foreach my $name (keys %$indexes) {
        $return_indexes{$name} = $self->bz_index_info($table, $name);
    }
    return \%return_indexes;
}

sub bz_table_list {
    my ($self) = @_;
    return $self->_bz_real_schema->get_table_list();
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

sub bz_in_transaction {
    return $_[0]->{private_bz_transaction_count} ? 1 : 0;
}

sub bz_start_transaction {
    my ($self) = @_;

    if ($self->bz_in_transaction) {
        $self->{private_bz_transaction_count}++;
    } else {
        # Turn AutoCommit off and start a new transaction
        $self->begin_work();
        # REPEATABLE READ means "We work on a snapshot of the DB that
        # is created when we execute our first SQL statement." It's
        # what we need in Bugzilla to be safe, for what we do.
        # Different DBs have different defaults for their isolation
        # level, so we just set it here manually.
        if ($self->ISOLATION_LEVEL) {
            $self->do('SET TRANSACTION ISOLATION LEVEL ' 
                      . $self->ISOLATION_LEVEL);
        }
        $self->{private_bz_transaction_count} = 1;
    }
}

sub bz_commit_transaction {
    my ($self) = @_;

    if ($self->{private_bz_transaction_count} > 1) {
        $self->{private_bz_transaction_count}--;
    } elsif ($self->bz_in_transaction) {
        $self->commit();
        $self->{private_bz_transaction_count} = 0;
        Bugzilla::Mailer->send_staged_mail();
    } else {
       ThrowCodeError('not_in_transaction');
    }
}

sub bz_rollback_transaction {
    my ($self) = @_;

    # Unlike start and commit, if we rollback at any point it happens
    # instantly, even if we're in a nested transaction.
    if (!$self->bz_in_transaction) {
        ThrowCodeError("not_in_transaction");
    } else {
        $self->rollback();
        $self->{private_bz_transaction_count} = 0;
    }
}

#####################################################################
# Subclass Helpers
#####################################################################

sub db_new {
    my ($class, $params) = @_;
    my ($dsn, $user, $pass, $override_attrs) = 
        @$params{qw(dsn user pass attrs)};

    # set up default attributes used to connect to the database
    # (may be overridden by DB driver implementations)
    my $attributes = { RaiseError => 0,
                       AutoCommit => 1,
                       PrintError => 0,
                       ShowErrorStatement => 1,
                       HandleError => \&_handle_error,
                       TaintIn => 1,
                       # See https://rt.perl.org/rt3/Public/Bug/Display.html?id=30933
                       # for the reason to use NAME instead of NAME_lc (bug 253696).
                       FetchHashKeyName => 'NAME',
                     };

    if ($override_attrs) {
        foreach my $key (keys %$override_attrs) {
            $attributes->{$key} = $override_attrs->{$key};
        }
    }

    # connect using our known info to the specified db
    my $self = DBI->connect($dsn, $user, $pass, $attributes)
        or die "\nCan't connect to the database.\nError: $DBI::errstr\n"
        . "  Is your database installed and up and running?\n  Do you have"
        . " the correct username and password selected in localconfig?\n\n";

    # RaiseError was only set to 0 so that we could catch the 
    # above "die" condition.
    $self->{RaiseError} = 1;

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

        say install_string('db_schema_init');
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

    my $bz_schema;
    unless ($bz_schema = Bugzilla->memcached->get({ key => 'bz_schema' })) {
        $bz_schema = $self->selectrow_arrayref(
            "SELECT schema_data, version FROM bz_schema"
        );
        Bugzilla->memcached->set({ key => 'bz_schema', value => $bz_schema });
    }

    (die "_bz_real_schema tried to read the bz_schema table but it's empty!")
        if !$bz_schema;

    $self->{private_real_schema} =
        $self->_bz_schema->deserialize_abstract($bz_schema->[0], $bz_schema->[1]);

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

    Bugzilla->memcached->clear({ key => 'bz_schema' });
}

# For bz_populate_enum_tables
sub _bz_populate_enum_table {
    my ($self, $table, $valuelist) = @_;

    my $sql_table = $self->quote_identifier($table);

    # Check if there are any table entries
    my $table_size = $self->selectrow_array("SELECT COUNT(*) FROM $sql_table");

    # If the table is empty...
    if (!$table_size) {
        print " $table";
        my $insert = $self->prepare(
            "INSERT INTO $sql_table (value,sortkey) VALUES (?,?)");
        my $sortorder = 0;
        my $maxlen    = max(map(length($_), @$valuelist)) + 2;
        foreach my $value (@$valuelist) {
            $sortorder += 100;
            $insert->execute($value, $sortorder);
        }
    }
}

# This is used before adding a foreign key to a column, to make sure
# that the database won't fail adding the key.
sub _check_references {
    my ($self, $table, $column, $fk) = @_;
    my $foreign_table = $fk->{TABLE};
    my $foreign_column = $fk->{COLUMN};

    # We use table aliases because sometimes we join a table to itself,
    # and we can't use the same table name on both sides of the join.
    # We also can't use the words "table" or "foreign" because those are
    # reserved words.
    my $bad_values = $self->selectcol_arrayref(
        "SELECT DISTINCT tabl.$column 
           FROM $table AS tabl LEFT JOIN $foreign_table AS forn
                ON tabl.$column = forn.$foreign_column
          WHERE forn.$foreign_column IS NULL
                AND tabl.$column IS NOT NULL");

    if (@$bad_values) {
        my $delete_action = $fk->{DELETE} || '';
        if ($delete_action eq 'CASCADE') {
            $self->do("DELETE FROM $table WHERE $column IN (" 
                      . join(',', ('?') x @$bad_values)  . ")",
                      undef, @$bad_values);
            if (Bugzilla->usage_mode == USAGE_MODE_CMDLINE) {
                print "\n", get_text('install_fk_invalid_fixed',
                    { table => $table, column => $column,
                      foreign_table => $foreign_table,
                      foreign_column => $foreign_column,
                      'values' => $bad_values, action => 'delete' }), "\n";
            }
        }
        elsif ($delete_action eq 'SET NULL') {
            $self->do("UPDATE $table SET $column = NULL
                        WHERE $column IN ("
                      . join(',', ('?') x @$bad_values)  . ")",
                      undef, @$bad_values);
            if (Bugzilla->usage_mode == USAGE_MODE_CMDLINE) {
                print "\n", get_text('install_fk_invalid_fixed',
                    { table => $table, column => $column,
                      foreign_table => $foreign_table, 
                      foreign_column => $foreign_column,
                      'values' => $bad_values, action => 'null' }), "\n";
            }
        }
        else {
            die "\n", get_text('install_fk_invalid',
                { table => $table, column => $column,
                  foreign_table => $foreign_table,
                  foreign_column => $foreign_column,
                 'values' => $bad_values }), "\n";
        }
    }
}

1;

__END__

=head1 NAME

Bugzilla::DB - Database access routines, using L<DBI|https://metacpan.org/pod/DBI>

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

=head1 DESCRIPTION

Functions in this module allows creation of a database handle to connect
to the Bugzilla database. This should never be done directly; all users
should use the L<Bugzilla> module to access the current C<dbh> instead.

This module also contains methods extending the returned handle with
functionality which is different between databases allowing for easy
customization for particular database via inheritance. These methods
should be always preffered over hard-coding SQL commands.

=head1 CONSTANTS

Subclasses of Bugzilla::DB are required to define certain constants. These
constants are required to be subroutines or "use constant" variables.

=over

=item C<BLOB_TYPE>

The C<\%attr> argument that must be passed to bind_param in order to 
correctly escape a C<LONGBLOB> type.

=item C<ISOLATION_LEVEL>

The argument that this database should send to 
C<SET TRANSACTION ISOLATION LEVEL> when starting a transaction. If you
override this in a subclass, the isolation level you choose should
be as strict as or more strict than the default isolation level defined in
L<Bugzilla::DB>.

=back


=head1 CONNECTION

A new database handle to the required database can be created using this
module. This is normally done by the L<Bugzilla> module, and so these routines
should not be called from anywhere else.

=head2 Functions

=over

=item C<connect_main>

=over

=item B<Description>

Function to connect to the main database, returning a new database handle.

=item B<Params>

=over

=item C<$no_db_name> (optional) - If true, connect to the database
server, but don't connect to a specific database. This is only used 
when creating a database. After you create the database, you should 
re-create a new Bugzilla::DB object without using this parameter. 

=back

=item B<Returns>

New instance of the DB class

=back

=item C<connect_shadow>

=over

=item B<Description>

Function to connect to the shadow database, returning a new database handle.
This routine C<die>s if no shadow database is configured.

=item B<Params> (none)

=item B<Returns>

A new instance of the DB class

=back

=item C<bz_check_requirements>

=over

=item B<Description>

Checks to make sure that you have the correct DBD and database version 
installed for the database that Bugzilla will be using. Prints a message 
and exits if you don't pass the requirements.

If C<$db_check> is false (from F<localconfig>), we won't check the 
database version.

=item B<Params>

=over

=item C<$output> - C<true> if the function should display informational 
output about what it's doing, such as versions found.

=back

=item B<Returns> (nothing)

=back


=item C<bz_create_database>

=over

=item B<Description>

Creates an empty database with the name C<$db_name>, if that database 
doesn't already exist. Prints an error message and exits if we can't 
create the database.

=item B<Params> (none)

=item B<Returns> (nothing)

=back

=item C<_connect>

=over

=item B<Description>

Internal function, creates and returns a new, connected instance of the 
correct DB class.  This routine C<die>s if no driver is specified.

=item B<Params>

=over

=item C<$driver> - name of the database driver to use

=item C<$host> - host running the database we are connecting to

=item C<$dbname> - name of the database to connect to

=item C<$port> - port the database is listening on

=item C<$sock> - socket the database is listening on

=item C<$user> - username used to log in to the database

=item C<$pass> - password used to log in to the database

=back

=item B<Returns>

A new instance of the DB class

=back

=item C<_handle_error>

Function passed to the DBI::connect call for error handling. It shortens the 
error for printing.

=item C<import>

Overrides the standard import method to check that derived class
implements all required abstract methods. Also calls original implementation 
in its super class.

=back

=head1 ABSTRACT METHODS

Note: Methods which can be implemented generically for all DBs are implemented in
this module. If needed, they can be overridden with DB specific code.
Methods which do not have standard implementation are abstract and must
be implemented for all supported databases separately.
To avoid confusion with standard DBI methods, all methods returning string with
formatted SQL command have prefix C<sql_>. All other methods have prefix C<bz_>.

=head2 Constructor

=over

=item C<new>

=over

=item B<Description>

Constructor.  Abstract method, should be overridden by database specific 
code.

=item B<Params>

=over 

=item C<$user> - username used to log in to the database

=item C<$pass> - password used to log in to the database

=item C<$host> - host running the database we are connecting to

=item C<$dbname> - name of the database to connect to

=item C<$port> - port the database is listening on

=item C<$sock> - socket the database is listening on

=back

=item B<Returns>

A new instance of the DB class

=item B<Note>

The constructor should create a DSN from the parameters provided and
then call C<db_new()> method of its super class to create a new
class instance. See L<db_new> description in this module. As per
DBI documentation, all class variables must be prefixed with
"private_". See L<DBI|https://metacpan.org/pod/DBI>.

=back

=back

=head2 SQL Generation

=over

=item C<sql_regexp>

=over

=item B<Description>

Outputs SQL regular expression operator for POSIX regex
searches (case insensitive) in format suitable for a given
database.

Abstract method, should be overridden by database specific code.

=item B<Params>

=over

=item C<$expr> - SQL expression for the text to be searched (scalar)

=item C<$pattern> - the regular expression to search for (scalar)

=item C<$nocheck> - true if the pattern should not be tested; false otherwise (boolean)

=item C<$real_pattern> - the real regular expression to search for.
This argument is used when C<$pattern> is a placeholder ('?').

=back

=item B<Returns>

Formatted SQL for regular expression search (e.g. REGEXP) (scalar)

=back

=item C<sql_not_regexp>

=over

=item B<Description>

Outputs SQL regular expression operator for negative POSIX
regex searches (case insensitive) in format suitable for a given
database.

Abstract method, should be overridden by database specific code.

=item B<Params>

Same as L</sql_regexp>.

=item B<Returns>

Formatted SQL for negative regular expression search (e.g. NOT REGEXP) 
(scalar)

=back

=item C<sql_limit>

=over

=item B<Description>

Returns SQL syntax for limiting results to some number of rows
with optional offset if not starting from the begining.

Abstract method, should be overridden by database specific code.

=item B<Params>

=over

=item C<$limit> - number of rows to return from query (scalar)

=item C<$offset> - number of rows to skip before counting (scalar)

=back

=item B<Returns>

Formatted SQL for limiting number of rows returned from query
with optional offset (e.g. LIMIT 1, 1) (scalar)

=back

=item C<sql_from_days>

=over

=item B<Description>

Outputs SQL syntax for converting Julian days to date.

Abstract method, should be overridden by database specific code.

=item B<Params>

=over

=item C<$days> - days to convert to date

=back

=item B<Returns>

Formatted SQL for returning Julian days in dates. (scalar)

=back

=item C<sql_to_days>

=over

=item B<Description>

Outputs SQL syntax for converting date to Julian days.

Abstract method, should be overridden by database specific code.

=item B<Params>

=over

=item C<$date> - date to convert to days

=back

=item B<Returns>

Formatted SQL for returning date fields in Julian days. (scalar)

=back

=item C<sql_date_format>

=over

=item B<Description>

Outputs SQL syntax for formatting dates.

Abstract method, should be overridden by database specific code.

=item B<Params>

=over

=item C<$date> - date or name of date type column (scalar)

=item C<$format> - format string for date output (scalar)
(C<%Y> = year, four digits, C<%y> = year, two digits, C<%m> = month,
C<%d> = day, C<%a> = weekday name, 3 letters, C<%H> = hour 00-23,
C<%i> = minute, C<%s> = second)

=back

=item B<Returns>

Formatted SQL for date formatting (scalar)

=back

=item C<sql_date_math>

=over

=item B<Description>

Outputs proper SQL syntax for adding some amount of time to a date.

Abstract method, should be overridden by database specific code.

=item B<Params>

=over

=item C<$date>

C<string> The date being added to or subtracted from.

=item C<$operator>

C<string> Either C<-> or C<+>, depending on whether you're subtracting
or adding.

=item C<$interval>

C<integer> The time interval you're adding or subtracting (e.g. C<30>)

=item C<$units> 

C<string> the units the interval is in (e.g. 'MINUTE')

=back

=item B<Returns>

Formatted SQL for adding or subtracting a date and some amount of time (scalar)

=back

=item C<sql_position>

=over

=item B<Description>

Outputs proper SQL syntax determining position of a substring
(fragment) withing a string (text). Note: if the substring or
text are string constants, they must be properly quoted (e.g. "'pattern'").

It searches for the string in a case-sensitive manner. If you want to do
a case-insensitive search, use L</sql_iposition>.

=item B<Params>

=over

=item C<$fragment> - the string fragment we are searching for (scalar)

=item C<$text> - the text to search (scalar)

=back

=item B<Returns>

Formatted SQL for substring search (scalar)

=back

=item C<sql_iposition>

Just like L</sql_position>, but case-insensitive.

=item C<sql_like>

=over

=item B<Description>

Outputs SQL to search for an instance of a string (fragment)
in a table column (column).

Note that the fragment must not be quoted. L</sql_like> will
quote the fragment itself.

This is a case sensitive search.

Note: This does not necessarily generate an ANSI LIKE statement, but
could be overridden to do so in a database subclass if required.

=item B<Params>

=over

=item C<$fragment> - the string fragment that we are searching for (scalar)

=item C<$column> - the column to search

=back

=item B<Returns>

Formatted SQL to return results from columns that contain the fragment.

=back

=item C<sql_ilike>

Just like L</sql_like>, but case-insensitive.

=item C<sql_not_ilike>

=over

=item B<Description>

Outputs SQL to search for columns (column) that I<do not> contain
instances of the string (fragment).

Note that the fragment must not be quoted. L</sql_not_ilike> will
quote the fragment itself.

This is a case insensitive search.

=item B<Params>

=over

=item C<$fragment> - the string fragment that we are searching for (scalar)

=item C<$column> - the column to search

=back

=item B<Returns>

Formated sql to return results from columns that do not contain the fragment

=back

=item C<sql_group_by>

=over

=item B<Description>

Outputs proper SQL syntax for grouping the result of a query.

For ANSI SQL databases, we need to group by all columns we are
querying for (except for columns used in aggregate functions).
Some databases require (or even allow) to specify only one
or few columns if the result is uniquely defined. For those
databases, the default implementation needs to be overloaded.

=item B<Params>

=over

=item C<$needed_columns> - string with comma separated list of columns
we need to group by to get expected result (scalar)

=item C<$optional_columns> - string with comma separated list of all
other columns we are querying for, but which are not in the required list.

=back

=item B<Returns>

Formatted SQL for row grouping (scalar)

=back

=item C<sql_string_concat>

=over

=item B<Description>

Returns SQL syntax for concatenating multiple strings (constants
or values from table columns) together.

=item B<Params>

=over

=item C<@params> - array of column names or strings to concatenate

=back

=item B<Returns>

Formatted SQL for concatenating specified strings

=back

=item C<sql_string_until>

=over

=item B<Description>

Returns SQL for truncating a string at the first occurrence of a certain
substring.

=item B<Params>

Note that both parameters need to be sql-quoted.

=item C<$string> The string we're truncating

=item C<$substring> The substring we're truncating at.

=back

=item C<sql_fulltext_search>

=over

=item B<Description>

Returns one or two SQL expressions for performing a full text search for
specified text on a given column.

If one value is returned, it is a numeric expression that indicates
a match with a positive value and a non-match with zero. In this case,
the DB must support casting numeric expresions to booleans.

If two values are returned, then the first value is a boolean expression
that indicates the presence of a match, and the second value is a numeric
expression that can be used for ranking.

There is a ANSI SQL version of this method implemented using LIKE operator,
but it's not a real full text search. DB specific modules should override 
this, as this generic implementation will be always much slower. This 
generic implementation returns 'relevance' as 0 for no match, or 1 for a 
match.

=item B<Params>

=over

=item C<$column> - name of column to search (scalar)

=item C<$text> - text to search for (scalar)

=back

=item B<Returns>

Formatted SQL for full text search

=back

=item C<sql_istrcmp>

=over

=item B<Description>

Returns SQL for a case-insensitive string comparison.

=item B<Params>

=over

=item C<$left> - What should be on the left-hand-side of the operation.

=item C<$right> - What should be on the right-hand-side of the operation.

=item C<$op> (optional) - What the operation is. Should be a  valid ANSI 
SQL comparison operator, such as C<=>, C<E<lt>>, C<LIKE>, etc. Defaults 
to C<=> if not specified.

=back

=item B<Returns>

A SQL statement that will run the comparison in a case-insensitive fashion.

=item B<Note>

Uses L</sql_istring>, so it has the same performance concerns.
Try to avoid using this function unless absolutely necessary.

Subclass Implementors: Override sql_istring instead of this
function, most of the time (this function uses sql_istring).

=back

=item C<sql_istring>

=over

=item B<Description>

Returns SQL syntax "preparing" a string or text column for case-insensitive 
comparison.

=item B<Params>

=over

=item C<$string> - string to convert (scalar)

=back

=item B<Returns>

Formatted SQL making the string case insensitive.

=item B<Note>

The default implementation simply calls LOWER on the parameter.
If this is used to search on a text column with index, the index
will not be usually used unless it was created as LOWER(column).

=back

=item C<sql_in>

=over

=item B<Description>

Returns SQL syntax for the C<IN ()> operator. 

Only necessary where an C<IN> clause can have more than 1000 items.

=item B<Params>

=over

=item C<$column_name> - Column name (e.g. C<bug_id>)

=item C<$in_list_ref> - an arrayref containing values for C<IN ()>

=back

=item B<Returns>

Formatted SQL for the C<IN> operator.

=back

=back


=head1 IMPLEMENTED METHODS

These methods are implemented in Bugzilla::DB, and only need
to be implemented in subclasses if you need to override them for 
database-compatibility reasons.

=head2 General Information Methods

These methods return information about data in the database.

=over

=item C<bz_last_key>

=over

=item B<Description>

Returns the last serial number, usually from a previous INSERT.

Must be executed directly following the relevant INSERT.
This base implementation uses DBI's
L<last_insert_id|https://metacpan.org/pod/DBI#last_insert_id>. If the
DBD supports it, it is the preffered way to obtain the last
serial index. If it is not supported, the DB-specific code
needs to override this function.

=item B<Params>

=over

=item C<$table> - name of table containing serial column (scalar)

=item C<$column> - name of column containing serial data type (scalar)

=back

=item B<Returns>

Last inserted ID (scalar)

=back

=back

=head2 Database Setup Methods

These methods are used by the Bugzilla installation programs to set up
the database.

=over

=item C<bz_populate_enum_tables>

=over

=item B<Description>

For an upgrade or an initial installation, populates the tables that hold 
the legal values for the old "enum" fields: C<bug_severity>, 
C<resolution>, etc. Prints out information if it inserts anything into the
DB.

=item B<Params> (none)

=item B<Returns> (nothing)

=back

=back


=head2 Schema Modification Methods

These methods modify the current Bugzilla Schema.

Where a parameter says "Abstract index/column definition", it returns/takes
information in the formats defined for indexes and columns in
C<Bugzilla::DB::Schema::ABSTRACT_SCHEMA>.

=over

=item C<bz_add_column>

=over

=item B<Description>

Adds a new column to a table in the database. Prints out a brief statement 
that it did so, to stdout. Note that you cannot add a NOT NULL column that 
has no default -- the database won't know what to set all the NULL
values to.

=item B<Params>

=over

=item C<$table> - the table where the column is being added

=item C<$name> - the name of the new column

=item C<\%definition> - Abstract column definition for the new column

=item C<$init_value> (optional) - An initial value to set the column
to. Required if your column is NOT NULL and has no DEFAULT set.

=back

=item B<Returns> (nothing)

=back

=item C<bz_add_index>

=over

=item B<Description>

Adds a new index to a table in the database. Prints out a brief statement 
that it did so, to stdout. If the index already exists, we will do nothing.

=item B<Params>

=over

=item C<$table> - The table the new index is on.

=item C<$name>  - A name for the new index.

=item C<$definition> - An abstract index definition. Either a hashref 
or an arrayref.

=back

=item B<Returns> (nothing)

=back

=item C<bz_add_table>

=over

=item B<Description>

Creates a new table in the database, based on the definition for that 
table in the abstract schema.

Note that unlike the other 'add' functions, this does not take a 
definition, but always creates the table as it exists in
L<Bugzilla::DB::Schema/ABSTRACT_SCHEMA>.

If a table with that name already exists, then this function returns 
silently.

=item B<Params>

=over

=item C<$name> - The name of the table you want to create.

=back

=item B<Returns> (nothing)

=back

=item C<bz_drop_index>

=over

=item B<Description>

Removes an index from the database. Prints out a brief statement that it 
did so, to stdout. If the index doesn't exist, we do nothing.

=item B<Params>

=over

=item C<$table> - The table that the index is on.

=item C<$name> - The name of the index that you want to drop.

=back

=item B<Returns> (nothing)

=back

=item C<bz_drop_table>

=over

=item B<Description>

Drops a table from the database. If the table doesn't exist, we just 
return silently.

=item B<Params>

=over

=item C<$name> - The name of the table to drop.

=back

=item B<Returns> (nothing)

=back

=item C<bz_alter_column>

=over

=item B<Description>

Changes the data type of a column in a table. Prints out the changes 
being made to stdout. If the new type is the same as the old type, 
the function returns without changing anything.

=item B<Params>

=over

=item C<$table> - the table where the column is

=item C<$name> - the name of the column you want to change

=item C<\%new_def> - An abstract column definition for the new 
data type of the columm

=item C<$set_nulls_to> (Optional) - If you are changing the column
to be NOT NULL, you probably also want to set any existing NULL columns 
to a particular value. Specify that value here. B<NOTE>: The value should 
not already be SQL-quoted.

=back

=item B<Returns> (nothing)

=back

=item C<bz_drop_column>

=over

=item B<Description>

Removes a column from a database table. If the column doesn't exist, we 
return without doing anything. If we do anything, we print a short 
message to C<stdout> about the change.

=item B<Params>

=over

=item C<$table> - The table where the column is

=item C<$column> - The name of the column you want to drop

=back

=item B<Returns> (nothing)

=back

=item C<bz_rename_column>

=over

=item B<Description>

Renames a column in a database table. If the C<$old_name> column 
doesn't exist, we return without doing anything. If C<$old_name> 
and C<$new_name> both already exist in the table specified, we fail.

=item B<Params>

=over

=item C<$table> - The name of the table containing the column 
that you want to rename

=item C<$old_name> - The current name of the column that you want to rename

=item C<$new_name> - The new name of the column

=back

=item B<Returns> (nothing)

=back

=item C<bz_rename_table>

=over

=item B<Description>

Renames a table in the database. Does nothing if the table doesn't exist.

Throws an error if the old table exists and there is already a table 
with the new name.

=item B<Params>

=over

=item C<$old_name> - The current name of the table.

=item C<$new_name> - What you're renaming the table to.

=back

=item B<Returns> (nothing)

=back

=back

=head2 Schema Information Methods

These methods return information about the current Bugzilla database
schema, as it currently exists on the disk. 

Where a parameter says "Abstract index/column definition", it returns/takes
information in the formats defined for indexes and columns for
L<Bugzilla::DB::Schema/ABSTRACT_SCHEMA>.

=over

=item C<bz_column_info>

=over

=item B<Description>

Get abstract column definition.

=item B<Params>

=over

=item C<$table> - The name of the table the column is in.

=item C<$column> - The name of the column.

=back

=item B<Returns>

An abstract column definition for that column. If the table or column 
does not exist, we return C<undef>.

=back

=item C<bz_index_info>

=over

=item B<Description>

Get abstract index definition.

=item B<Params>

=over

=item C<$table> - The table the index is on.

=item C<$index> - The name of the index.

=back

=item B<Returns>

An abstract index definition for that index, always in hashref format. 
The hashref will always contain the C<TYPE> element, but it will
be an empty string if it's just a normal index.

If the index does not exist, we return C<undef>.

=back

=back


=head2 Transaction Methods

These methods deal with the starting and stopping of transactions 
in the database.

=over

=item C<bz_in_transaction>

Returns C<1> if we are currently in the middle of an uncommitted transaction,
C<0> otherwise.

=item C<bz_start_transaction>

Starts a transaction.

It is OK to call C<bz_start_transaction> when you are already inside of
a transaction. However, you must call L</bz_commit_transaction> as many
times as you called C<bz_start_transaction>, in order for your transaction
to actually commit.

Bugzilla uses C<REPEATABLE READ> transactions.

Returns nothing and takes no parameters.

=item C<bz_commit_transaction>

Ends a transaction, commiting all changes. Returns nothing and takes
no parameters.

=item C<bz_rollback_transaction>

Ends a transaction, rolling back all changes. Returns nothing and takes 
no parameters.

=back


=head1 SUBCLASS HELPERS

Methods in this class are intended to be used by subclasses to help them
with their functions.

=over

=item C<db_new>

=over

=item B<Description>

Constructor

=item B<Params>

=over

=item C<$dsn> - database connection string

=item C<$user> - username used to log in to the database

=item C<$pass> - password used to log in to the database

=item C<\%override_attrs> - set of attributes for DB connection (optional).
You only have to set attributes that you want to be different from
the default attributes set inside of C<db_new>.

=back

=item B<Returns>

A new instance of the DB class

=item B<Note>

The name of this constructor is not C<new>, as that would make
our check for implementation of C<new> by derived class useless.

=back

=back


=head1 SEE ALSO

L<DBI|https://metacpan.org/pod/DBI>

L<Bugzilla::Constants/DB_MODULE>

=head1 B<Methods in need of POD>

=over

=item bz_add_fks

=item bz_add_fk

=item bz_drop_index_raw

=item bz_table_info

=item bz_add_index_raw

=item bz_get_related_fks

=item quote

=item bz_drop_fk

=item bz_drop_field_tables

=item bz_drop_related_fks

=item bz_table_columns

=item bz_drop_foreign_keys

=item bz_alter_column_raw

=item bz_table_list_real

=item bz_fk_info

=item bz_setup_database

=item bz_setup_foreign_keys

=item bz_table_indexes

=item bz_check_regexp

=item bz_enum_initial_values

=item bz_alter_fk

=item bz_set_next_serial_value

=item bz_table_list

=item bz_table_columns_real

=item bz_check_server_version

=item bz_server_version

=item bz_add_field_tables

=back
