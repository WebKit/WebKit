# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Search;

use 5.10.1;
use strict;
use warnings;

use parent qw(Exporter);
@Bugzilla::Search::EXPORT = qw(
    IsValidQueryType
    split_order_term
);

use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Constants;
use Bugzilla::Group;
use Bugzilla::User;
use Bugzilla::Field;
use Bugzilla::Search::Clause;
use Bugzilla::Search::ClauseGroup;
use Bugzilla::Search::Condition qw(condition);
use Bugzilla::Status;
use Bugzilla::Keyword;

use Data::Dumper;
use Date::Format;
use Date::Parse;
use Scalar::Util qw(blessed);
use List::MoreUtils qw(all firstidx part uniq);
use POSIX qw(INT_MAX floor);
use Storable qw(dclone);
use Time::HiRes qw(gettimeofday tv_interval);

# Description Of Boolean Charts
# -----------------------------
#
# A boolean chart is a way of representing the terms in a logical
# expression.  Bugzilla builds SQL queries depending on how you enter
# terms into the boolean chart. Boolean charts are represented in
# urls as three-tuples of (chart id, row, column). The query form
# (query.cgi) may contain an arbitrary number of boolean charts where
# each chart represents a clause in a SQL query.
#
# The query form starts out with one boolean chart containing one
# row and one column.  Extra rows can be created by pressing the
# AND button at the bottom of the chart.  Extra columns are created
# by pressing the OR button at the right end of the chart. Extra
# charts are created by pressing "Add another boolean chart".
#
# Each chart consists of an arbitrary number of rows and columns.
# The terms within a row are ORed together. The expressions represented
# by each row are ANDed together. The expressions represented by each
# chart are ANDed together.
#
#        ----------------------
#        | col2 | col2 | col3 |
# --------------|------|------|
# | row1 |  a1  |  a2  |      |
# |------|------|------|------|  => ((a1 OR a2) AND (b1 OR b2 OR b3) AND (c1))
# | row2 |  b1  |  b2  |  b3  |
# |------|------|------|------|
# | row3 |  c1  |      |      |
# -----------------------------
#
#        --------
#        | col2 |
# --------------|
# | row1 |  d1  | => (d1)
# ---------------
#
# Together, these two charts represent a SQL expression like this
# SELECT blah FROM blah WHERE ( (a1 OR a2)AND(b1 OR b2 OR b3)AND(c1)) AND (d1)
#
# The terms within a single row of a boolean chart are all constraints
# on a single piece of data.  If you're looking for a bug that has two
# different people cc'd on it, then you need to use two boolean charts.
# This will find bugs with one CC matching 'foo@blah.org' and and another
# CC matching 'bar@blah.org'.
#
# --------------------------------------------------------------
# CC    | equal to
# foo@blah.org
# --------------------------------------------------------------
# CC    | equal to
# bar@blah.org
#
# If you try to do this query by pressing the AND button in the
# original boolean chart then what you'll get is an expression that
# looks for a single CC where the login name is both "foo@blah.org",
# and "bar@blah.org". This is impossible.
#
# --------------------------------------------------------------
# CC    | equal to
# foo@blah.org
# AND
# CC    | equal to
# bar@blah.org
# --------------------------------------------------------------

#############
# Constants #
#############

# When doing searches, NULL datetimes are treated as this date.
use constant EMPTY_DATETIME => '1970-01-01 00:00:00';
use constant EMPTY_DATE     => '1970-01-01';

# This is the regex for real numbers from Regexp::Common, modified to be
# more readable.
use constant NUMBER_REGEX => qr/
    ^[+-]?      # A sign, optionally.

    (?=\d|\.)   # Then either a digit or "."
    \d*         # Followed by many other digits
    (?:
        \.      # Followed possibly by some decimal places
        (?:\d*)
    )?
 
    (?:         # Followed possibly by an exponent.
        [Ee]
        [+-]?
        \d+
    )?
    $
/x;

# If you specify a search type in the boolean charts, this describes
# which operator maps to which internal function here.
use constant OPERATORS => {
    equals         => \&_simple_operator,
    notequals      => \&_simple_operator,
    casesubstring  => \&_casesubstring,
    substring      => \&_substring,
    substr         => \&_substring,
    notsubstring   => \&_notsubstring,
    regexp         => \&_regexp,
    notregexp      => \&_notregexp,
    lessthan       => \&_simple_operator,
    lessthaneq     => \&_simple_operator,
    matches        => sub { ThrowUserError("search_content_without_matches"); },
    notmatches     => sub { ThrowUserError("search_content_without_matches"); },
    greaterthan    => \&_simple_operator,
    greaterthaneq  => \&_simple_operator,
    anyexact       => \&_anyexact,
    anywordssubstr => \&_anywordsubstr,
    allwordssubstr => \&_allwordssubstr,
    nowordssubstr  => \&_nowordssubstr,
    anywords       => \&_anywords,
    allwords       => \&_allwords,
    nowords        => \&_nowords,
    changedbefore  => \&_changedbefore_changedafter,
    changedafter   => \&_changedbefore_changedafter,
    changedfrom    => \&_changedfrom_changedto,
    changedto      => \&_changedfrom_changedto,
    changedby      => \&_changedby,
    isempty        => \&_isempty,
    isnotempty     => \&_isnotempty,
};

# Some operators are really just standard SQL operators, and are
# all implemented by the _simple_operator function, which uses this
# constant.
use constant SIMPLE_OPERATORS => {
    equals        => '=',
    notequals     => '!=',
    greaterthan   => '>',
    greaterthaneq => '>=',
    lessthan      => '<',
    lessthaneq    => "<=",
};

# Most operators just reverse by removing or adding "not" from/to them.
# However, some operators reverse in a different way, so those are listed
# here.
use constant OPERATOR_REVERSE => {
    nowords        => 'anywords',
    nowordssubstr  => 'anywordssubstr',
    anywords       => 'nowords',
    anywordssubstr => 'nowordssubstr',
    lessthan       => 'greaterthaneq',
    lessthaneq     => 'greaterthan',
    greaterthan    => 'lessthaneq',
    greaterthaneq  => 'lessthan',
    isempty        => 'isnotempty',
    isnotempty     => 'isempty',
    # The following don't currently have reversals:
    # casesubstring, anyexact, allwords, allwordssubstr
};

# For these operators, even if a field is numeric (is_numeric returns true),
# we won't treat the input like a number.
use constant NON_NUMERIC_OPERATORS => qw(
    changedafter
    changedbefore
    changedfrom
    changedto
    regexp
    notregexp
);

# These operators ignore the entered value
use constant NO_VALUE_OPERATORS => qw(
    isempty
    isnotempty
);

use constant MULTI_SELECT_OVERRIDE => {
    notequals      => \&_multiselect_negative,
    notregexp      => \&_multiselect_negative,
    notsubstring   => \&_multiselect_negative,
    nowords        => \&_multiselect_negative,
    nowordssubstr  => \&_multiselect_negative,
    
    allwords       => \&_multiselect_multiple,
    allwordssubstr => \&_multiselect_multiple,
    anyexact       => \&_multiselect_multiple,
    anywords       => \&_multiselect_multiple,
    anywordssubstr => \&_multiselect_multiple,
    
    _non_changed    => \&_multiselect_nonchanged,
};

use constant OPERATOR_FIELD_OVERRIDE => {
    # User fields
    'attachments.submitter' => {
        _non_changed => \&_user_nonchanged,
    },
    assigned_to => {
        _non_changed => \&_user_nonchanged,
    },
    assigned_to_realname => {
        _non_changed => \&_user_nonchanged,
    },
    cc => {
        _non_changed => \&_user_nonchanged,
    },
    commenter => {
        _non_changed => \&_user_nonchanged,
    },
    reporter => {
        _non_changed => \&_user_nonchanged,
    },
    reporter_realname => {
        _non_changed => \&_user_nonchanged,
    },
    'requestees.login_name' => {
        _non_changed => \&_user_nonchanged,
    },
    'setters.login_name' => {
        _non_changed => \&_user_nonchanged,    
    },
    qa_contact => {
        _non_changed => \&_user_nonchanged,
    },
    qa_contact_realname => {
        _non_changed => \&_user_nonchanged,
    },

    # General Bug Fields
    alias        => { _non_changed => \&_alias_nonchanged },
    'attach_data.thedata' => MULTI_SELECT_OVERRIDE,
    # We check all attachment fields against this.
    attachments  => MULTI_SELECT_OVERRIDE,
    blocked      => MULTI_SELECT_OVERRIDE,
    bug_file_loc => { _non_changed => \&_nullable },
    bug_group    => MULTI_SELECT_OVERRIDE,
    classification => {
        _non_changed => \&_classification_nonchanged,
    },
    component => {
        _non_changed => \&_component_nonchanged,
    },
    content => {
        matches    => \&_content_matches,
        notmatches => \&_content_matches,
        _default   => sub { ThrowUserError("search_content_without_matches"); },
    },
    days_elapsed => {
        _default => \&_days_elapsed,
    },
    dependson        => MULTI_SELECT_OVERRIDE,
    keywords         => MULTI_SELECT_OVERRIDE,
    'flagtypes.name' => {
        _non_changed => \&_flagtypes_nonchanged,
    },
    longdesc => {
        changedby     => \&_long_desc_changedby,
        changedbefore => \&_long_desc_changedbefore_after,
        changedafter  => \&_long_desc_changedbefore_after,
        _non_changed  => \&_long_desc_nonchanged,
    },
    'longdescs.count' => {
        changedby     => \&_long_desc_changedby,
        changedbefore => \&_long_desc_changedbefore_after,
        changedafter  => \&_long_desc_changedbefore_after,
        changedfrom   => \&_invalid_combination,
        changedto     => \&_invalid_combination,
        _default      => \&_long_descs_count,
    },
    'longdescs.isprivate' => MULTI_SELECT_OVERRIDE,
    owner_idle_time => {
        greaterthan   => \&_owner_idle_time_greater_less,
        greaterthaneq => \&_owner_idle_time_greater_less,
        lessthan      => \&_owner_idle_time_greater_less,
        lessthaneq    => \&_owner_idle_time_greater_less,
        _default      => \&_invalid_combination,
    },
    product => {
        _non_changed => \&_product_nonchanged,
    },
    tag => MULTI_SELECT_OVERRIDE,
    comment_tag => MULTI_SELECT_OVERRIDE,

    # Timetracking Fields
    deadline => { _non_changed => \&_deadline },
    percentage_complete => {
        _non_changed => \&_percentage_complete,
    },
    work_time => {
        changedby     => \&_work_time_changedby,
        changedbefore => \&_work_time_changedbefore_after,
        changedafter  => \&_work_time_changedbefore_after,
        _default      => \&_work_time,
    },
    last_visit_ts => {
        _non_changed => \&_last_visit_ts,
        _default     => \&_last_visit_ts_invalid_operator,
    },
    
    # Custom Fields
    FIELD_TYPE_FREETEXT, { _non_changed => \&_nullable },
    FIELD_TYPE_BUG_ID,   { _non_changed => \&_nullable_int },
    FIELD_TYPE_DATETIME, { _non_changed => \&_nullable_datetime },
    FIELD_TYPE_DATE,     { _non_changed => \&_nullable_date },
    FIELD_TYPE_TEXTAREA, { _non_changed => \&_nullable },
    FIELD_TYPE_MULTI_SELECT, MULTI_SELECT_OVERRIDE,
    FIELD_TYPE_BUG_URLS,     MULTI_SELECT_OVERRIDE,    
};

# These are fields where special action is taken depending on the
# *value* passed in to the chart, sometimes.
# This is a sub because custom fields are dynamic
sub SPECIAL_PARSING {
    my $map = {
        # Pronoun Fields (Ones that can accept %user%, etc.)
        assigned_to => \&_contact_pronoun,
        cc          => \&_contact_pronoun,
        commenter   => \&_contact_pronoun,
        qa_contact  => \&_contact_pronoun,
        reporter    => \&_contact_pronoun,
        'setters.login_name' => \&_contact_pronoun,
        'requestees.login_name' => \&_contact_pronoun,

        # Date Fields that accept the 1d, 1w, 1m, 1y, etc. format.
        creation_ts => \&_datetime_translate,
        deadline    => \&_date_translate,
        delta_ts    => \&_datetime_translate,

        # last_visit field that accept both a 1d, 1w, 1m, 1y format and the
        # %last_changed% pronoun.
        last_visit_ts => \&_last_visit_datetime,
    };
    foreach my $field (Bugzilla->active_custom_fields) {
        if ($field->type == FIELD_TYPE_DATETIME) {
            $map->{$field->name} = \&_datetime_translate;
        } elsif ($field->type == FIELD_TYPE_DATE) {
            $map->{$field->name} = \&_date_translate;
        }
    }
    return $map;
};

# Information about fields that represent "users", used by _user_nonchanged.
# There are other user fields than the ones listed here, but those use
# defaults in _user_nonchanged.
use constant USER_FIELDS => {
    'attachments.submitter' => {
        field    => 'submitter_id',
        join     => { table => 'attachments' },
        isprivate => 1,
    },
    cc => {
        field => 'who',
        join  => { table => 'cc' },
    },
    commenter => {
        field => 'who',
        join  => { table => 'longdescs', join => 'INNER' },
        isprivate => 1,
    },
    qa_contact => {
        nullable => 1,
    },
    'requestees.login_name' => {
        nullable => 1,
        field    => 'requestee_id',
        join     => { table => 'flags' },
    },
    'setters.login_name' => {
        field    => 'setter_id',
        join     => { table => 'flags' },
    },
};

# Backwards compatibility for times that we changed the names of fields
# or URL parameters.
use constant FIELD_MAP => {
    'attachments.thedata' => 'attach_data.thedata',
    bugidtype => 'bug_id_type',
    changedin => 'days_elapsed',
    long_desc => 'longdesc',
    tags      => 'tag',
};

# Some fields are not sorted on themselves, but on other fields.
# We need to have a list of these fields and what they map to.
use constant SPECIAL_ORDER => {
    'target_milestone' => {
        order => ['map_target_milestone.sortkey','map_target_milestone.value'],
        join  => {
            table => 'milestones',
            from  => 'target_milestone',
            to    => 'value',
            extra => ['bugs.product_id = map_target_milestone.product_id'],
            join  => 'INNER',
        }
    },
};

# Certain columns require other columns to come before them
# in _select_columns, and should be put there if they're not there.
use constant COLUMN_DEPENDS => {
    classification      => ['product'],
    percentage_complete => ['actual_time', 'remaining_time'],
};

