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
#                 Dawn Endico <endico@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Joe Robins <jmrobins@tgix.com>
#                 Jake <jake@bugzilla.org>
#                 J. Paul Reed <preed@sigkill.com>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Christopher Aillon <christopher@aillon.com>
#                 Shane H. W. Travis <travis@sedsystems.ca>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Marc Schumann <wurblzap@gmail.com>

package Bugzilla::Constants;
use strict;
use base qw(Exporter);

# For bz_locations
use File::Basename;
use Memoize;

@Bugzilla::Constants::EXPORT = qw(
    BUGZILLA_VERSION

    REMOTE_FILE
    LOCAL_FILE

    bz_locations

    IS_NULL
    NOT_NULL

    CONTROLMAPNA
    CONTROLMAPSHOWN
    CONTROLMAPDEFAULT
    CONTROLMAPMANDATORY

    AUTH_OK
    AUTH_NODATA
    AUTH_ERROR
    AUTH_LOGINFAILED
    AUTH_DISABLED
    AUTH_NO_SUCH_USER
    AUTH_LOCKOUT

    USER_PASSWORD_MIN_LENGTH

    LOGIN_OPTIONAL
    LOGIN_NORMAL
    LOGIN_REQUIRED

    LOGOUT_ALL
    LOGOUT_CURRENT
    LOGOUT_KEEP_CURRENT

    GRANT_DIRECT
    GRANT_REGEXP

    GROUP_MEMBERSHIP
    GROUP_BLESS
    GROUP_VISIBLE

    MAILTO_USER
    MAILTO_GROUP

    DEFAULT_COLUMN_LIST
    DEFAULT_QUERY_NAME
    DEFAULT_MILESTONE

    SAVE_NUM_SEARCHES

    COMMENT_COLS
    COMMENT_COLS_WRAP
    MAX_COMMENT_LENGTH

    CMT_NORMAL
    CMT_DUPE_OF
    CMT_HAS_DUPE
    CMT_ATTACHMENT_CREATED
    CMT_ATTACHMENT_UPDATED

    THROW_ERROR
    
    RELATIONSHIPS
    REL_ASSIGNEE REL_QA REL_REPORTER REL_CC REL_GLOBAL_WATCHER
    REL_ANY
    
    POS_EVENTS
    EVT_OTHER EVT_ADDED_REMOVED EVT_COMMENT EVT_ATTACHMENT EVT_ATTACHMENT_DATA
    EVT_PROJ_MANAGEMENT EVT_OPENED_CLOSED EVT_KEYWORD EVT_CC EVT_DEPEND_BLOCK
    EVT_BUG_CREATED

    NEG_EVENTS
    EVT_UNCONFIRMED EVT_CHANGED_BY_ME 
        
    GLOBAL_EVENTS
    EVT_FLAG_REQUESTED EVT_REQUESTED_FLAG

    ADMIN_GROUP_NAME
    PER_PRODUCT_PRIVILEGES

    SENDMAIL_EXE
    SENDMAIL_PATH

    FIELD_TYPE_UNKNOWN
    FIELD_TYPE_FREETEXT
    FIELD_TYPE_SINGLE_SELECT
    FIELD_TYPE_MULTI_SELECT
    FIELD_TYPE_TEXTAREA
    FIELD_TYPE_DATETIME
    FIELD_TYPE_BUG_ID
    FIELD_TYPE_BUG_URLS
    FIELD_TYPE_KEYWORDS

    EMPTY_DATETIME_REGEX

    ABNORMAL_SELECTS

    TIMETRACKING_FIELDS

    USAGE_MODE_BROWSER
    USAGE_MODE_CMDLINE
    USAGE_MODE_XMLRPC
    USAGE_MODE_EMAIL
    USAGE_MODE_JSON
    USAGE_MODE_TEST

    ERROR_MODE_WEBPAGE
    ERROR_MODE_DIE
    ERROR_MODE_DIE_SOAP_FAULT
    ERROR_MODE_JSON_RPC
    ERROR_MODE_TEST

    COLOR_ERROR
    COLOR_SUCCESS

    INSTALLATION_MODE_INTERACTIVE
    INSTALLATION_MODE_NON_INTERACTIVE

    DB_MODULE
    ROOT_USER
    ON_WINDOWS
    ON_ACTIVESTATE

    MAX_TOKEN_AGE
    MAX_LOGINCOOKIE_AGE
    MAX_SUDO_TOKEN_AGE
    MAX_LOGIN_ATTEMPTS
    LOGIN_LOCKOUT_INTERVAL
    MAX_STS_AGE

    SAFE_PROTOCOLS
    LEGAL_CONTENT_TYPES

    MIN_SMALLINT
    MAX_SMALLINT
    MAX_INT_32

    MAX_LEN_QUERY_NAME
    MAX_CLASSIFICATION_SIZE
    MAX_PRODUCT_SIZE
    MAX_MILESTONE_SIZE
    MAX_COMPONENT_SIZE
    MAX_FIELD_VALUE_SIZE
    MAX_FREETEXT_LENGTH
    MAX_BUG_URL_LENGTH
    MAX_POSSIBLE_DUPLICATES

    PASSWORD_DIGEST_ALGORITHM
    PASSWORD_SALT_LENGTH
    
    CGI_URI_LIMIT

    PRIVILEGES_REQUIRED_NONE
    PRIVILEGES_REQUIRED_REPORTER
    PRIVILEGES_REQUIRED_ASSIGNEE
    PRIVILEGES_REQUIRED_EMPOWERED

    AUDIT_CREATE
    AUDIT_REMOVE
);