# This describes tables that must be joined when you want to display
# certain columns in the buglist. For the most part, Search.pm uses
# DB::Schema to figure out what needs to be joined, but for some
# fields it needs a little help.
sub COLUMN_JOINS {
    my $invocant = shift;
    my $user = blessed($invocant) ? $invocant->_user : Bugzilla->user;

    my $joins = {
        actual_time => {
            table => '(SELECT bug_id, SUM(work_time) AS total'
                     . ' FROM longdescs GROUP BY bug_id)',
            join  => 'INNER',
        },
        alias => {
            table => 'bugs_aliases',
            as => 'map_alias',
        },
        assigned_to => {
            from  => 'assigned_to',
            to    => 'userid',
            table => 'profiles',
            join  => 'INNER',
        },
        reporter => {
            from  => 'reporter',
            to    => 'userid',
            table => 'profiles',
            join  => 'INNER',
        },
        qa_contact => {
            from  => 'qa_contact',
            to    => 'userid',
            table => 'profiles',
        },
        component => {
            from  => 'component_id',
            to    => 'id',
            table => 'components',
            join  => 'INNER',
        },
        product => {
            from  => 'product_id',
            to    => 'id',
            table => 'products',
            join  => 'INNER',
        },
        classification => {
            table => 'classifications',
            from  => 'map_product.classification_id',
            to    => 'id',
            join  => 'INNER',
        },
        'flagtypes.name' => {
            as    => 'map_flags',
            table => 'flags',
            extra => ['map_flags.attach_id IS NULL'],
            then_to => {
                as    => 'map_flagtypes',
                table => 'flagtypes',
                from  => 'map_flags.type_id',
                to    => 'id',
            },
        },
        keywords => {
            table => 'keywords',
            then_to => {
                as    => 'map_keyworddefs',
                table => 'keyworddefs',
                from  => 'map_keywords.keywordid',
                to    => 'id',
            },
        },
        blocked => {
            table => 'dependencies',
            to => 'dependson',
        },
        dependson => {
            table => 'dependencies',
            to => 'blocked',
        },
        'longdescs.count' => {
            table => 'longdescs',
            join  => 'INNER',
        },
        tag => {
            as => 'map_bug_tag',
            table => 'bug_tag',
            then_to => {
                as => 'map_tag',
                table => 'tag',
                extra => ['map_tag.user_id = ' . $user->id],
                from => 'map_bug_tag.tag_id',
                to => 'id',
            },
        },
        last_visit_ts => {
            as    => 'bug_user_last_visit',
            table => 'bug_user_last_visit',
            extra => ['bug_user_last_visit.user_id = ' . $user->id],
            from  => 'bug_id',
            to    => 'bug_id',
        },
    };
    return $joins;
};

# This constant defines the columns that can be selected in a query 
# and/or displayed in a bug list.  Column records include the following
# fields:
#
# 1. id: a unique identifier by which the column is referred in code;
#
# 2. name: The name of the column in the database (may also be an expression
#          that returns the value of the column);
#
# 3. title: The title of the column as displayed to users.
# 
# Note: There are a few hacks in the code that deviate from these definitions.
#       In particular, the redundant short_desc column is removed when the
#       client requests "all" columns.
#
# This is really a constant--that is, once it's been called once, the value
# will always be the same unless somebody adds a new custom field. But
# we have to do a lot of work inside the subroutine to get the data,
# and we don't want it to happen at compile time, so we have it as a
# subroutine.
sub COLUMNS {
    my $invocant = shift;
    my $user = blessed($invocant) ? $invocant->_user : Bugzilla->user;
    my $dbh = Bugzilla->dbh;
    my $cache = Bugzilla->request_cache;

    if (defined $cache->{search_columns}->{$user->id}) {
        return $cache->{search_columns}->{$user->id};
    }

    # These are columns that don't exist in fielddefs, but are valid buglist
    # columns. (Also see near the bottom of this function for the definition
    # of short_short_desc.)
    my %columns = (
        relevance            => { title => 'Relevance'  },
    );

    # Next we define columns that have special SQL instead of just something
    # like "bugs.bug_id".
    my $total_time = "(map_actual_time.total + bugs.remaining_time)";
    my %special_sql = (
        alias       => $dbh->sql_group_concat('DISTINCT map_alias.alias'),
        deadline    => $dbh->sql_date_format('bugs.deadline', '%Y-%m-%d'),
        actual_time => 'map_actual_time.total',

        # "FLOOR" is in there to turn this into an integer, making searches
        # totally predictable. Otherwise you get floating-point numbers that
        # are rather hard to search reliably if you're asking for exact
        # numbers.
        percentage_complete =>
            "(CASE WHEN $total_time = 0"
               . " THEN 0"
               . " ELSE FLOOR(100 * (map_actual_time.total / $total_time))"
                . " END)",

        'flagtypes.name' => $dbh->sql_group_concat('DISTINCT ' 
            . $dbh->sql_string_concat('map_flagtypes.name', 'map_flags.status'),
            undef, undef, 'map_flagtypes.sortkey, map_flagtypes.name'),

        'keywords' => $dbh->sql_group_concat('DISTINCT map_keyworddefs.name'),

        blocked => $dbh->sql_group_concat('DISTINCT map_blocked.blocked'),
        dependson => $dbh->sql_group_concat('DISTINCT map_dependson.dependson'),
        
        'longdescs.count' => 'COUNT(DISTINCT map_longdescs_count.comment_id)',

        tag => $dbh->sql_group_concat('DISTINCT map_tag.name'),
        last_visit_ts => 'bug_user_last_visit.last_visit_ts',
    );

    # Backward-compatibility for old field names. Goes new_name => old_name.
    # These are here and not in _translate_old_column because the rest of the
    # code actually still uses the old names, while the fielddefs table uses
    # the new names (which is not the case for the fields handled by
    # _translate_old_column).
    my %old_names = (
        creation_ts => 'opendate',
        delta_ts    => 'changeddate',
        work_time   => 'actual_time',
    );

    # Fields that are email addresses
    my @email_fields = qw(assigned_to reporter qa_contact);
    # Other fields that are stored in the bugs table as an id, but
    # should be displayed using their name.
    my @id_fields = qw(product component classification);

    foreach my $col (@email_fields) {
        my $sql = "map_${col}.login_name";
        if (!$user->id) {
             $sql = $dbh->sql_string_until($sql, $dbh->quote('@'));
        }
        $special_sql{$col} = $sql;
        $special_sql{"${col}_realname"} = "map_${col}.realname";
    }

    foreach my $col (@id_fields) {
        $special_sql{$col} = "map_${col}.name";
    }

    # Do the actual column-getting from fielddefs, now.
    my @fields = @{ Bugzilla->fields({ obsolete => 0, buglist => 1 }) };
    foreach my $field (@fields) {
        my $id = $field->name;
        $id = $old_names{$id} if exists $old_names{$id};
        my $sql;
        if (exists $special_sql{$id}) {
            $sql = $special_sql{$id};
        }
        elsif ($field->type == FIELD_TYPE_MULTI_SELECT) {
            $sql = $dbh->sql_group_concat(
                'DISTINCT map_' . $field->name . '.value');
        }
        else {
            $sql = 'bugs.' . $field->name;
        }
        $columns{$id} = { name => $sql, title => $field->description };
    }

    # The short_short_desc column is identical to short_desc
    $columns{'short_short_desc'} = $columns{'short_desc'};

    Bugzilla::Hook::process('buglist_columns', { columns => \%columns });

    $cache->{search_columns}->{$user->id} = \%columns;
    return $cache->{search_columns}->{$user->id};
}

sub REPORT_COLUMNS {
    my $invocant = shift;
    my $user = blessed($invocant) ? $invocant->_user : Bugzilla->user;

    my $columns = dclone(blessed($invocant) ? $invocant->COLUMNS : COLUMNS);
    # There's no reason to support reporting on unique fields.
    # Also, some other fields don't make very good reporting axises,
    # or simply don't work with the current reporting system.
    my @no_report_columns = 
        qw(bug_id alias short_short_desc opendate changeddate
           flagtypes.name relevance);

    # If you're not a time-tracker, you can't use time-tracking
    # columns.
    if (!$user->is_timetracker) {
        push(@no_report_columns, TIMETRACKING_FIELDS);
    }

    foreach my $name (@no_report_columns) {
        delete $columns->{$name};
    }
    return $columns;
}

# These are fields that never go into the GROUP BY on any DB. bug_id
# is here because it *always* goes into the GROUP BY as the first item,
# so it should be skipped when determining extra GROUP BY columns.
use constant GROUP_BY_SKIP => qw(
    alias
    blocked
    bug_id
    dependson
    flagtypes.name
    keywords
    longdescs.count
    percentage_complete
    tag
);

###############
# Constructor #
###############

# Note that the params argument may be modified by Bugzilla::Search
sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
  
    my $self = { @_ };
    bless($self, $class);
    $self->{'user'} ||= Bugzilla->user;
    
    # There are certain behaviors of the CGI "Vars" hash that we don't want.
    # In particular, if you put a single-value arrayref into it, later you
    # get back out a string, which breaks anyexact charts (because they
    # need arrays even for individual items, or we will re-trigger bug 67036).
    #
    # We can't just untie the hash--that would give us a hash with no values.
    # We have to manually copy the hash into a new one, and we have to always
    # do it, because there's no way to know if we were passed a tied hash
    # or not.
    my $params_in = $self->_params;
    my %params = map { $_ => $params_in->{$_} } keys %$params_in;
    $self->{params} = \%params;

    return $self;
}


####################
# Public Accessors #
####################

sub data {
    my $self = shift;
    return $self->{data} if $self->{data};
    my $dbh = Bugzilla->dbh;

    # If all fields belong to the 'bugs' table, there is no need to split
    # the original query into two pieces. Else we override the 'fields'
    # argument to first get bug IDs based on the search criteria defined
    # by the caller, and the desired fields are collected in the 2nd query.
    my @orig_fields = $self->_input_columns;
    my $all_in_bugs_table = 1;
    foreach my $field (@orig_fields) {
        next if ($self->COLUMNS->{$field}->{name} // $field) =~ /^bugs\.\w+$/;
        $self->{fields} = ['bug_id'];
        $all_in_bugs_table = 0;
        last;
    }

    my $start_time = [gettimeofday()];
    my $sql = $self->_sql;
    # Do we just want bug IDs to pass to the 2nd query or all the data immediately?
    my $func = $all_in_bugs_table ? 'selectall_arrayref' : 'selectcol_arrayref';
    my $bug_ids = $dbh->$func($sql);
    my @extra_data = ({sql => $sql, time => tv_interval($start_time)});
    # Restore the original 'fields' argument, just in case.
    $self->{fields} = \@orig_fields unless $all_in_bugs_table;

    # If there are no bugs found, or all fields are in the 'bugs' table,
    # there is no need for another query.
    if (!scalar @$bug_ids || $all_in_bugs_table) {
        $self->{data} = $bug_ids;
        return wantarray ? ($self->{data}, \@extra_data) : $self->{data};
    }

    # Make sure the bug_id will be returned. If not, append it to the list.
    my $pos = firstidx { $_ eq 'bug_id' } @orig_fields;
    if ($pos < 0) {
        push(@orig_fields, 'bug_id');
        $pos = $#orig_fields;
    }

    # Now create a query with the buglist above as the single criteria
    # and the fields that the caller wants. No need to redo security checks;
    # the list has already been validated above.
    my $search = $self->new('fields' => \@orig_fields,
                            'params' => {bug_id => $bug_ids, bug_id_type => 'anyexact'},
                            'sharer' => $self->_sharer_id,
                            'user'   => $self->_user,
                            'allow_unlimited'    => 1,
                            '_no_security_check' => 1);

    $start_time = [gettimeofday()];
    $sql = $search->_sql;
    my $unsorted_data = $dbh->selectall_arrayref($sql);
    push(@extra_data, {sql => $sql, time => tv_interval($start_time)});
    # Let's sort the data. We didn't do it in the query itself because
    # we already know in which order to sort bugs thanks to the first query,
    # and this avoids additional table joins in the SQL query.
    my %data = map { $_->[$pos] => $_ } @$unsorted_data;
    $self->{data} = [map { $data{$_} } @$bug_ids];
    return wantarray ? ($self->{data}, \@extra_data) : $self->{data};
}

sub _sql {
    my ($self) = @_;
    return $self->{sql} if $self->{sql};
    my $dbh = Bugzilla->dbh;

    my ($joins, $clause) = $self->_charts_to_conditions();

    if (!$clause->as_string
        && !Bugzilla->params->{'search_allow_no_criteria'}
        && !$self->{allow_unlimited})
    {
        ThrowUserError('buglist_parameters_required');
    }

    my $select = join(', ', $self->_sql_select);
    my $from = $self->_sql_from($joins);
    my $where = $self->_sql_where($clause);
    my $group_by = $dbh->sql_group_by($self->_sql_group_by);
    my $order_by = $self->_sql_order_by
                   ? "\nORDER BY " . join(', ', $self->_sql_order_by) : '';
    my $limit = $self->_sql_limit;
    $limit = "\n$limit" if $limit;
    
    my $query = <<END;
SELECT $select
  FROM $from
 WHERE $where
$group_by$order_by$limit
END
    $self->{sql} = $query;
    return $self->{sql};
}

sub search_description {
    my ($self, $params) = @_;
    my $desc = $self->{'search_description'} ||= [];
    if ($params) {
        push(@$desc, $params);
    }
    # Make sure that the description has actually been generated if
    # people are asking for the whole thing.
    else {
        $self->_sql;
    }
    return $self->{'search_description'};
}

sub boolean_charts_to_custom_search {
    my ($self, $cgi_buffer) = @_;
    my $boolean_charts = $self->_boolean_charts;
    my @as_params = $boolean_charts ? $boolean_charts->as_params : ();

    # We need to start our new ids after the last custom search "f" id.
    # We can just pick the last id in the array because they are sorted
    # numerically.
    my $last_id = ($self->_field_ids)[-1];
    my $count = defined($last_id) ? $last_id + 1 : 0;
    foreach my $param_set (@as_params) {
        foreach my $name (keys %$param_set) {
            my $value = $param_set->{$name};
            next if !defined $value;
            $cgi_buffer->param($name . $count, $value);
        }
        $count++;
    }
}

sub invalid_order_columns {
   my ($self) = @_;
   my @invalid_columns;
   foreach my $order ($self->_input_order) {
       next if defined $self->_validate_order_column($order);
       push(@invalid_columns, $order);
   }
   return \@invalid_columns;
}

sub order {
   my ($self) = @_;
   return $self->_valid_order;
}

######################
# Internal Accessors #
######################

# Fields that are legal for boolean charts of any kind.
sub _chart_fields {
    my ($self) = @_;

    if (!$self->{chart_fields}) {
        my $chart_fields = Bugzilla->fields({ by_name => 1 });

        if (!$self->_user->is_timetracker) {
            foreach my $tt_field (TIMETRACKING_FIELDS) {
                delete $chart_fields->{$tt_field};
            }
        }
        $self->{chart_fields} = $chart_fields;
    }
    return $self->{chart_fields};
}

# There are various places in Search.pm that we need to know the list of
# valid multi-select fields--or really, fields that are stored like
# multi-selects, which includes BUG_URLS fields.
sub _multi_select_fields {
    my ($self) = @_;
    $self->{multi_select_fields} ||= Bugzilla->fields({
        by_name => 1,
        type    => [FIELD_TYPE_MULTI_SELECT, FIELD_TYPE_BUG_URLS]});
    return $self->{multi_select_fields};
}

# $self->{params} contains values that could be undef, could be a string,
# or could be an arrayref. Sometimes we want that value as an array,
# always.
sub _param_array {
    my ($self, $name) = @_;
    my $value = $self->_params->{$name};
    if (!defined $value) {
        return ();
    }
    if (ref($value) eq 'ARRAY') {
        return @$value;
    }
    return ($value);
}

sub _params { $_[0]->{params} }
sub _user { return $_[0]->{user} }
sub _sharer_id { $_[0]->{sharer} }

##############################
# Internal Accessors: SELECT #
##############################

# These are the fields the user has chosen to display on the buglist,
# exactly as they were passed to new().
sub _input_columns { @{ $_[0]->{'fields'} || [] } }

# These are columns that are also going to be in the SELECT for one reason
# or another, but weren't actually requested by the caller.
sub _extra_columns {
    my ($self) = @_;
    # Everything that's going to be in the ORDER BY must also be
    # in the SELECT.
    push(@{ $self->{extra_columns} }, $self->_valid_order_columns);
    return @{ $self->{extra_columns} };
}

# For search functions to modify extra_columns. It doesn't matter if
# people push the same column onto this array multiple times, because
# _select_columns will call "uniq" on its final result.
sub _add_extra_column {
    my ($self, $column) = @_;
    push(@{ $self->{extra_columns} }, $column);
}

# These are the columns that we're going to be actually SELECTing.
sub _display_columns {
    my ($self) = @_;
    return @{ $self->{display_columns} } if $self->{display_columns};

    # Do not alter the list from _input_columns at all, even if there are
    # duplicated columns. Those are passed by the caller, and the caller
    # expects to get them back in the exact same order.
    my @columns = $self->_input_columns;

    # Only add columns which are not already listed.
    my %list = map { $_ => 1 } @columns;
    foreach my $column ($self->_extra_columns) {
        push(@columns, $column) unless $list{$column}++;
    }
    $self->{display_columns} = \@columns;
    return @{ $self->{display_columns} };
}

# These are the columns that are involved in the query.
sub _select_columns {
    my ($self) = @_;
    return @{ $self->{select_columns} } if $self->{select_columns};

    my @select_columns;
    foreach my $column ($self->_display_columns) {
        if (my $add_first = COLUMN_DEPENDS->{$column}) {
            push(@select_columns, @$add_first);
        }
        push(@select_columns, $column);
    }
    # Remove duplicated columns.
    $self->{select_columns} = [uniq @select_columns];
    return @{ $self->{select_columns} };
}

# This takes _display_columns and translates it into the actual SQL that
# will go into the SELECT clause.
sub _sql_select {
    my ($self) = @_;
    my @sql_fields;
    foreach my $column ($self->_display_columns) {
        my $sql = $self->COLUMNS->{$column}->{name} // '';
        if ($sql) {
            my $alias = $column;
            # Aliases cannot contain dots in them. We convert them to underscores.
            $alias =~ tr/./_/;
            $sql .= " AS $alias";
        }
        else {
            $sql = $column;
        }
        push(@sql_fields, $sql);
    }
    return @sql_fields;
}

################################
# Internal Accessors: ORDER BY #
################################

# The "order" that was requested by the consumer, exactly as it was
# requested.
sub _input_order { @{ $_[0]->{'order'} || [] } }
# Requested order with invalid values removed and old names translated
sub _valid_order {
    my ($self) = @_;
    return map { ($self->_validate_order_column($_)) } $self->_input_order;
}
# The valid order with just the column names, and no ASC or DESC.
sub _valid_order_columns {
    my ($self) = @_;
    return map { (split_order_term($_))[0] } $self->_valid_order;
}

sub _validate_order_column {
    my ($self, $order_item) = @_;

    # Translate old column names
    my ($field, $direction) = split_order_term($order_item);
    $field = $self->_translate_old_column($field);

    # Only accept valid columns
    return if (!exists $self->COLUMNS->{$field});

    # Relevance column can be used only with one or more fulltext searches
    return if ($field eq 'relevance' && !$self->COLUMNS->{$field}->{name});

    $direction = " $direction" if $direction;
    return "$field$direction";
}

# A hashref that describes all the special stuff that has to be done
# for various fields if they go into the ORDER BY clause.
sub _special_order {
    my ($self) = @_;
    return $self->{special_order} if $self->{special_order};
    
    my %special_order = %{ SPECIAL_ORDER() };
    my $select_fields = Bugzilla->fields({ type => FIELD_TYPE_SINGLE_SELECT });
    foreach my $field (@$select_fields) {
        next if $field->is_abnormal;
        my $name = $field->name;
        $special_order{$name} = {
            order => ["map_$name.sortkey", "map_$name.value"],
            join  => {
                table => $name,
                from  => "bugs.$name",
                to    => "value",
                join  => 'INNER',
            }
        };
    }
    $self->{special_order} = \%special_order;
    return $self->{special_order};
}

sub _sql_order_by {
    my ($self) = @_;
    if (!$self->{sql_order_by}) {
        my @order_by = map { $self->_translate_order_by_column($_) }
                           $self->_valid_order;
        $self->{sql_order_by} = \@order_by;
    }
    return @{ $self->{sql_order_by} };
}

sub _translate_order_by_column {
    my ($self, $order_by_item) = @_;

    my ($field, $direction) = split_order_term($order_by_item);
    
    $direction = '' if lc($direction) eq 'asc';
    my $special_order = $self->_special_order->{$field}->{order};
    # Standard fields have underscores in their SELECT alias instead
    # of a period (because aliases can't have periods).
    $field =~ s/\./_/g;
    my @items = $special_order ? @$special_order : $field;
    if (lc($direction) eq 'desc') {
        @items = map { "$_ DESC" } @items;
    }
    return @items;
}

#############################
# Internal Accessors: LIMIT #
#############################

sub _sql_limit {
    my ($self) = @_;
    my $limit = $self->_params->{limit};
    my $offset = $self->_params->{offset};
    
    my $max_results = Bugzilla->params->{'max_search_results'};
    if (!$self->{allow_unlimited} && (!$limit || $limit > $max_results)) {
        $limit = $max_results;
    }
    
    if (defined($offset) && !$limit) {
        $limit = INT_MAX;
    }
    if (defined $limit) {
        detaint_natural($limit) 
            || ThrowCodeError('param_must_be_numeric', 
                              { function => 'Bugzilla::Search::new',
                                param    => 'limit' });
        if (defined $offset) {
            detaint_natural($offset)
                || ThrowCodeError('param_must_be_numeric',
                                  { function => 'Bugzilla::Search::new',
                                    param    => 'offset' });
        }
        return Bugzilla->dbh->sql_limit($limit, $offset);
    }
    return '';
}

############################
# Internal Accessors: FROM #
############################

sub _column_join {
    my ($self, $field) = @_;
    # The _realname fields require the same join as the username fields.
    $field =~ s/_realname$//;
    my $column_joins = $self->_get_column_joins();
    my $join_info = $column_joins->{$field};
    if ($join_info) {
        # Don't allow callers to modify the constant.
        $join_info = dclone($join_info);
    }
    else {
        if ($self->_multi_select_fields->{$field}) {
            $join_info = { table => "bug_$field" };
        }
    }
    if ($join_info and !$join_info->{as}) {
        $join_info = dclone($join_info);
        $join_info->{as} = "map_$field";
    }
    return $join_info ? $join_info : ();
}

# Sometimes we join the same table more than once. In this case, we
# want to AND all the various critiera that were used in both joins.
sub _combine_joins {
    my ($self, $joins) = @_;
    my @result;
    while(my $join = shift @$joins) {
        my $name = $join->{as};
        my ($others_like_me, $the_rest) = part { $_->{as} eq $name ? 0 : 1 }
                                               @$joins;
        if ($others_like_me) {
            my $from = $join->{from};
            my $to   = $join->{to};
            # Sanity check to make sure that we have the same from and to
            # for all the same-named joins.
            if ($from) {
                all { $_->{from} eq $from } @$others_like_me
                  or die "Not all same-named joins have identical 'from': "
                         . Dumper($join, $others_like_me);
            }
            if ($to) {
                all { $_->{to} eq $to } @$others_like_me
                  or die "Not all same-named joins have identical 'to': "
                         . Dumper($join, $others_like_me);
            }
            
            # We don't need to call uniq here--translate_join will do that
            # for us.
            my @conditions = map { @{ $_->{extra} || [] } }
                                 ($join, @$others_like_me);
            $join->{extra} = \@conditions;
            $joins = $the_rest;
        }
        push(@result, $join);
    }
    
    return @result;
}

# Takes all the "then_to" items and just puts them as the next item in
# the array. Right now this only does one level of "then_to", but we
# could re-write this to handle then_to recursively if we need more levels.
sub _extract_then_to {
    my ($self, $joins) = @_;
    my @result;
    foreach my $join (@$joins) {
        push(@result, $join);
        if (my $then_to = $join->{then_to}) {
            push(@result, $then_to);
        }
    }
    return @result;
}

# JOIN statements for the SELECT and ORDER BY columns. This should not be
# called until the moment it is needed, because _select_columns might be
# modified by the charts.
sub _select_order_joins {
    my ($self) = @_;
    my @joins;
    foreach my $field ($self->_select_columns) {
        my @column_join = $self->_column_join($field);
        push(@joins, @column_join);
    }
    foreach my $field ($self->_valid_order_columns) {
        my $join_info = $self->_special_order->{$field}->{join};
        if ($join_info) {
            # Don't let callers modify SPECIAL_ORDER.
            $join_info = dclone($join_info);
            if (!$join_info->{as}) {
                $join_info->{as} = "map_$field";
            }
            push(@joins, $join_info);
        }
    }
    return @joins;
}

# These are the joins that are *always* in the FROM clause.
sub _standard_joins {
    my ($self) = @_;
    my $user = $self->_user;
    my @joins;
    return () if $self->{_no_security_check};

    my $security_join = {
        table => 'bug_group_map',
        as    => 'security_map',
    };
    push(@joins, $security_join);

    if ($user->id) {
        # See also _standard_joins for the other half of the below statement
        if (!Bugzilla->params->{'or_groups'}) {
            $security_join->{extra} =
                ["NOT (" . $user->groups_in_sql('security_map.group_id') . ")"];
        }

        my $security_cc_join = {
            table => 'cc',
            as    => 'security_cc',
            extra => ['security_cc.who = ' . $user->id],
        };
        push(@joins, $security_cc_join);
    }
    
    return @joins;
}

sub _sql_from {
    my ($self, $joins_input) = @_;
    my @joins = ($self->_standard_joins, $self->_select_order_joins,
                 @$joins_input);
    @joins = $self->_extract_then_to(\@joins);
    @joins = $self->_combine_joins(\@joins);
    my @join_sql = map { $self->_translate_join($_) } @joins;
    return "bugs\n" . join("\n", @join_sql);
}

# This takes a join data structure and turns it into actual JOIN SQL.
sub _translate_join {
    my ($self, $join_info) = @_;
    
    die "join with no table: " . Dumper($join_info) if !$join_info->{table};
    die "join with no 'as': " . Dumper($join_info) if !$join_info->{as};

    my $from_table = $join_info->{bugs_table} || "bugs";
    my $from  = $join_info->{from} || "bug_id";
    if ($from =~ /^(\w+)\.(\w+)$/) {
        ($from_table, $from) = ($1, $2);
    }
    my $table = $join_info->{table};
    my $name  = $join_info->{as};
    my $to    = $join_info->{to}    || "bug_id";
    my $join  = $join_info->{join}  || 'LEFT';
    my @extra = @{ $join_info->{extra} || [] };
    $name =~ s/\./_/g;
    
    # If a term contains ORs, we need to put parens around the condition.
    # This is a pretty weak test, but it's actually OK to put parens
    # around too many things.
    @extra = map { $_ =~ /\bOR\b/i ? "($_)" : $_ } @extra;
    my $extra_condition = join(' AND ', uniq @extra);
    if ($extra_condition) {
        $extra_condition = " AND $extra_condition";
    }

    my @join_sql = "$join JOIN $table AS $name"
                        . " ON $from_table.$from = $name.$to$extra_condition";
    return @join_sql;
}

#############################
# Internal Accessors: WHERE #
#############################

# Note: There's also quite a bit of stuff that affects the WHERE clause
# in the "Internal Accessors: Boolean Charts" section.

# The terms that are always in the WHERE clause. These implement bug
# group security.
sub _standard_where {
    my ($self) = @_;
    return ('1=1') if $self->{_no_security_check};
    # If replication lags badly between the shadow db and the main DB,
    # it's possible for bugs to show up in searches before their group
    # controls are properly set. To prevent this, when initially creating
    # bugs we set their creation_ts to NULL, and don't give them a creation_ts
    # until their group controls are set. So if a bug has a NULL creation_ts,
    # it shouldn't show up in searches at all.
    my @where = ('bugs.creation_ts IS NOT NULL');

    my $user = $self->_user;
    my $security_term = '';
    # See also _standard_joins for the other half of the below statement
    if (Bugzilla->params->{'or_groups'}) {
        $security_term .= " (security_map.group_id IS NULL OR security_map.group_id IN (" . $user->groups_as_string . "))";
    }
    else {
        $security_term = 'security_map.group_id IS NULL';
    }

    if ($user->id) {
        my $userid = $user->id;
        # This indentation makes the resulting SQL more readable.
        $security_term .= <<END;

        OR (bugs.reporter_accessible = 1 AND bugs.reporter = $userid)
        OR (bugs.cclist_accessible = 1 AND security_cc.who IS NOT NULL)
        OR bugs.assigned_to = $userid
END
        if (Bugzilla->params->{'useqacontact'}) {
            $security_term.= "        OR bugs.qa_contact = $userid";
        }
        $security_term = "($security_term)";
    }

    push(@where, $security_term);

    return @where;
}

sub _sql_where {
    my ($self, $main_clause) = @_;
    # The newline and this particular spacing makes the resulting
    # SQL a bit more readable for debugging.
    my $where = join("\n   AND ", $self->_standard_where);
    my $clause_sql = $main_clause->as_string;
    $where .= "\n   AND " . $clause_sql if $clause_sql;
    return $where;
}

################################
# Internal Accessors: GROUP BY #
################################

# And these are the fields that we have to do GROUP BY for in DBs
# that are more strict about putting everything into GROUP BY.
sub _sql_group_by {
    my ($self) = @_;

    # Strict DBs require every element from the SELECT to be in the GROUP BY,
    # unless that element is being used in an aggregate function.
    my @extra_group_by;
    foreach my $column ($self->_select_columns) {
        next if $self->_skip_group_by->{$column};
        my $sql = $self->COLUMNS->{$column}->{name} // $column;
        push(@extra_group_by, $sql);
    }

    # And all items from ORDER BY must be in the GROUP BY. The above loop 
    # doesn't catch items that were put into the ORDER BY from SPECIAL_ORDER.
    foreach my $column ($self->_valid_order_columns) {
        my $special_order = $self->_special_order->{$column}->{order};
        next if !$special_order;
        push(@extra_group_by, @$special_order);
    }
    
    @extra_group_by = uniq @extra_group_by;
    
    # bug_id is the only field we actually group by.
    return ('bugs.bug_id', join(',', @extra_group_by));
}

# A helper for _sql_group_by.
sub _skip_group_by {
    my ($self) = @_;
    return $self->{skip_group_by} if $self->{skip_group_by};
    my @skip_list = GROUP_BY_SKIP;
    push(@skip_list, keys %{ $self->_multi_select_fields });
    my %skip_hash = map { $_ => 1 } @skip_list;
    $self->{skip_group_by} = \%skip_hash;
    return $self->{skip_group_by};
}

##############################################
# Internal Accessors: Special Params Parsing #
##############################################

# Backwards compatibility for old field names.
sub _convert_old_params {
    my ($self) = @_;
    my $params = $self->_params;
    
    # bugidtype has different values in modern Search.pm.
    if (defined $params->{'bugidtype'}) {
        my $value = $params->{'bugidtype'};
        $params->{'bugidtype'} = $value eq 'exclude' ? 'nowords' : 'anyexact';
    }
    
    foreach my $old_name (keys %{ FIELD_MAP() }) {
        if (defined $params->{$old_name}) {
            my $new_name = FIELD_MAP->{$old_name};
            $params->{$new_name} = delete $params->{$old_name};
        }
    }
}

# This parses all the standard search parameters except for the boolean
# charts.
sub _special_charts {
    my ($self) = @_;
    $self->_convert_old_params();
    $self->_special_parse_bug_status();
    $self->_special_parse_resolution();
    my $clause = new Bugzilla::Search::Clause();
    $clause->add( $self->_parse_basic_fields()     );
    $clause->add( $self->_special_parse_email()    );
    $clause->add( $self->_special_parse_chfield()  );
    $clause->add( $self->_special_parse_deadline() );
    return $clause;
}

sub _parse_basic_fields {
    my ($self) = @_;
    my $params = $self->_params;
    my $chart_fields = $self->_chart_fields;
    
    my $clause = new Bugzilla::Search::Clause();
    foreach my $field_name (keys %$chart_fields) {
        # CGI params shouldn't have periods in them, so we only accept
        # period-separated fields with underscores where the periods go.
        my $param_name = $field_name;
        $param_name =~ s/\./_/g;
        my @values = $self->_param_array($param_name);
        next if !@values;
        my $default_op = $param_name eq 'content' ? 'matches' : 'anyexact';
        my $operator = $params->{"${param_name}_type"} || $default_op;
        # Fields that are displayed as multi-selects are passed as arrays,
        # so that they can properly search values that contain commas.
        # However, other fields are sent as strings, so that they are properly
        # split on commas if required.
        my $field = $chart_fields->{$field_name};
        my $pass_value;
        if ($field->is_select or $field->name eq 'version'
            or $field->name eq 'target_milestone')
        {
            $pass_value = \@values;
        }
        else {
            $pass_value = join(',', @values);
        }
        $clause->add($field_name, $operator, $pass_value);
    }
    return @{$clause->children} ? $clause : undef;
}

sub _special_parse_bug_status {
    my ($self) = @_;
    my $params = $self->_params;
    return if !defined $params->{'bug_status'};
    # We want to allow the bug_status_type parameter to work normally,
    # meaning that this special code should only be activated if we are
    # doing the normal "anyexact" search on bug_status.
    return if (defined $params->{'bug_status_type'}
               and $params->{'bug_status_type'} ne 'anyexact');

    my @bug_status = $self->_param_array('bug_status');
    # Also include inactive bug statuses, as you can query them.
    my $legal_statuses = $self->_chart_fields->{'bug_status'}->legal_values;

    # If the status contains __open__ or __closed__, translate those
    # into their equivalent lists of open and closed statuses.
    if (grep { $_ eq '__open__' } @bug_status) {
        my @open = grep { $_->is_open } @$legal_statuses;
        @open = map { $_->name } @open;
        push(@bug_status, @open);
    }
    if (grep { $_ eq '__closed__' } @bug_status) {
        my @closed = grep { not $_->is_open } @$legal_statuses;
        @closed = map { $_->name } @closed;
        push(@bug_status, @closed);
    }

    @bug_status = uniq @bug_status;
    my $all = grep { $_ eq "__all__" } @bug_status;
    # This will also handle removing __open__ and __closed__ for us
    # (__all__ too, which is why we check for it above, first).
    @bug_status = _valid_values(\@bug_status, $legal_statuses);

    # If the user has selected every status, change to selecting none.
    # This is functionally equivalent, but quite a lot faster.
    if ($all or scalar(@bug_status) == scalar(@$legal_statuses)) {
        delete $params->{'bug_status'};
    }
    else {
        $params->{'bug_status'} = \@bug_status;
    }
}

sub _special_parse_chfield {
    my ($self) = @_;
    my $params = $self->_params;
    
    my $date_from = trim(lc($params->{'chfieldfrom'} || ''));
    my $date_to = trim(lc($params->{'chfieldto'} || ''));
    $date_from = '' if $date_from eq 'now';
    $date_to = '' if $date_to eq 'now';
    my @fields = $self->_param_array('chfield');
    my $value_to = $params->{'chfieldvalue'};
    $value_to = '' if !defined $value_to;

    @fields = map { $_ eq '[Bug creation]' ? 'creation_ts' : $_ } @fields;

    return undef unless ($date_from ne '' || $date_to ne '' || $value_to ne '');

    my $clause = new Bugzilla::Search::Clause();

    # It is always safe and useful to push delta_ts into the charts
    # if there is a "from" date specified. It doesn't conflict with
    # searching [Bug creation], because a bug's delta_ts is set to
    # its creation_ts when it is created. So this just gives the
    # database an additional index to possibly choose, on a table that
    # is smaller than bugs_activity.
    if ($date_from ne '') {
        $clause->add('delta_ts', 'greaterthaneq', $date_from);
    }
    # It's not normally safe to do it for "to" dates, though--"chfieldto" means
    # "a field that changed before this date", and delta_ts could be either
    # later or earlier than that, if we're searching for the time that a field
    # changed. However, chfieldto all by itself, without any chfieldvalue or
    # chfield, means "just search delta_ts", and so we still want that to
    # work.
    if ($date_to ne '' and !@fields and $value_to eq '') {
        $clause->add('delta_ts', 'lessthaneq', $date_to);
    }

    # chfieldto is supposed to be a relative date or a date of the form
    # YYYY-MM-DD, i.e. without the time appended to it. We append the
    # time ourselves so that the end date is correctly taken into account.
    $date_to .= ' 23:59:59' if $date_to =~ /^\d{4}-\d{1,2}-\d{1,2}$/;

    my $join_clause = new Bugzilla::Search::Clause('OR');

    foreach my $field (@fields) {
        my $sub_clause = new Bugzilla::Search::ClauseGroup();
        $sub_clause->add(condition($field, 'changedto', $value_to)) if $value_to ne '';
        $sub_clause->add(condition($field, 'changedafter', $date_from)) if $date_from ne '';
        $sub_clause->add(condition($field, 'changedbefore', $date_to)) if $date_to ne '';
        $join_clause->add($sub_clause);
    }
    $clause->add($join_clause);

    return @{$clause->children} ? $clause : undef;
}

sub _special_parse_deadline {
    my ($self) = @_;
    my $params = $self->_params;

    my $clause = new Bugzilla::Search::Clause();
    if (my $from = $params->{'deadlinefrom'}) {
        $clause->add('deadline', 'greaterthaneq', $from);
    }
    if (my $to = $params->{'deadlineto'}) {
        $clause->add('deadline', 'lessthaneq', $to);
    }

    return @{$clause->children} ? $clause : undef;
}

sub _special_parse_email {
    my ($self) = @_;
    my $params = $self->_params;
    
    my @email_params = grep { $_ =~ /^email\d+$/ } keys %$params;
    
    my $clause = new Bugzilla::Search::Clause();
    foreach my $param (@email_params) {
        $param =~ /(\d+)$/;
        my $id = $1;
        my $email = trim($params->{"email$id"});
        next if !$email;
        my $type = $params->{"emailtype$id"} || 'anyexact';
        # for backward compatibility
        $type = "equals" if $type eq "exact";

        my $or_clause = new Bugzilla::Search::Clause('OR');
        foreach my $field (qw(assigned_to reporter cc qa_contact)) {
            if ($params->{"email$field$id"}) {
                $or_clause->add($field, $type, $email);
            }
        }
        if ($params->{"emaillongdesc$id"}) {
            $or_clause->add("commenter", $type, $email);
        }
        
        $clause->add($or_clause);
    }

    return @{$clause->children} ? $clause : undef;
}

sub _special_parse_resolution {
    my ($self) = @_;
    my $params = $self->_params;
    return if !defined $params->{'resolution'};

    my @resolution = $self->_param_array('resolution');
    my $legal_resolutions = $self->_chart_fields->{resolution}->legal_values;
    @resolution = _valid_values(\@resolution, $legal_resolutions, '---');
    if (scalar(@resolution) == scalar(@$legal_resolutions)) {
        delete $params->{'resolution'};
    }
}

sub _valid_values {
    my ($input, $valid, $extra_value) = @_;
    my @result;
    foreach my $item (@$input) {
        $item = trim($item);
        if (defined $extra_value and $item eq $extra_value) {
            push(@result, $item);
        }
        elsif (grep { $_->name eq $item } @$valid) {
            push(@result, $item);
        }
    }
    return @result;
}

######################################
# Internal Accessors: Boolean Charts #
######################################

sub _charts_to_conditions {
    my ($self) = @_;
    
    my $clause = $self->_charts;
    my @joins;
    $clause->walk_conditions(sub {
        my ($clause, $condition) = @_;
        return if !$condition->translated;
        push(@joins, @{ $condition->translated->{joins} });
    });
    return (\@joins, $clause);
}

sub _charts {
    my ($self) = @_;
    
    my $clause = $self->_params_to_data_structure();
    my $chart_id = 0;
    $clause->walk_conditions(sub { $self->_handle_chart($chart_id++, @_) });
    return $clause;
}

sub _params_to_data_structure {
    my ($self) = @_;
    
    # First we get the "special" charts, representing all the normal
    # fields on the search page. This may modify _params, so it needs to
    # happen first.
    my $clause = $self->_special_charts;

    # Then we process the old Boolean Charts input format.
    $clause->add( $self->_boolean_charts );
    
    # And then process the modern "custom search" format.
    $clause->add( $self->_custom_search );
    
    return $clause;
}

sub _boolean_charts {
    my ($self) = @_;
    
    my $params = $self->_params;
    my @param_list = keys %$params;
    
    my @all_field_params = grep { /^field-?\d+/ } @param_list;
    my @chart_ids = map { /^field(-?\d+)/; $1 } @all_field_params;
    @chart_ids = sort { $a <=> $b } uniq @chart_ids;
    
    my $clause = new Bugzilla::Search::Clause();
    foreach my $chart_id (@chart_ids) {
        my @all_and = grep { /^field$chart_id-\d+/ } @param_list;
        my @and_ids = map { /^field$chart_id-(\d+)/; $1 } @all_and;
        @and_ids = sort { $a <=> $b } uniq @and_ids;
        
        my $and_clause = new Bugzilla::Search::Clause();
        foreach my $and_id (@and_ids) {
            my @all_or = grep { /^field$chart_id-$and_id-\d+/ } @param_list;
            my @or_ids = map { /^field$chart_id-$and_id-(\d+)/; $1 } @all_or;
            @or_ids = sort { $a <=> $b } uniq @or_ids;
            
            my $or_clause = new Bugzilla::Search::Clause('OR');
            foreach my $or_id (@or_ids) {
                my $identifier = "$chart_id-$and_id-$or_id";
                my $field = $params->{"field$identifier"};
                my $operator = $params->{"type$identifier"};
                my $value = $params->{"value$identifier"};
                # no-value operators ignore the value, however a value needs to be set
                $value = ' ' if $operator && grep { $_ eq $operator } NO_VALUE_OPERATORS;
                $or_clause->add($field, $operator, $value);
            }
            $and_clause->add($or_clause);
            $and_clause->negate(1) if $params->{"negate$chart_id"};
        }
        $clause->add($and_clause);
    }

    return @{$clause->children} ? $clause : undef;
}

sub _custom_search {
    my ($self) = @_;
    my $params = $self->_params;

    my @field_ids = $self->_field_ids;
    return unless scalar @field_ids;

    my $joiner = $params->{j_top} || '';
    my $current_clause = $joiner eq 'AND_G'
        ? new Bugzilla::Search::ClauseGroup()
        : new Bugzilla::Search::Clause($joiner);

    my @clause_stack;
    foreach my $id (@field_ids) {
        my $field = $params->{"f$id"};
        if ($field eq 'OP') {
            my $joiner = $params->{"j$id"} || '';
            my $new_clause = $joiner eq 'AND_G'
                ? new Bugzilla::Search::ClauseGroup()
                : new Bugzilla::Search::Clause($joiner);
            $new_clause->negate($params->{"n$id"});
            $current_clause->add($new_clause);
            push(@clause_stack, $current_clause);
            $current_clause = $new_clause;
            next;
        }
        if ($field eq 'CP') {
            $current_clause = pop @clause_stack;
            ThrowCodeError('search_cp_without_op', { id => $id })
                if !$current_clause;
            next;
        }
        
        my $operator = $params->{"o$id"};
        my $value = $params->{"v$id"};
        # no-value operators ignore the value, however a value needs to be set
        $value = ' ' if $operator && grep { $_ eq $operator } NO_VALUE_OPERATORS;
        my $condition = condition($field, $operator, $value);
        $condition->negate($params->{"n$id"});
        $current_clause->add($condition);
    }
    
    # We allow people to specify more OPs than CPs, so at the end of the
    # loop our top clause may be still in the stack instead of being
    # $current_clause.
    return $clause_stack[0] || $current_clause;
}

sub _field_ids {
    my ($self) = @_;
    my $params = $self->_params;
    my @param_list = keys %$params;
    
    my @field_params = grep { /^f\d+$/ } @param_list;
    my @field_ids = map { /(\d+)/; $1 } @field_params;
    @field_ids = sort { $a <=> $b } @field_ids;
    return @field_ids;
}

sub _handle_chart {
    my ($self, $chart_id, $clause, $condition) = @_;
    my $dbh = Bugzilla->dbh;
    my $params = $self->_params;
    my ($field, $operator, $value) = $condition->fov;
    return if (!defined $field or !defined $operator or !defined $value);
    $field = FIELD_MAP->{$field} || $field;

    my ($string_value, $orig_value);
    state $is_mysql = $dbh->isa('Bugzilla::DB::Mysql') ? 1 : 0;

    if (ref $value eq 'ARRAY') {
        # Trim input and ignore blank values.
        @$value = map { trim($_) } @$value;
        @$value = grep { defined $_ and $_ ne '' } @$value;
        return if !@$value;
        $orig_value = join(',', @$value);
        if ($field eq 'longdesc' && $is_mysql) {
            @$value = map { _convert_unicode_characters($_) } @$value;
        }
        $string_value = join(',', @$value);
    }
    else {
        return if $value eq '';
        $orig_value = $value;
        if ($field eq 'longdesc' && $is_mysql) {
            $value = _convert_unicode_characters($value);
        }
        $string_value = $value;
    }

    $self->_chart_fields->{$field}
        or ThrowCodeError("invalid_field_name", { field => $field });
    trick_taint($field);
    
    # This is the field as you'd reference it in a SQL statement.
    my $full_field = $field =~ /\./ ? $field : "bugs.$field";

    # "value" and "quoted" are for search functions that always operate
    # on a scalar string and never care if they were passed multiple
    # parameters. If the user does pass multiple parameters, they will
    # become a space-separated string for those search functions.
    #
    # all_values is for search functions that do operate
    # on multiple values, like anyexact.
    
    my %search_args = (
        chart_id     => $chart_id,
        sequence     => $chart_id,
        field        => $field,
        full_field   => $full_field,
        operator     => $operator,
        value        => $string_value,
        all_values   => $value,
        joins        => [],
        bugs_table   => 'bugs',
        table_suffix => '',
        condition    => $condition,
    );
    $clause->update_search_args(\%search_args);

    $search_args{quoted} = $self->_quote_unless_numeric(\%search_args);
    # This should add a "term" selement to %search_args.
    $self->do_search_function(\%search_args);

    # If term is left empty, then this means the criteria
    # has no effect and can be ignored.
    return unless $search_args{term};

    # All the things here that don't get pulled out of
    # %search_args are their original values before
    # do_search_function modified them.   
    $self->search_description({
        field => $field, type => $operator,
        value => $orig_value, term => $search_args{term},
    });

    foreach my $join (@{ $search_args{joins} }) {
        $join->{bugs_table}   = $search_args{bugs_table};
        $join->{table_suffix} = $search_args{table_suffix};
    }

    $condition->translated(\%search_args);
}

# XXX - This is a hack for MySQL which doesn't understand Unicode characters
# above U+FFFF, see Bugzilla::Comment::_check_thetext(). This hack can go away
# once we require MySQL 5.5.3 and use utf8mb4.
sub _convert_unicode_characters {
    my $string = shift;

    # Perl 5.13.8 and older complain about non-characters.
    no warnings 'utf8';
    $string =~ s/([\x{10000}-\x{10FFFF}])/"\x{FDD0}[" . uc(sprintf('U+%04x', ord($1))) . "]\x{FDD1}"/eg;
    return $string;
}

##################################
# do_search_function And Helpers #
##################################

# This takes information about the current boolean chart and translates
# it into SQL, using the constants at the top of this file.
sub do_search_function {
    my ($self, $args) = @_;
    my ($field, $operator) = @$args{qw(field operator)};
    
    if (my $parse_func = SPECIAL_PARSING->{$field}) {
        $self->$parse_func($args);
        # Some parsing functions set $term, though most do not.
        # For the ones that set $term, we don't need to do any further
        # parsing.
        return if $args->{term};
    }
    
    my $operator_field_override = $self->_get_operator_field_override();
    my $override = $operator_field_override->{$field};
    # Attachment fields get special handling, if they don't have a specific
    # individual override.
    if (!$override and $field =~ /^attachments\./) {
        $override = $operator_field_override->{attachments};
    }
    # If there's still no override, check for an override on the field's type.
    if (!$override) {
        my $field_obj = $self->_chart_fields->{$field};
        $override = $operator_field_override->{$field_obj->type};
    }
    
    if ($override) {
        my $search_func = $self->_pick_override_function($override, $operator);
        $self->$search_func($args) if $search_func;
    }

    # Some search functions set $term, and some don't. For the ones that
    # don't (or for fields that don't have overrides) we now call the
    # direct operator function from OPERATORS.
    if (!defined $args->{term}) {
        $self->_do_operator_function($args);
    }
    
    if (!defined $args->{term}) {
        # This field and this type don't work together. Generally,
        # this should never be reached, because it should be handled
        # explicitly by OPERATOR_FIELD_OVERRIDE.
        ThrowUserError("search_field_operator_invalid",
                       { field => $field, operator => $operator });
    }
}

# A helper for various search functions that need to run operator
# functions directly.
sub _do_operator_function {
    my ($self, $func_args) = @_;
    my $operator = $func_args->{operator};
    my $operator_func = OPERATORS->{$operator}
      || ThrowCodeError("search_field_operator_unsupported",
                        { operator => $operator });
    $self->$operator_func($func_args);
}

sub _reverse_operator {
    my ($self, $operator) = @_;
    my $reverse = OPERATOR_REVERSE->{$operator};
    return $reverse if $reverse;
    if ($operator =~ s/^not//) {
        return $operator;
    }
    return "not$operator";
}

sub _pick_override_function {
    my ($self, $override, $operator) = @_;
    my $search_func = $override->{$operator};

    if (!$search_func) {
        # If we don't find an override for one specific operator,
        # then there are some special override types:
        # _non_changed: For any operator that doesn't have the word
        #               "changed" in it
        # _default: Overrides all operators that aren't explicitly specified.
        if ($override->{_non_changed} and $operator !~ /changed/) {
            $search_func = $override->{_non_changed};
        }
        elsif ($override->{_default}) {
            $search_func = $override->{_default};
        }
    }

    return $search_func;
}

sub _get_operator_field_override {
    my $self = shift;
    my $cache = Bugzilla->request_cache;

    return $cache->{operator_field_override} 
        if defined $cache->{operator_field_override};

    my %operator_field_override = %{ OPERATOR_FIELD_OVERRIDE() };
    Bugzilla::Hook::process('search_operator_field_override',
                            { search => $self, 
                              operators => \%operator_field_override });

    $cache->{operator_field_override} = \%operator_field_override;
    return $cache->{operator_field_override};
}

sub _get_column_joins {
    my $self = shift;
    my $cache = Bugzilla->request_cache;

    return $cache->{column_joins} if defined $cache->{column_joins};

    my %column_joins = %{ $self->COLUMN_JOINS() };
    Bugzilla::Hook::process('buglist_column_joins',
                            { column_joins => \%column_joins });

    $cache->{column_joins} = \%column_joins;
    return $cache->{column_joins};
}

###########################
# Search Function Helpers #
###########################

# When we're doing a numeric search against a numeric column, we want to
# just put a number into the SQL instead of a string. On most DBs, this
# is just a performance optimization, but on SQLite it actually changes
# the behavior of some searches.
sub _quote_unless_numeric {
    my ($self, $args, $value) = @_;
    if (!defined $value) {
        $value = $args->{value};
    }
    my ($field, $operator) = @$args{qw(field operator)};
    
    my $numeric_operator = !grep { $_ eq $operator } NON_NUMERIC_OPERATORS;
    my $numeric_field = $self->_chart_fields->{$field}->is_numeric;
    my $numeric_value = ($value =~ NUMBER_REGEX) ? 1 : 0;
    my $is_numeric = $numeric_operator && $numeric_field && $numeric_value;

    # These operators are really numeric operators with numeric fields.
    $numeric_operator = grep { $_ eq $operator } keys %{ SIMPLE_OPERATORS() };

    if ($is_numeric) {
        my $quoted = $value;
        trick_taint($quoted);
        return $quoted;
    }
    elsif ($numeric_field && !$numeric_value && $numeric_operator) {
        ThrowUserError('number_not_numeric', { field => $field, num => $value });
    }
    return Bugzilla->dbh->quote($value);
}

sub build_subselect {
    my ($outer, $inner, $table, $cond, $negate) = @_;
    if ($table =~ /\battach_data\b/) {
        # It takes a long time to scan the whole attach_data table
        # unconditionally, so we return the subselect and let the DB optimizer
        # restrict the search based on other search criteria.
        my $not = $negate ? "NOT" : "";
        return "$outer $not IN (SELECT DISTINCT $inner FROM $table WHERE $cond)";
    }
    # Execute subselects immediately to avoid dependent subqueries, which are
    # large performance hits on MySql
    my $q = "SELECT DISTINCT $inner FROM $table WHERE $cond";
    my $dbh = Bugzilla->dbh;
    my $list = $dbh->selectcol_arrayref($q);
    return $negate ? "1=1" : "1=2" unless @$list;
    return $dbh->sql_in($outer, $list, $negate);
}

# Used by anyexact to get the list of input values. This allows us to
# support values with commas inside of them in the standard charts, and
# still accept string values for the boolean charts (and split them on
# commas).
sub _all_values {
    my ($self, $args, $split_on) = @_;
    $split_on ||= qr/[\s,]+/;
    my $dbh = Bugzilla->dbh;
    my $all_values = $args->{all_values};
    
    my @array;
    if (ref $all_values eq 'ARRAY') {
        @array = @$all_values;
    }
    else {
        @array = split($split_on, $all_values);
        @array = map { trim($_) } @array;
        @array = grep { defined $_ and $_ ne '' } @array;
    }
    
    if ($args->{field} eq 'resolution') {
        @array = map { $_ eq '---' ? '' : $_ } @array;
    }
    
    return @array;
}

# Support for "any/all/nowordssubstr" comparison type ("words as substrings")
sub _substring_terms {
    my ($self, $args) = @_;
    my $dbh = Bugzilla->dbh;

    # We don't have to (or want to) use _all_values, because we'd just
    # split each term on spaces and commas anyway.
    my @words = split(/[\s,]+/, $args->{value});
    @words = grep { defined $_ and $_ ne '' } @words;
    my @terms = map { $dbh->sql_ilike($_, $args->{full_field}) } @words;
    return @terms;
}

sub _word_terms {
    my ($self, $args) = @_;
    my $dbh = Bugzilla->dbh;
    
    my @values = split(/[\s,]+/, $args->{value});
    @values = grep { defined $_ and $_ ne '' } @values;
    my @substring_terms = $self->_substring_terms($args);
    
    my @terms;
    my $start = $dbh->WORD_START;
    my $end   = $dbh->WORD_END;
    foreach my $word (@values) {
        my $regex  = $start . quotemeta($word) . $end;
        my $quoted = $dbh->quote($regex);
        # We don't have to check the regexp, because we escaped it, so we're
        # sure it's valid.
        my $regex_term = $dbh->sql_regexp($args->{full_field}, $quoted,
                                          'no check');
        # Regular expressions are slow--substring searches are faster.
        # If we're searching for a word, we're also certain that the
        # substring will appear in the value. So we limit first by
        # substring and then by a regex that will match just words.
        my $substring_term = shift @substring_terms;
        push(@terms, "$substring_term AND $regex_term");
    }
    
    return @terms;
}

#####################################
# "Special Parsing" Functions: Date #
#####################################

sub _timestamp_translate {
    my ($self, $ignore_time, $args) = @_;
    my $value = $args->{value};
    my $dbh = Bugzilla->dbh;

    return if $value !~ /^(?:[\+\-]?\d+[hdwmy]s?|now)$/i;

    $value = SqlifyDate($value);
    # By default, the time is appended to the date, which we don't always want.
    if ($ignore_time) {
        ($value) = split(/\s/, $value);
    }
    $args->{value} = $value;
    $args->{quoted} = $dbh->quote($value);
}

sub _datetime_translate {
    return shift->_timestamp_translate(0, @_);
}

sub _last_visit_datetime {
    my ($self, $args) = @_;
    my $value = $args->{value};

    $self->_datetime_translate($args);
    if ($value eq $args->{value}) {
        # Failed to translate a datetime. let's try the pronoun expando.
        if ($value eq '%last_changed%') {
            $self->_add_extra_column('changeddate');
            $args->{value} = $args->{quoted} = 'bugs.delta_ts';
        }
    }
}


sub _date_translate {
    return shift->_timestamp_translate(1, @_);
}

sub SqlifyDate {
    my ($str) = @_;
    my $fmt = "%Y-%m-%d %H:%M:%S";
    $str = "" if (!defined $str || lc($str) eq 'now');
    if ($str eq "") {
        my ($sec, $min, $hour, $mday, $month, $year, $wday) = localtime(time());
        return sprintf("%4d-%02d-%02d 00:00:00", $year+1900, $month+1, $mday);
    }

    if ($str =~ /^(-|\+)?(\d+)([hdwmy])(s?)$/i) {   # relative date
        my ($sign, $amount, $unit, $startof, $date) = ($1, $2, lc $3, lc $4, time);
        my ($sec, $min, $hour, $mday, $month, $year, $wday)  = localtime($date);
        if ($sign && $sign eq '+') { $amount = -$amount; }
        $startof = 1 if $amount == 0;
        if ($unit eq 'w') {                  # convert weeks to days
            $amount = 7*$amount;
            $amount += $wday if $startof;
            $unit = 'd';
        }
        if ($unit eq 'd') {
            if ($startof) {
              $fmt = "%Y-%m-%d 00:00:00";
              $date -= $sec + 60*$min + 3600*$hour;
            }
            $date -= 24*3600*$amount;
            return time2str($fmt, $date);
        }
        elsif ($unit eq 'y') {
            if ($startof) {
                return sprintf("%4d-01-01 00:00:00", $year+1900-$amount);
            } 
            else {
                return sprintf("%4d-%02d-%02d %02d:%02d:%02d", 
                               $year+1900-$amount, $month+1, $mday, $hour, $min, $sec);
            }
        }
        elsif ($unit eq 'm') {
            $month -= $amount;
            $year += floor($month/12);
            $month %= 12;
            if ($startof) {
                return sprintf("%4d-%02d-01 00:00:00", $year+1900, $month+1);
            }
            else {
                return sprintf("%4d-%02d-%02d %02d:%02d:%02d", 
                               $year+1900, $month+1, $mday, $hour, $min, $sec);
            }
        }
        elsif ($unit eq 'h') {
            # Special case for 'beginning of an hour'
            if ($startof) {
                $fmt = "%Y-%m-%d %H:00:00";
            } 
            $date -= 3600*$amount;
            return time2str($fmt, $date);
        }
        return undef;                      # should not happen due to regexp at top
    }
    my $date = str2time($str);
    if (!defined($date)) {
        ThrowUserError("illegal_date", { date => $str });
    }
    return time2str($fmt, $date);
}

######################################
# "Special Parsing" Functions: Users #
######################################

sub pronoun {
    my ($noun, $user) = (@_);
    if ($noun eq "%user%") {
        if ($user->id) {
            return $user->id;
        } else {
            ThrowUserError('login_required_for_pronoun');
        }
    }
    if ($noun eq "%reporter%") {
        return "bugs.reporter";
    }
    if ($noun eq "%assignee%") {
        return "bugs.assigned_to";
    }
    if ($noun eq "%qacontact%") {
        return "COALESCE(bugs.qa_contact,0)";
    }

    ThrowUserError('illegal_pronoun', { pronoun => $noun });
}

sub _contact_pronoun {
    my ($self, $args) = @_;
    my $value = $args->{value};
    my $user = $self->_user;

    if ($value =~ /^\%group\.[^%]+%$/) {
        $self->_contact_exact_group($args);
    }
    elsif ($value =~ /^(%\w+%)$/) {
        $args->{value} = pronoun($1, $user);
        $args->{quoted} = $args->{value};
        $args->{value_is_id} = 1;
    }
}

sub _contact_exact_group {
    my ($self, $args) = @_;
    my ($value, $operator, $field, $chart_id, $joins, $sequence) =
        @$args{qw(value operator field chart_id joins sequence)};
    my $dbh = Bugzilla->dbh;
    my $user = $self->_user;

    # We already know $value will match this regexp, else we wouldn't be here.
    $value =~ /\%group\.([^%]+)%/;
    my $group_name = $1;
    my $group = Bugzilla::Group->check({ name => $group_name, _error => 'invalid_group_name' });
    # Pass $group_name instead of $group->name to the error message
    # to not leak the existence of the group.
    $user->in_group($group)
      || ThrowUserError('invalid_group_name', { name => $group_name });
    # Now that we know the user belongs to this group, it's safe
    # to disclose more information.
    $group->check_members_are_visible();

    my $group_ids = Bugzilla::Group->flatten_group_membership($group->id);

    if ($field eq 'cc' && $chart_id eq '') {
        # This is for the email1, email2, email3 fields from query.cgi.
        $chart_id = "CC$$sequence";
        $args->{sequence}++;
    }

    my $from = $field;
    # These fields need an additional table.
    if ($field =~ /^(commenter|cc)$/) {
        my $join_table = $field;
        $join_table = 'longdescs' if $field eq 'commenter';
        my $join_table_alias = "${field}_$chart_id";
        push(@$joins, { table => $join_table, as => $join_table_alias });
        $from = "$join_table_alias.who";
    }

    my $table = "user_group_map_$chart_id";
    my $join = {
        table => 'user_group_map',
        as    => $table,
        from  => $from,
        to    => 'user_id',
        extra => [$dbh->sql_in("$table.group_id", $group_ids),
                  "$table.isbless = 0"],
    };
    push(@$joins, $join);
    if ($operator =~ /^not/) {
        $args->{term} = "$table.group_id IS NULL";
    }
    else {
        $args->{term} = "$table.group_id IS NOT NULL";
    }
}

sub _get_user_id {
    my ($self, $value) = @_;

    if ($value =~ /^%\w+%$/) {
        return pronoun($value, $self->_user);
    }
    return login_to_id($value, THROW_ERROR);
}

#####################################################################
# Search Functions
#####################################################################

sub _invalid_combination {
    my ($self, $args) = @_;
    my ($field, $operator) = @$args{qw(field operator)};
    ThrowUserError('search_field_operator_invalid',
                   { field => $field, operator => $operator });
}

# For all the "user" fields--assigned_to, reporter, qa_contact,
# cc, commenter, requestee, etc.
sub _user_nonchanged {
    my ($self, $args) = @_;
    my ($field, $operator, $chart_id, $sequence, $joins) =
        @$args{qw(field operator chart_id sequence joins)};

    my $is_in_other_table;
    if (my $join = USER_FIELDS->{$field}->{join}) {
        $is_in_other_table = 1;
        my $as = "${field}_$chart_id";
        # Needed for setters.login_name and requestees.login_name.
        # Otherwise when we try to join "profiles" below, we'd get
        # something like "setters.login_name.login_name" in the "from".
        $as =~ s/\./_/g;        
        # This helps implement the email1, email2, etc. parameters.
        if ($chart_id =~ /default/) {
            $as .= "_$sequence";
        }
        my $isprivate = USER_FIELDS->{$field}->{isprivate};
        my $extra = ($isprivate and !$self->_user->is_insider)
                    ? ["$as.isprivate = 0"] : [];
        # We want to copy $join so as not to modify USER_FIELDS.
        push(@$joins, { %$join, as => $as, extra => $extra });
        my $search_field = USER_FIELDS->{$field}->{field};
        $args->{full_field} = "$as.$search_field";
    }

    my $is_nullable = USER_FIELDS->{$field}->{nullable};
    my $null_alternate = "''";
    # When using a pronoun, we use the userid, and we don't have to
    # join the profiles table.
    if ($args->{value_is_id}) {
        $null_alternate = 0;
    }
    elsif (substr($field, -9) eq '_realname') {
        my $as = "name_${field}_$chart_id";
        # For fields with periods in their name.
        $as =~ s/\./_/;
        my $join = {
            table => 'profiles',
            as    => $as,
            from  => substr($args->{full_field}, 0, -9),
            to    => 'userid',
            join  => (!$is_in_other_table and !$is_nullable) ? 'INNER' : undef,
        };
        push(@$joins, $join);
        $args->{full_field} = "$as.realname";
    }
    else {
        my $as = "name_${field}_$chart_id";
        # For fields with periods in their name.
        $as =~ s/\./_/;
        my $join = {
            table => 'profiles',
            as    => $as,
            from  => $args->{full_field},
            to    => 'userid',
            join  => (!$is_in_other_table and !$is_nullable) ? 'INNER' : undef,
        };
        push(@$joins, $join);
        $args->{full_field} = "$as.login_name";
    }

    # We COALESCE fields that can be NULL, to make "not"-style operators
    # continue to work properly. For example, "qa_contact is not equal to bob"
    # should also show bugs where the qa_contact is NULL. With COALESCE,
    # it does.
    if ($is_nullable) {
        $args->{full_field} = "COALESCE($args->{full_field}, $null_alternate)";
    }
    
    # For fields whose values are stored in other tables, negation (NOT)
    # only works properly if we put the condition into the JOIN instead
    # of the WHERE.
    if ($is_in_other_table) {
        # Using the last join works properly whether we're searching based
        # on userid or login_name.
        my $last_join = $joins->[-1];
        
        # For negative operators, the system we're using here
        # only works properly if we reverse the operator and check IS NULL
        # in the WHERE.
        my $is_negative = $operator =~ /^(?:no|isempty)/ ? 1 : 0;
        if ($is_negative) {
            $args->{operator} = $self->_reverse_operator($operator);
        }
        $self->_do_operator_function($args);
        push(@{ $last_join->{extra} }, $args->{term});
        
        # For login_name searches, we only want a single join.
        # So we create a subselect table out of our two joins. This makes
        # negation (NOT) work properly for values that are in other
        # tables.
        if ($last_join->{table} eq 'profiles') {
            pop @$joins;
            $last_join->{join} = 'INNER';
            my ($join_sql) = $self->_translate_join($last_join);
            my $first_join = $joins->[-1];
            my $as = $first_join->{as};            
            my $table = $first_join->{table};
            my $columns = "bug_id";
            $columns .= ",isprivate" if @{ $first_join->{extra} };
            my $new_table = "SELECT DISTINCT $columns FROM $table AS $as $join_sql";
            $first_join->{table} = "($new_table)";
            # We always want to LEFT JOIN the generated table.
            delete $first_join->{join};
            # To support OR charts, we need multiple tables.
            my $new_as = $first_join->{as} . "_$sequence";
            $_ =~ s/\Q$as\E/$new_as/ foreach @{ $first_join->{extra} };
            $first_join->{as} = $new_as;
            $last_join = $first_join;
        }
        
        # If we're joining the first table (we're using a pronoun and
        # searching by user id) then we need to check $other_table->{field}.
        my $check_field = $last_join->{as} . '.bug_id';
        if ($is_negative) {
            $args->{term} = "$check_field IS NULL";
        }
        else {
            $args->{term} = "$check_field IS NOT NULL";
        }
    }
}

# XXX This duplicates having Commenter as a search field.
sub _long_desc_changedby {
    my ($self, $args) = @_;
    my ($chart_id, $joins, $value) = @$args{qw(chart_id joins value)};

    my $table = "longdescs_$chart_id";
    push(@$joins, { table => 'longdescs', as => $table });
    my $user_id = $self->_get_user_id($value);
    $args->{term} = "$table.who = $user_id";

    # If the user is not part of the insiders group, they cannot see
    # private comments
    if (!$self->_user->is_insider) {
        $args->{term} .= " AND $table.isprivate = 0";
    }
}

sub _long_desc_changedbefore_after {
    my ($self, $args) = @_;
    my ($chart_id, $operator, $value, $joins) =
        @$args{qw(chart_id operator value joins)};
    my $dbh = Bugzilla->dbh;

    my $sql_operator = ($operator =~ /before/) ? '<=' : '>=';
    my $table = "longdescs_$chart_id";
    my $sql_date = $dbh->quote(SqlifyDate($value));
    my $join = {
        table => 'longdescs',
        as    => $table,
        extra => ["$table.bug_when $sql_operator $sql_date"],
    };
    push(@$joins, $join);
    $args->{term} = "$table.bug_when IS NOT NULL";

    # If the user is not part of the insiders group, they cannot see
    # private comments
    if (!$self->_user->is_insider) {
        $args->{term} .= " AND $table.isprivate = 0";
    }
}

sub _long_desc_nonchanged {
    my ($self, $args) = @_;
    my ($chart_id, $operator, $value, $joins, $bugs_table) =
        @$args{qw(chart_id operator value joins bugs_table)};

    if ($operator =~ /^is(not)?empty$/) {
        $args->{term} = $self->_multiselect_isempty($args, $operator eq 'isnotempty');
        return;
    }
    my $dbh = Bugzilla->dbh;

    my $table = "longdescs_$chart_id";
    my $join_args = {
        chart_id   => $chart_id,
        sequence   => $chart_id,
        field      => 'longdesc',
        full_field => "$table.thetext",
        operator   => $operator,
        value      => $value,
        all_values => $value,
        quoted     => $dbh->quote($value),
        joins      => [],
        bugs_table => $bugs_table,
    };
    $self->_do_operator_function($join_args);

    # If the user is not part of the insiders group, they cannot see
    # private comments
    if (!$self->_user->is_insider) {
        $join_args->{term} .= " AND $table.isprivate = 0";
    }

    my $join = {
        table => 'longdescs',
        as    => $table,
        extra => [ $join_args->{term} ],
    };
    push(@$joins, $join);

    $args->{term} =  "$table.comment_id IS NOT NULL";
}

sub _content_matches {
    my ($self, $args) = @_;
    my ($chart_id, $joins, $fields, $operator, $value) =
        @$args{qw(chart_id joins fields operator value)};
    my $dbh = Bugzilla->dbh;

    # "content" is an alias for columns containing text for which we
    # can search a full-text index and retrieve results by relevance,
    # currently just bug comments (and summaries to some degree).
    # There's only one way to search a full-text index, so we only
    # accept the "matches" operator, which is specific to full-text
    # index searches.

    # Add the fulltext table to the query so we can search on it.
    my $table = "bugs_fulltext_$chart_id";
    my $comments_col = "comments";
    $comments_col = "comments_noprivate" unless $self->_user->is_insider;
    push(@$joins, { table => 'bugs_fulltext', as => $table });
    
    # Create search terms to add to the SELECT and WHERE clauses.
    my ($term1, $rterm1) =
        $dbh->sql_fulltext_search("$table.$comments_col", $value);
    my ($term2, $rterm2) =
        $dbh->sql_fulltext_search("$table.short_desc", $value);
    $rterm1 = $term1 if !$rterm1;
    $rterm2 = $term2 if !$rterm2;

    # The term to use in the WHERE clause.
    my $term = "$term1 OR $term2";
    if ($operator =~ /not/i) {
        $term = "NOT($term)";
    }
    $args->{term} = $term;
    
    # In order to sort by relevance (in case the user requests it),
    # we SELECT the relevance value so we can add it to the ORDER BY
    # clause. Every time a new fulltext chart isadded, this adds more 
    # terms to the relevance sql.
    #
    # We build the relevance SQL by modifying the COLUMNS list directly,
    # which is kind of a hack but works.
    my $current = $self->COLUMNS->{'relevance'}->{name};
    $current = $current ? "$current + " : '';
    # For NOT searches, we just add 0 to the relevance.
    my $select_term = $operator =~ /not/ ? 0 : "($current$rterm1 + $rterm2)";
    $self->COLUMNS->{'relevance'}->{name} = $select_term;
}

sub _long_descs_count {
    my ($self, $args) = @_;
    my ($chart_id, $joins) = @$args{qw(chart_id joins)};
    my $table = "longdescs_count_$chart_id";
    my $extra =  $self->_user->is_insider ? "" : "WHERE isprivate = 0";
    my $join = {
        table => "(SELECT bug_id, COUNT(*) AS num"
                 . " FROM longdescs $extra GROUP BY bug_id)",
        as    => $table,
    };
    push(@$joins, $join);
    $args->{full_field} = "${table}.num";
}

sub _work_time_changedby {
    my ($self, $args) = @_;
    my ($chart_id, $joins, $value) = @$args{qw(chart_id joins value)};
    
    my $table = "longdescs_$chart_id";
    push(@$joins, { table => 'longdescs', as => $table });
    my $user_id = $self->_get_user_id($value);
    $args->{term} = "$table.who = $user_id AND $table.work_time != 0";
}

sub _work_time_changedbefore_after {
    my ($self, $args) = @_;
    my ($chart_id, $operator, $value, $joins) =
        @$args{qw(chart_id operator value joins)};
    my $dbh = Bugzilla->dbh;
    
    my $table = "longdescs_$chart_id";
    my $sql_operator = ($operator =~ /before/) ? '<=' : '>=';
    my $sql_date = $dbh->quote(SqlifyDate($value));
    my $join = {
        table => 'longdescs',
        as    => $table,
        extra => ["$table.work_time != 0",
                  "$table.bug_when $sql_operator $sql_date"],
    };
    push(@$joins, $join);
    
    $args->{term} = "$table.bug_when IS NOT NULL";
}

sub _work_time {
    my ($self, $args) = @_;
    $self->_add_extra_column('actual_time');
    $args->{full_field} = $self->COLUMNS->{actual_time}->{name};
}

sub _percentage_complete {
    my ($self, $args) = @_;
    
    $args->{full_field} = $self->COLUMNS->{percentage_complete}->{name};

    # We need actual_time in _select_columns, otherwise we can't use
    # it in the expression for searching percentage_complete.
    $self->_add_extra_column('actual_time');
}

sub _last_visit_ts {
    my ($self, $args) = @_;

    $args->{full_field} = $self->COLUMNS->{last_visit_ts}->{name};
    $self->_add_extra_column('last_visit_ts');
}

sub _last_visit_ts_invalid_operator {
    my ($self, $args) = @_;

    ThrowUserError('search_field_operator_invalid',
        { field    => $args->{field},
          operator => $args->{operator} });
}

sub _days_elapsed {
    my ($self, $args) = @_;
    my $dbh = Bugzilla->dbh;
    
    $args->{full_field} = "(" . $dbh->sql_to_days('NOW()') . " - " .
                                $dbh->sql_to_days('bugs.delta_ts') . ")";
}

sub _component_nonchanged {
    my ($self, $args) = @_;
    
    $args->{full_field} = "components.name";
    $self->_do_operator_function($args);
    my $term = $args->{term};
    $args->{term} = build_subselect("bugs.component_id",
        "components.id", "components", $args->{term});
}

sub _product_nonchanged {
    my ($self, $args) = @_;
    
    # Generate the restriction condition
    $args->{full_field} = "products.name";
    $self->_do_operator_function($args);
    my $term = $args->{term};
    $args->{term} = build_subselect("bugs.product_id",
        "products.id", "products", $term);
}

sub _alias_nonchanged {
    my ($self, $args) = @_;

    $args->{full_field} = "bugs_aliases.alias";
    $self->_do_operator_function($args);
    $args->{term} = build_subselect("bugs.bug_id",
        "bugs_aliases.bug_id", "bugs_aliases", $args->{term});
}

sub _classification_nonchanged {
    my ($self, $args) = @_;
    my $joins = $args->{joins};
    
    # This joins the right tables for us.
    $self->_add_extra_column('product');
    
    # Generate the restriction condition    
    $args->{full_field} = "classifications.name";
    $self->_do_operator_function($args);
    my $term = $args->{term};
    $args->{term} = build_subselect("map_product.classification_id",
        "classifications.id", "classifications", $term);
}

sub _nullable {
    my ($self, $args) = @_;
    my $field = $args->{full_field};
    $args->{full_field} = "COALESCE($field, '')";
}

sub _nullable_int {
    my ($self, $args) = @_;
    my $field = $args->{full_field};
    $args->{full_field} = "COALESCE($field, 0)";
}

sub _nullable_datetime {
    my ($self, $args) = @_;
    my $field = $args->{full_field};
    my $empty = Bugzilla->dbh->quote(EMPTY_DATETIME);
    $args->{full_field} = "COALESCE($field, $empty)";
}

sub _nullable_date {
    my ($self, $args) = @_;
    my $field = $args->{full_field};
    my $empty = Bugzilla->dbh->quote(EMPTY_DATE);
    $args->{full_field} = "COALESCE($field, $empty)";
}

sub _deadline {
    my ($self, $args) = @_;
    my $field = $args->{full_field};
    # This makes "equals" searches work on all DBs (even on MySQL, which
    # has a bug: http://bugs.mysql.com/bug.php?id=60324).
    $args->{full_field} = Bugzilla->dbh->sql_date_format($field, '%Y-%m-%d');
    $self->_nullable_datetime($args);
}

sub _owner_idle_time_greater_less {
    my ($self, $args) = @_;
    my ($chart_id, $joins, $value, $operator) =
        @$args{qw(chart_id joins value operator)};
    my $dbh = Bugzilla->dbh;
    
    my $table = "idle_$chart_id";
    my $quoted = $dbh->quote(SqlifyDate($value));
    
    my $ld_table = "comment_$table";
    my $act_table = "activity_$table";    
    my $comments_join = {
        table => 'longdescs',
        as    => $ld_table,
        from  => 'assigned_to',
        to    => 'who',
        extra => ["$ld_table.bug_when > $quoted"],
    };
    my $activity_join = {
        table => 'bugs_activity',
        as    => $act_table,
        from  => 'assigned_to',
        to    => 'who',
        extra => ["$act_table.bug_when > $quoted"]
    };
    
    push(@$joins, $comments_join, $activity_join);
    
    if ($operator =~ /greater/) {
        $args->{term} =
            "$ld_table.who IS NULL AND $act_table.who IS NULL";
    } else {
         $args->{term} =
            "($ld_table.who IS NOT NULL OR $act_table.who IS NOT NULL)";
    }
}

sub _multiselect_negative {
    my ($self, $args) = @_;
    my ($field, $operator) = @$args{qw(field operator)};

    $args->{operator} = $self->_reverse_operator($operator);
    $args->{term} = $self->_multiselect_term($args, 1);
}

sub _multiselect_multiple {
    my ($self, $args) = @_;
    my ($chart_id, $field, $operator, $value)
        = @$args{qw(chart_id field operator value)};
    my $dbh = Bugzilla->dbh;
    
    # We want things like "cf_multi_select=two+words" to still be
    # considered a search for two separate words, unless we're using
    # anyexact. (_all_values would consider that to be one "word" with a
    # space in it, because it's not in the Boolean Charts).
    my @words = $operator eq 'anyexact' ? $self->_all_values($args)
                                        : split(/[\s,]+/, $value);
    
    my @terms;
    foreach my $word (@words) {
        next if $word eq '';
        $args->{value} = $word;
        $args->{quoted} = $dbh->quote($word);
        push(@terms, $self->_multiselect_term($args));
    }
    
    # The spacing in the joins helps make the resulting SQL more readable.
    if ($operator =~ /^any/) {
        $args->{term} = join("\n        OR ", @terms);
    }
    else {
        $args->{term} = join("\n        AND ", @terms);
    }
}

sub _flagtypes_nonchanged {
    my ($self, $args) = @_;
    my ($chart_id, $operator, $value, $joins, $bugs_table, $condition) =
        @$args{qw(chart_id operator value joins bugs_table condition)};

    if ($operator =~ /^is(not)?empty$/) {
        $args->{term} = $self->_multiselect_isempty($args, $operator eq 'isnotempty');
        return;
    }

    my $dbh = Bugzilla->dbh;

    # For 'not' operators, we need to negate the whole term.
    # If you search for "Flags" (does not contain) "approval+" we actually want
    # to return *bugs* that don't contain an approval+ flag.  Without rewriting
    # the negation we'll search for *flags* which don't contain approval+.
    if ($operator =~ s/^not//) {
        $args->{operator} = $operator;
        $condition->operator($operator);
        $condition->negate(1);
    }

    my $subselect_args = {
        chart_id   => $chart_id,
        sequence   => $chart_id,
        field      => 'flagtypes.name',
        full_field =>  $dbh->sql_string_concat("flagtypes_$chart_id.name", "flags_$chart_id.status"),
        operator   => $operator,
        value      => $value,
        all_values => $value,
        quoted     => $dbh->quote($value),
        joins      => [],
        bugs_table => "bugs_$chart_id",
    };
    $self->_do_operator_function($subselect_args);
    my $subselect_term = $subselect_args->{term};

    # don't call build_subselect as this must run as a true sub-select
    $args->{term} = "EXISTS (
        SELECT 1
          FROM $bugs_table bugs_$chart_id
          LEFT JOIN attachments AS attachments_$chart_id
                    ON bugs_$chart_id.bug_id = attachments_$chart_id.bug_id
          LEFT JOIN flags AS flags_$chart_id
                    ON bugs_$chart_id.bug_id = flags_$chart_id.bug_id
                       AND (flags_$chart_id.attach_id = attachments_$chart_id.attach_id
                            OR flags_$chart_id.attach_id IS NULL)
          LEFT JOIN flagtypes AS flagtypes_$chart_id
                    ON flags_$chart_id.type_id = flagtypes_$chart_id.id
     WHERE bugs_$chart_id.bug_id = $bugs_table.bug_id
           AND $subselect_term
    )";
}