@Bugzilla::Constants::EXPORT_OK = qw(contenttypes);

# CONSTANTS
#
# Bugzilla version
use constant BUGZILLA_VERSION => "4.2.7";

# Location of the remote and local XML files to track new releases.
use constant REMOTE_FILE => 'http://updates.bugzilla.org/bugzilla-update.xml';
use constant LOCAL_FILE  => 'bugzilla-update.xml'; # Relative to datadir.

# These are unique values that are unlikely to match a string or a number,
# to be used in criteria for match() functions and other things. They start
# and end with spaces because most Bugzilla stuff has trim() called on it,
# so this is unlikely to match anything we get out of the DB.
#
# We can't use a reference, because Template Toolkit doesn't work with
# them properly (constants.IS_NULL => {} just returns an empty string instead
# of the reference).
use constant IS_NULL  => '  __IS_NULL__  ';
use constant NOT_NULL => '  __NOT_NULL__  ';

#
# ControlMap constants for group_control_map.
# membercontol:othercontrol => meaning
# Na:Na               => Bugs in this product may not be restricted to this 
#                        group.
# Shown:Na            => Members of the group may restrict bugs 
#                        in this product to this group.
# Shown:Shown         => Members of the group may restrict bugs
#                        in this product to this group.
#                        Anyone who can enter bugs in this product may initially
#                        restrict bugs in this product to this group.
# Shown:Mandatory     => Members of the group may restrict bugs
#                        in this product to this group.
#                        Non-members who can enter bug in this product
#                        will be forced to restrict it.
# Default:Na          => Members of the group may restrict bugs in this
#                        product to this group and do so by default.
# Default:Default     => Members of the group may restrict bugs in this
#                        product to this group and do so by default and
#                        nonmembers have this option on entry.
# Default:Mandatory   => Members of the group may restrict bugs in this
#                        product to this group and do so by default.
#                        Non-members who can enter bug in this product
#                        will be forced to restrict it.
# Mandatory:Mandatory => Bug will be forced into this group regardless.
# All other combinations are illegal.

use constant CONTROLMAPNA => 0;
use constant CONTROLMAPSHOWN => 1;
use constant CONTROLMAPDEFAULT => 2;
use constant CONTROLMAPMANDATORY => 3;

# See Bugzilla::Auth for docs on AUTH_*, LOGIN_* and LOGOUT_*

use constant AUTH_OK => 0;
use constant AUTH_NODATA => 1;
use constant AUTH_ERROR => 2;
use constant AUTH_LOGINFAILED => 3;
use constant AUTH_DISABLED => 4;
use constant AUTH_NO_SUCH_USER  => 5;
use constant AUTH_LOCKOUT => 6;

# The minimum length a password must have.
use constant USER_PASSWORD_MIN_LENGTH => 6;

use constant LOGIN_OPTIONAL => 0;
use constant LOGIN_NORMAL => 1;
use constant LOGIN_REQUIRED => 2;