sub _multiselect_nonchanged {
    my ($self, $args) = @_;
    my ($chart_id, $joins, $field, $operator) =
        @$args{qw(chart_id joins field operator)};
    $args->{term} = $self->_multiselect_term($args)
}

sub _multiselect_table {
    my ($self, $args) = @_;
    my ($field, $chart_id) = @$args{qw(field chart_id)};
    my $dbh = Bugzilla->dbh;
    
    if ($field eq 'keywords') {
        $args->{full_field} = 'keyworddefs.name';
        return "keywords INNER JOIN keyworddefs".
                               " ON keywords.keywordid = keyworddefs.id";
    }
    elsif ($field eq 'tag') {
        $args->{full_field} = 'tag.name';
        return "bug_tag INNER JOIN tag ON bug_tag.tag_id = tag.id AND user_id = "
               . ($self->_sharer_id || $self->_user->id);
    }
    elsif ($field eq 'bug_group') {
        $args->{full_field} = 'groups.name';
        return "bug_group_map INNER JOIN groups
                                      ON bug_group_map.group_id = groups.id";
    }
    elsif ($field eq 'blocked' or $field eq 'dependson') {
        my $select = $field eq 'blocked' ? 'dependson' : 'blocked';
        $args->{_select_field} = $select;
        $args->{full_field} = $field;
        return "dependencies";
    }
    elsif ($field eq 'longdesc') {
        $args->{_extra_where} = " AND isprivate = 0"
            if !$self->_user->is_insider;
        $args->{full_field} = 'thetext';
        return "longdescs";
    }
    elsif ($field eq 'longdescs.isprivate') {
        ThrowUserError('auth_failure', { action => 'search',
                                         object => 'bug_fields',
                                         field => 'longdescs.isprivate' })
            if !$self->_user->is_insider;
        $args->{full_field} = 'isprivate';
        return "longdescs";
    }
    elsif ($field =~ /^attachments/) {
        $args->{_extra_where} = " AND isprivate = 0"
            if !$self->_user->is_insider;
        $field =~ /^attachments\.(.+)$/;
        $args->{full_field} = $1;
        return "attachments";
    }
    elsif ($field eq 'attach_data.thedata') {
        $args->{_extra_where} = " AND attachments.isprivate = 0"
            if !$self->_user->is_insider;
        return "attachments INNER JOIN attach_data "
               . " ON attachments.attach_id = attach_data.id"
    }
    elsif ($field eq 'comment_tag') {
        $args->{_extra_where} = " AND longdescs.isprivate = 0"
            if !$self->_user->is_insider;
        $args->{full_field} = 'longdescs_tags.tag';
        return "longdescs INNER JOIN longdescs_tags".
               " ON longdescs.comment_id = longdescs_tags.comment_id";
    }
    my $table = "bug_$field";
    $args->{full_field} = "bug_$field.value";
    return $table;
}

sub _multiselect_term {
    my ($self, $args, $not) = @_;
    my ($operator) = $args->{operator};
    my $value = $args->{value} || '';
    # 'empty' operators require special handling
    return $self->_multiselect_isempty($args, $not)
        if ($operator =~ /^is(not)?empty$/ || $value eq '---');
    my $table = $self->_multiselect_table($args);
    $self->_do_operator_function($args);
    my $term = $args->{term};
    $term .= $args->{_extra_where} || '';
    my $select = $args->{_select_field} || 'bug_id';
    return build_subselect("$args->{bugs_table}.bug_id", $select, $table, $term, $not);
}

# We can't use the normal operator_functions to build isempty queries which
# join to different tables.
sub _multiselect_isempty {
    my ($self, $args, $not) = @_;
    my ($field, $operator, $joins, $chart_id) = @$args{qw(field operator joins chart_id)};
    my $dbh = Bugzilla->dbh;
    $operator = $self->_reverse_operator($operator) if $not;
    $not = $operator eq 'isnotempty' ? 'NOT' : '';

    if ($field eq 'keywords') {
        push @$joins, {
            table => 'keywords',
            as    => "keywords_$chart_id",
            from  => 'bug_id',
            to    => 'bug_id',
        };
        return "keywords_$chart_id.bug_id IS $not NULL";
    }
    elsif ($field eq 'bug_group') {
        push @$joins, {
            table => 'bug_group_map',
            as    => "bug_group_map_$chart_id",
            from  => 'bug_id',
            to    => 'bug_id',
        };
        return "bug_group_map_$chart_id.bug_id IS $not NULL";
    }
    elsif ($field eq 'flagtypes.name') {
        push @$joins, {
            table => 'flags',
            as    => "flags_$chart_id",
            from  => 'bug_id',
            to    => 'bug_id',
        };
        return "flags_$chart_id.bug_id IS $not NULL";
    }
    elsif ($field eq 'blocked' or $field eq 'dependson') {
        my $to = $field eq 'blocked' ? 'dependson' : 'blocked';
        push @$joins, {
            table => 'dependencies',
            as    => "dependencies_$chart_id",
            from  => 'bug_id',
            to    => $to,
        };
        return "dependencies_$chart_id.$to IS $not NULL";
    }
    elsif ($field eq 'longdesc') {
        my @extra = ( "longdescs_$chart_id.type != " . CMT_HAS_DUPE );
        push @extra, "longdescs_$chart_id.isprivate = 0"
            unless $self->_user->is_insider;
        push @$joins, {
            table => 'longdescs',
            as    => "longdescs_$chart_id",
            from  => 'bug_id',
            to    => 'bug_id',
            extra => \@extra,
        };
        return $not
            ? "longdescs_$chart_id.thetext != ''"
            : "longdescs_$chart_id.thetext = ''";
    }
    elsif ($field eq 'longdescs.isprivate') {
        ThrowUserError('search_field_operator_invalid', { field  => $field,
                                                          operator => $operator });
    }
    elsif ($field =~ /^attachments\.(.+)/) {
        my $sub_field = $1;
        if ($sub_field eq 'description' || $sub_field eq 'filename' || $sub_field eq 'mimetype') {
            # can't be null/empty
            return $not ? '1=1' : '1=2';
        } else {
            # all other fields which get here are boolean
            ThrowUserError('search_field_operator_invalid', { field => $field,
                                                              operator => $operator });
        }
    }
    elsif ($field eq 'attach_data.thedata') {
        push @$joins, {
            table => 'attachments',
            as    => "attachments_$chart_id",
            from  => 'bug_id',
            to    => 'bug_id',
            extra => [ $self->_user->is_insider ? '' : "attachments_$chart_id.isprivate = 0" ],
        };
        push @$joins, {
            table => 'attach_data',
            as    => "attach_data_$chart_id",
            from  => "attachments_$chart_id.attach_id",
            to    => 'id',
        };
        return "attach_data_$chart_id.thedata IS $not NULL";
    }
    elsif ($field eq 'tag') {
        push @$joins, {
            table => 'bug_tag',
            as    => "bug_tag_$chart_id",
            from  => 'bug_id',
            to    => 'bug_id',
        };
        push @$joins, {
            table => 'tag',
            as    => "tag_$chart_id",
            from  => "bug_tag_$chart_id.tag_id",
            to    => 'id',
            extra => [ "tag_$chart_id.user_id = " . ($self->_sharer_id || $self->_user->id) ],
        };
        return "tag_$chart_id.id IS $not NULL";
    }
    elsif ($self->_multi_select_fields->{$field}) {
        push @$joins, {
            table => "bug_$field",
            as => "bug_${field}_$chart_id",
            from  => 'bug_id',
            to    => 'bug_id',
        };
        return "bug_${field}_$chart_id.bug_id IS $not NULL";
    }
}