use constant LOGOUT_ALL => 0;
use constant LOGOUT_CURRENT => 1;
use constant LOGOUT_KEEP_CURRENT => 2;

use constant GRANT_DIRECT => 0;
use constant GRANT_REGEXP => 2;

use constant GROUP_MEMBERSHIP => 0;
use constant GROUP_BLESS => 1;
use constant GROUP_VISIBLE => 2;

use constant MAILTO_USER => 0;
use constant MAILTO_GROUP => 1;

# The default list of columns for buglist.cgi
use constant DEFAULT_COLUMN_LIST => (
    "product", "component", "assigned_to",
    "bug_status", "resolution", "short_desc", "changeddate"
);

# Used by query.cgi and buglist.cgi as the named-query name
# for the default settings.
use constant DEFAULT_QUERY_NAME => '(Default query)';

# The default "defaultmilestone" created for products.
use constant DEFAULT_MILESTONE => '---';

# How many of the user's most recent searches to save.
use constant SAVE_NUM_SEARCHES => 10;

# The column width for comment textareas and comments in bugmails.
use constant COMMENT_COLS => 80;
#if WEBKIT_CHANGES
# The column width at which to wrap comments prior to display -- using
# Perl's Text::Wrap::wrap().  Only Bugzilla/Util.pm's wrap_comment() method
# uses this constant.
#
# Empirical testing showed that $Text::Wrap::columns must be a value
# between 2 and 32766, inclusive.  We choose a large value to prevent
# wrap() from inserting any hard line breaks at all.  This lets us use
# "white-space: pre-wrap" in the CSS to do line-wrapping instead.  We
# arbitrarily choose 8000, which is enough for a 100-line paragraph.
use constant COMMENT_COLS_WRAP => 8000;
#endif // WEBKIT_CHANGES
# Used in _check_comment(). Gives the max length allowed for a comment.
use constant MAX_COMMENT_LENGTH => 65535;

# The type of bug comments.
use constant CMT_NORMAL => 0;
use constant CMT_DUPE_OF => 1;
use constant CMT_HAS_DUPE => 2;
# Type 3 was CMT_POPULAR_VOTES, which moved to the Voting extension.
# Type 4 was CMT_MOVED_TO, which moved to the OldBugMove extension.
use constant CMT_ATTACHMENT_CREATED => 5;
use constant CMT_ATTACHMENT_UPDATED => 6;

# Determine whether a validation routine should return 0 or throw
# an error when the validation fails.
use constant THROW_ERROR => 1;

use constant REL_ASSIGNEE           => 0;
use constant REL_QA                 => 1;
use constant REL_REPORTER           => 2;
use constant REL_CC                 => 3;
# REL 4 was REL_VOTER, before it was moved ino an extension.
use constant REL_GLOBAL_WATCHER     => 5;

# We need these strings for the X-Bugzilla-Reasons header
# Note: this hash uses "," rather than "=>" to avoid auto-quoting of the LHS.
# This should be accessed through Bugzilla::BugMail::relationships() instead
# of being accessed directly.
use constant RELATIONSHIPS => {
    REL_ASSIGNEE      , "AssignedTo",
    REL_REPORTER      , "Reporter",
    REL_QA            , "QAcontact",
    REL_CC            , "CC",
    REL_GLOBAL_WATCHER, "GlobalWatcher"
};
                              
# Used for global events like EVT_FLAG_REQUESTED
use constant REL_ANY                => 100;

# There are two sorts of event - positive and negative. Positive events are
# those for which the user says "I want mail if this happens." Negative events
# are those for which the user says "I don't want mail if this happens."
#
# Exactly when each event fires is defined in wants_bug_mail() in User.pm; I'm
# not commenting them here in case the comments and the code get out of sync.
use constant EVT_OTHER              => 0;
use constant EVT_ADDED_REMOVED      => 1;
use constant EVT_COMMENT            => 2;
use constant EVT_ATTACHMENT         => 3;
use constant EVT_ATTACHMENT_DATA    => 4;
use constant EVT_PROJ_MANAGEMENT    => 5;
use constant EVT_OPENED_CLOSED      => 6;
use constant EVT_KEYWORD            => 7;
use constant EVT_CC                 => 8;
use constant EVT_DEPEND_BLOCK       => 9;
use constant EVT_BUG_CREATED        => 10;

use constant POS_EVENTS => EVT_OTHER, EVT_ADDED_REMOVED, EVT_COMMENT, 
                           EVT_ATTACHMENT, EVT_ATTACHMENT_DATA, 
                           EVT_PROJ_MANAGEMENT, EVT_OPENED_CLOSED, EVT_KEYWORD,
                           EVT_CC, EVT_DEPEND_BLOCK, EVT_BUG_CREATED;

use constant EVT_UNCONFIRMED        => 50;
use constant EVT_CHANGED_BY_ME      => 51;

use constant NEG_EVENTS => EVT_UNCONFIRMED, EVT_CHANGED_BY_ME;

# These are the "global" flags, which aren't tied to a particular relationship.
# and so use REL_ANY.
use constant EVT_FLAG_REQUESTED     => 100; # Flag has been requested of me
use constant EVT_REQUESTED_FLAG     => 101; # I have requested a flag

use constant GLOBAL_EVENTS => EVT_FLAG_REQUESTED, EVT_REQUESTED_FLAG;

# Default administration group name.
use constant ADMIN_GROUP_NAME => 'admin';

# Privileges which can be per-product.
use constant PER_PRODUCT_PRIVILEGES => ('editcomponents', 'editbugs', 'canconfirm');

# Path to sendmail.exe (Windows only)
use constant SENDMAIL_EXE => '/usr/lib/sendmail.exe';
# Paths to search for the sendmail binary (non-Windows)
use constant SENDMAIL_PATH => '/usr/lib:/usr/sbin:/usr/ucblib';

# Field types.  Match values in fielddefs.type column.  These are purposely
# not named after database column types, since Bugzilla fields comprise not
# only storage but also logic.  For example, we might add a "user" field type
# whose values are stored in an integer column in the database but for which
# we do more than we would do for a standard integer type (f.e. we might
# display a user picker).

use constant FIELD_TYPE_UNKNOWN   => 0;
use constant FIELD_TYPE_FREETEXT  => 1;
use constant FIELD_TYPE_SINGLE_SELECT => 2;
use constant FIELD_TYPE_MULTI_SELECT => 3;
use constant FIELD_TYPE_TEXTAREA  => 4;
use constant FIELD_TYPE_DATETIME  => 5;
use constant FIELD_TYPE_BUG_ID  => 6;
use constant FIELD_TYPE_BUG_URLS => 7;
use constant FIELD_TYPE_KEYWORDS => 8;

use constant EMPTY_DATETIME_REGEX => qr/^[0\-:\sA-Za-z]+$/; 

# See the POD for Bugzilla::Field/is_abnormal to see why these are listed
# here.
use constant ABNORMAL_SELECTS => {
    classification => 1,
    component      => 1,
    product        => 1,
};

# The fields from fielddefs that are blocked from non-timetracking users.
# work_time is sometimes called actual_time.
use constant TIMETRACKING_FIELDS =>
    qw(estimated_time remaining_time work_time actual_time
       percentage_complete deadline);

# The maximum number of days a token will remain valid.
use constant MAX_TOKEN_AGE => 3;
# How many days a logincookie will remain valid if not used.
use constant MAX_LOGINCOOKIE_AGE => 30;
# How many seconds (default is 6 hours) a sudo cookie remains valid.
use constant MAX_SUDO_TOKEN_AGE => 21600;

# Maximum failed logins to lock account for this IP
use constant MAX_LOGIN_ATTEMPTS => 5;
# If the maximum login attempts occur during this many minutes, the
# account is locked.
use constant LOGIN_LOCKOUT_INTERVAL => 30;

# The maximum number of seconds the Strict-Transport-Security header
# will remain valid. Default is one week.
use constant MAX_STS_AGE => 604800;

# Protocols which are considered as safe.
use constant SAFE_PROTOCOLS => ('afs', 'cid', 'ftp', 'gopher', 'http', 'https',
                                'irc', 'ircs', 'mid', 'news', 'nntp', 'prospero',
                                'telnet', 'view-source', 'wais');

# Valid MIME types for attachments.
use constant LEGAL_CONTENT_TYPES => ('application', 'audio', 'image', 'message',
                                     'model', 'multipart', 'text', 'video');