###############################
# Standard Operator Functions #
###############################

sub _simple_operator {
    my ($self, $args) = @_;
    my ($full_field, $quoted, $operator) =
        @$args{qw(full_field quoted operator)};
    my $sql_operator = SIMPLE_OPERATORS->{$operator};
    $args->{term} = "$full_field $sql_operator $quoted";
}

sub _casesubstring {
    my ($self, $args) = @_;
    my ($full_field, $value) = @$args{qw(full_field value)};
    my $dbh = Bugzilla->dbh;

    $args->{term} = $dbh->sql_like($value, $full_field);
}

sub _substring {
    my ($self, $args) = @_;
    my ($full_field, $value) = @$args{qw(full_field value)};
    my $dbh = Bugzilla->dbh;

    $args->{term} = $dbh->sql_ilike($value, $full_field);
}

sub _notsubstring {
    my ($self, $args) = @_;
    my ($full_field, $value) = @$args{qw(full_field value)};
    my $dbh = Bugzilla->dbh;

    $args->{term} = $dbh->sql_not_ilike($value, $full_field);
}

sub _regexp {
    my ($self, $args) = @_;
    my ($full_field, $quoted) = @$args{qw(full_field quoted)};
    my $dbh = Bugzilla->dbh;
    
    $args->{term} = $dbh->sql_regexp($full_field, $quoted);
}