use constant contenttypes =>
  {
   "html"=> "text/html" ,
   "rdf" => "application/rdf+xml" ,
   "atom"=> "application/atom+xml" ,
   "xml" => "application/xml" ,
   "js"  => "application/x-javascript" ,
   "json"=> "application/json" ,
   "csv" => "text/csv" ,
   "png" => "image/png" ,
   "ics" => "text/calendar" ,
  };

# Usage modes. Default USAGE_MODE_BROWSER. Use with Bugzilla->usage_mode.
use constant USAGE_MODE_BROWSER    => 0;
use constant USAGE_MODE_CMDLINE    => 1;
use constant USAGE_MODE_XMLRPC     => 2;
use constant USAGE_MODE_EMAIL      => 3;
use constant USAGE_MODE_JSON       => 4;
use constant USAGE_MODE_TEST       => 5;

# Error modes. Default set by Bugzilla->usage_mode (so ERROR_MODE_WEBPAGE
# usually). Use with Bugzilla->error_mode.
use constant ERROR_MODE_WEBPAGE        => 0;
use constant ERROR_MODE_DIE            => 1;
use constant ERROR_MODE_DIE_SOAP_FAULT => 2;
use constant ERROR_MODE_JSON_RPC       => 3;
use constant ERROR_MODE_TEST           => 4;

# The ANSI colors of messages that command-line scripts use
use constant COLOR_ERROR => 'red';
use constant COLOR_SUCCESS => 'green';

# The various modes that checksetup.pl can run in.
use constant INSTALLATION_MODE_INTERACTIVE => 0;
use constant INSTALLATION_MODE_NON_INTERACTIVE => 1;

# Data about what we require for different databases.
use constant DB_MODULE => {
    # MySQL 5.0.15 was the first production 5.0.x release.
    'mysql' => {db => 'Bugzilla::DB::Mysql', db_version => '5.0.15',
                dbd => { 
                    package => 'DBD-mysql',
                    module  => 'DBD::mysql',
                    # Disallow development versions
                    blacklist => ['_'],
                    # For UTF-8 support. 4.001 makes sure that blobs aren't
                    # marked as UTF-8.
                    version => '4.001',
                },
                name => 'MySQL'},
    # Also see Bugzilla::DB::Pg::bz_check_server_version, which has special
    # code to require DBD::Pg 2.17.2 for PostgreSQL 9 and above.
    'pg'    => {db => 'Bugzilla::DB::Pg', db_version => '8.03.0000',
                dbd => {
                    package => 'DBD-Pg',
                    module  => 'DBD::Pg',
                    version => '1.45',
                },
                name => 'PostgreSQL'},
     'oracle'=> {db => 'Bugzilla::DB::Oracle', db_version => '10.02.0',
                dbd => {
                     package => 'DBD-Oracle',
                     module  => 'DBD::Oracle',
                     version => '1.19',
                },
                name => 'Oracle'},
     # SQLite 3.6.22 fixes a WHERE clause problem that may affect us.
    sqlite => {db => 'Bugzilla::DB::Sqlite', db_version => '3.6.22',
               dbd => {
                   package => 'DBD-SQLite',
                   module  => 'DBD::SQLite',
                   # 1.29 is the version that contains 3.6.22.
                   version => '1.29',
               },
               name => 'SQLite'},
};

# True if we're on Win32.
use constant ON_WINDOWS => ($^O =~ /MSWin32/i) ? 1 : 0;
# True if we're using ActiveState Perl (as opposed to Strawberry) on Windows.
use constant ON_ACTIVESTATE => eval { &Win32::BuildNumber };

# The user who should be considered "root" when we're giving
# instructions to Bugzilla administrators.
use constant ROOT_USER => ON_WINDOWS ? 'Administrator' : 'root';

use constant MIN_SMALLINT => -32768;
use constant MAX_SMALLINT => 32767;
use constant MAX_INT_32 => 2147483647;

# The longest that a saved search name can be.
use constant MAX_LEN_QUERY_NAME => 64;

# The longest classification name allowed.
use constant MAX_CLASSIFICATION_SIZE => 64;

# The longest product name allowed.
use constant MAX_PRODUCT_SIZE => 64;

# The longest milestone name allowed.
use constant MAX_MILESTONE_SIZE => 20;