sub _notregexp {
    my ($self, $args) = @_;
    my ($full_field, $quoted) = @$args{qw(full_field quoted)};
    my $dbh = Bugzilla->dbh;
    
    $args->{term} = $dbh->sql_not_regexp($full_field, $quoted);
}

sub _anyexact {
    my ($self, $args) = @_;
    my ($field, $full_field) = @$args{qw(field full_field)};
    my $dbh = Bugzilla->dbh;
    
    my @list = $self->_all_values($args, ',');
    @list = map { $self->_quote_unless_numeric($args, $_) } @list;
    
    if (@list) {
        $args->{term} = $dbh->sql_in($full_field, \@list);
    }
    else {
        $args->{term} = '';
    }
}

sub _anywordsubstr {
    my ($self, $args) = @_;

    my @terms = $self->_substring_terms($args);
    $args->{term} = @terms ? '(' . join("\n\tOR ", @terms) . ')' : '';
}

sub _allwordssubstr {
    my ($self, $args) = @_;

    my @terms = $self->_substring_terms($args);
    $args->{term} = @terms ? '(' . join("\n\tAND ", @terms) . ')' : '';
}

sub _nowordssubstr {
    my ($self, $args) = @_;
    $self->_anywordsubstr($args);
    my $term = $args->{term};
    $args->{term} = "NOT($term)";
}

sub _anywords {
    my ($self, $args) = @_;

    my @terms = $self->_word_terms($args);
    # Because _word_terms uses AND, we need to parenthesize its terms
    # if there are more than one.
    @terms = map("($_)", @terms) if scalar(@terms) > 1;
    $args->{term} = @terms ? '(' . join("\n\tOR ", @terms) . ')' : '';
}

sub _allwords {
    my ($self, $args) = @_;

    my @terms = $self->_word_terms($args);
    $args->{term} = @terms ? '(' . join("\n\tAND ", @terms) . ')' : '';
}

sub _nowords {
    my ($self, $args) = @_;
    $self->_anywords($args);
    my $term = $args->{term};
    $args->{term} = "NOT($term)";
}

sub _changedbefore_changedafter {
    my ($self, $args) = @_;
    my ($chart_id, $joins, $field, $operator, $value) =
        @$args{qw(chart_id joins field operator value)};
    my $dbh = Bugzilla->dbh;

    my $field_object = $self->_chart_fields->{$field}
        || ThrowCodeError("invalid_field_name", { field => $field });
    
    # Asking when creation_ts changed is just asking when the bug was created.
    if ($field_object->name eq 'creation_ts') {
        $args->{operator} =
            $operator eq 'changedbefore' ? 'lessthaneq' : 'greaterthaneq';
        return $self->_do_operator_function($args);
    }
    
    my $sql_operator = ($operator =~ /before/) ? '<=' : '>=';
    my $field_id = $field_object->id;
    # Charts on changed* fields need to be field-specific. Otherwise,
    # OR chart rows make no sense if they contain multiple fields.
    my $table = "act_${field_id}_$chart_id";

    my $sql_date = $dbh->quote(SqlifyDate($value));
    my $join = {
        table => 'bugs_activity',
        as    => $table,
        extra => ["$table.fieldid = $field_id",
                  "$table.bug_when $sql_operator $sql_date"],
    };

    $args->{term} = "$table.bug_when IS NOT NULL";
    $self->_changed_security_check($args, $join);
    push(@$joins, $join);
}

sub _changedfrom_changedto {
    my ($self, $args) = @_;
    my ($chart_id, $joins, $field, $operator, $quoted) =
        @$args{qw(chart_id joins field operator quoted)};
    
    my $column = ($operator =~ /from/) ? 'removed' : 'added';
    my $field_object = $self->_chart_fields->{$field}
        || ThrowCodeError("invalid_field_name", { field => $field });
    my $field_id = $field_object->id;
    my $table = "act_${field_id}_$chart_id";
    my $join = {
        table => 'bugs_activity',
        as    => $table,
        extra => ["$table.fieldid = $field_id",
                  "$table.$column = $quoted"],
    };

    $args->{term} = "$table.bug_when IS NOT NULL";
    $self->_changed_security_check($args, $join);
    push(@$joins, $join);
}

sub _changedby {
    my ($self, $args) = @_;
    my ($chart_id, $joins, $field, $operator, $value) =
        @$args{qw(chart_id joins field operator value)};
    
    my $field_object = $self->_chart_fields->{$field}
        || ThrowCodeError("invalid_field_name", { field => $field });
    my $field_id = $field_object->id;
    my $table = "act_${field_id}_$chart_id";
    my $user_id  = $self->_get_user_id($value);
    my $join = {
        table => 'bugs_activity',
        as    => $table,
        extra => ["$table.fieldid = $field_id",
                  "$table.who = $user_id"],
    };

    $args->{term} = "$table.bug_when IS NOT NULL";
    $self->_changed_security_check($args, $join);
    push(@$joins, $join);
}

sub _changed_security_check {
    my ($self, $args, $join) = @_;
    my ($chart_id, $field) = @$args{qw(chart_id field)};

    my $field_object = $self->_chart_fields->{$field}
        || ThrowCodeError("invalid_field_name", { field => $field });
    my $field_id = $field_object->id;

    # If the user is not part of the insiders group, they cannot see
    # changes to attachments (including attachment flags) that are private
    if ($field =~ /^(?:flagtypes\.name$|attach)/ and !$self->_user->is_insider) {
        $join->{then_to} = {
            as    => "attach_${field_id}_$chart_id",
            table => 'attachments',
            from  => "act_${field_id}_$chart_id.attach_id",
            to    => 'attach_id',
        };

        $args->{term} .= " AND COALESCE(attach_${field_id}_$chart_id.isprivate, 0) = 0";
    }
}

sub _isempty {
    my ($self, $args) = @_;
    my $full_field = $args->{full_field};
    $args->{term} = "$full_field IS NULL OR $full_field = " . $self->_empty_value($args->{field});
}

sub _isnotempty {
    my ($self, $args) = @_;
    my $full_field = $args->{full_field};
    $args->{term} = "$full_field IS NOT NULL AND $full_field != " . $self->_empty_value($args->{field});
}

sub _empty_value {
    my ($self, $field) = @_;
    my $field_obj = $self->_chart_fields->{$field};
    return "0" if $field_obj->type == FIELD_TYPE_BUG_ID;
    return Bugzilla->dbh->quote(EMPTY_DATETIME) if $field_obj->type == FIELD_TYPE_DATETIME;
    return Bugzilla->dbh->quote(EMPTY_DATE) if $field_obj->type == FIELD_TYPE_DATE;
    return "''";
}

######################
# Public Subroutines #
######################

# Validate that the query type is one we can deal with
sub IsValidQueryType
{
    my ($queryType) = @_;
    if (grep { $_ eq $queryType } qw(specific advanced)) {
        return 1;
    }
    return 0;
}

# Splits out "asc|desc" from a sort order item.
sub split_order_term {
    my $fragment = shift;
    $fragment =~ /^(.+?)(?:\s+(ASC|DESC))?$/i;
    my ($column_name, $direction) = (lc($1), uc($2 || ''));
    return wantarray ? ($column_name, $direction) : $column_name;
}

# Used to translate old SQL fragments from buglist.cgi's "order" argument
# into our modern field IDs.
sub _translate_old_column {
    my ($self, $column) = @_;
    # All old SQL fragments have a period in them somewhere.
    return $column if $column !~ /\./;

    if ($column =~ /\bAS\s+(\w+)$/i) {
        return $1;
    }
    # product, component, classification, assigned_to, qa_contact, reporter
    elsif ($column =~ /map_(\w+?)s?\.(login_)?name/i) {
        return $1;
    }
    
    # If it doesn't match the regexps above, check to see if the old 
    # SQL fragment matches the SQL of an existing column
    foreach my $key (%{ $self->COLUMNS }) {
        next unless exists $self->COLUMNS->{$key}->{name};
        return $key if $self->COLUMNS->{$key}->{name} eq $column;
    }

    return $column;
}

1;

__END__

=head1 NAME

Bugzilla::Search - Provides methods to run queries against bugs.

=head1 SYNOPSIS

    use Bugzilla::Search;

    my $search = new Bugzilla::Search({'fields' => \@fields,
                                       'params' => \%search_criteria,
                                       'sharer' => $sharer_id,
                                       'user'   => $user_obj,
                                       'allow_unlimited' => 1});

    my $data = $search->data;
    my ($data, $extra_data) = $search->data;

=head1 DESCRIPTION

Search.pm represents a search object. It's the single way to collect
data about bugs in a secure way. The list of bugs matching criteria
defined by the caller are filtered based on the user privileges.

=head1 METHODS

=head2 new

=over

=item B<Description>

Create a Bugzilla::Search object.

=item B<Params>

=over

=item C<fields>

An arrayref representing the bug attributes for which data is desired.
Legal attributes are listed in the fielddefs DB table. At least one field
must be defined, typically the 'bug_id' field.

=item C<params>

A hashref representing search criteria. Each key => value pair represents
a search criteria, where the key is the search field and the value is the
value for this field. At least one search criteria must be defined if the
'search_allow_no_criteria' parameter is turned off, else an error is thrown.

=item C<sharer>

When a saved search is shared by a user, this is their user ID.

=item C<user>

A L<Bugzilla::User> object representing the user to whom the data is addressed.
All security checks are done based on this user object, so it's not safe
to share results of the query with other users as not all users have the
same privileges or have the same role for all bugs in the list. If this
parameter is not defined, then the currently logged in user is taken into
account. If no user is logged in, then only public bugs will be returned.

=item C<allow_unlimited>

If set to a true value, the number of bugs retrieved by the query is not
limited.

=back

=item B<Returns>

A L<Bugzilla::Search> object.

=back

=head2 data

=over

=item B<Description>

Returns bugs matching search criteria passed to C<new()>.

=item B<Params>

None

=item B<Returns>

In scalar context, this method returns a reference to a list of bugs.
Each item of the list represents a bug, which is itself a reference to
a list where each item represents a bug attribute, in the same order as
specified in the C<fields> parameter of C<new()>.

In list context, this methods also returns a reference to a list containing
references to hashes. For each hash, two keys are defined: C<sql> contains
the SQL query which has been executed, and C<time> contains the time spent
to execute the SQL query, in seconds. There can be either a single hash, or
two hashes if two SQL queries have been executed sequentially to get all the
required data.

=back

=head1 B<Methods in need of POD>

=over

=item invalid_order_columns

=item COLUMN_JOINS

=item split_order_term

=item SqlifyDate

=item REPORT_COLUMNS

=item pronoun

=item COLUMNS

=item order

=item search_description

=item IsValidQueryType

=item build_subselect

=item do_search_function

=item boolean_charts_to_custom_search

=back