# The longest component name allowed.
use constant MAX_COMPONENT_SIZE => 64;

# The maximum length for values of <select> fields.
use constant MAX_FIELD_VALUE_SIZE => 64;

# Maximum length allowed for free text fields.
use constant MAX_FREETEXT_LENGTH => 255;

# The longest a bug URL in a BUG_URLS field can be.
use constant MAX_BUG_URL_LENGTH => 255;

# The largest number of possible duplicates that Bug::possible_duplicates
# will return.
use constant MAX_POSSIBLE_DUPLICATES => 25;

# This is the name of the algorithm used to hash passwords before storing
# them in the database. This can be any string that is valid to pass to
# Perl's "Digest" module. Note that if you change this, it won't take
# effect until a user changes his password.
use constant PASSWORD_DIGEST_ALGORITHM => 'SHA-256';
# How long of a salt should we use? Note that if you change this, none
# of your users will be able to log in until they reset their passwords.
use constant PASSWORD_SALT_LENGTH => 8;

# Certain scripts redirect to GET even if the form was submitted originally
# via POST such as buglist.cgi. This value determines whether the redirect
# can be safely done or not based on the web server's URI length setting.
use constant CGI_URI_LIMIT => 8000;

# If the user isn't allowed to change a field, we must tell him who can.
# We store the required permission set into the $PrivilegesRequired
# variable which gets passed to the error template.

use constant PRIVILEGES_REQUIRED_NONE      => 0;
use constant PRIVILEGES_REQUIRED_REPORTER  => 1;
use constant PRIVILEGES_REQUIRED_ASSIGNEE  => 2;
use constant PRIVILEGES_REQUIRED_EMPOWERED => 3;

# Special field values used in the audit_log table to mean either
# "we just created this object" or "we just deleted this object".
use constant AUDIT_CREATE => '__create__';
use constant AUDIT_REMOVE => '__remove__';

sub bz_locations {
    # We know that Bugzilla/Constants.pm must be in %INC at this point.
    # So the only question is, what's the name of the directory
    # above it? This is the most reliable way to get our current working
    # directory under both mod_cgi and mod_perl. We call dirname twice
    # to get the name of the directory above the "Bugzilla/" directory.
    #
    # Calling dirname twice like that won't work on VMS or AmigaOS
    # but I doubt anybody runs Bugzilla on those.
    #
    # On mod_cgi this will be a relative path. On mod_perl it will be an
    # absolute path.
    my $libpath = dirname(dirname($INC{'Bugzilla/Constants.pm'}));
    # We have to detaint $libpath, but we can't use Bugzilla::Util here.
    $libpath =~ /(.*)/;
    $libpath = $1;

    my ($project, $localconfig, $datadir);
    if ($ENV{'PROJECT'} && $ENV{'PROJECT'} =~ /^(\w+)$/) {
        $project = $1;
        $localconfig = "localconfig.$project";
        $datadir = "data/$project";
    } else {
        $localconfig = "localconfig";
        $datadir = "data";
    }

    $datadir = "$libpath/$datadir";
    # We have to return absolute paths for mod_perl. 
    # That means that if you modify these paths, they must be absolute paths.
    return {
        'libpath'     => $libpath,
        'ext_libpath' => "$libpath/lib",
        # If you put the libraries in a different location than the CGIs,
        # make sure this still points to the CGIs.
        'cgi_path'    => $libpath,
        'templatedir' => "$libpath/template",
        'template_cache' => "$datadir/template",
        'project'     => $project,
        'localconfig' => "$libpath/$localconfig",
        'datadir'     => $datadir,
        'attachdir'   => "$datadir/attachments",
        'skinsdir'    => "$libpath/skins",
        'graphsdir'   => "$libpath/graphs",
        # $webdotdir must be in the web server's tree somewhere. Even if you use a 
        # local dot, we output images to there. Also, if $webdotdir is 
        # not relative to the bugzilla root directory, you'll need to 
        # change showdependencygraph.cgi to set image_url to the correct 
        # location.
        # The script should really generate these graphs directly...
        'webdotdir'   => "$datadir/webdot",
        'extensionsdir' => "$libpath/extensions",
    };
}

# This makes us not re-compute all the bz_locations data every time it's
# called.
BEGIN { memoize('bz_locations') };

1;
