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

package Bugzilla::Constants;
use strict;
use base qw(Exporter);

@Bugzilla::Constants::EXPORT = qw(
    CONTROLMAPNA
    CONTROLMAPSHOWN
    CONTROLMAPDEFAULT
    CONTROLMAPMANDATORY

    AUTH_OK
    AUTH_NODATA
    AUTH_ERROR
    AUTH_LOGINFAILED
    AUTH_DISABLED

    LOGIN_OPTIONAL
    LOGIN_NORMAL
    LOGIN_REQUIRED

    LOGOUT_ALL
    LOGOUT_CURRENT
    LOGOUT_KEEP_CURRENT

    GRANT_DIRECT
    GRANT_DERIVED
    GRANT_REGEXP

    GROUP_MEMBERSHIP
    GROUP_BLESS
    GROUP_VISIBLE

    MAILTO_USER
    MAILTO_GROUP

    DEFAULT_COLUMN_LIST
    DEFAULT_QUERY_NAME

    COMMENT_COLS

    DERIVE_GROUPS_TABLES_ALREADY_LOCKED

    UNLOCK_ABORT
    
    RELATIONSHIPS
    REL_ASSIGNEE REL_QA REL_REPORTER REL_CC REL_VOTER 
    REL_ANY
    
    POS_EVENTS
    EVT_OTHER EVT_ADDED_REMOVED EVT_COMMENT EVT_ATTACHMENT EVT_ATTACHMENT_DATA
    EVT_PROJ_MANAGEMENT EVT_OPENED_CLOSED EVT_KEYWORD EVT_CC 
    
    NEG_EVENTS
    EVT_UNCONFIRMED EVT_CHANGED_BY_ME 
        
    GLOBAL_EVENTS
    EVT_FLAG_REQUESTED EVT_REQUESTED_FLAG

    FULLTEXT_BUGLIST_LIMIT

    SENDMAIL_EXE
);

@Bugzilla::Constants::EXPORT_OK = qw(contenttypes);

# CONSTANTS
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

use constant LOGIN_OPTIONAL => 0;
use constant LOGIN_NORMAL => 1;
use constant LOGIN_REQUIRED => 2;

use constant LOGOUT_ALL => 0;
use constant LOGOUT_CURRENT => 1;
use constant LOGOUT_KEEP_CURRENT => 2;

use constant contenttypes =>
  {
   "html"=> "text/html" , 
   "rdf" => "application/rdf+xml" , 
   "atom"=> "application/atom+xml" ,
   "xml" => "application/xml" , 
   "js"  => "application/x-javascript" , 
   "csv" => "text/plain" ,
   "png" => "image/png" ,
   "ics" => "text/calendar" ,
  };

use constant GRANT_DIRECT => 0;
use constant GRANT_DERIVED => 1;
use constant GRANT_REGEXP => 2;

use constant GROUP_MEMBERSHIP => 0;
use constant GROUP_BLESS => 1;
use constant GROUP_VISIBLE => 2;

use constant MAILTO_USER => 0;
use constant MAILTO_GROUP => 1;

# The default list of columns for buglist.cgi
use constant DEFAULT_COLUMN_LIST => (
    "bug_severity", "priority", "rep_platform","assigned_to",
    "bug_status", "resolution", "short_short_desc"
);

# Used by query.cgi and buglist.cgi as the named-query name
# for the default settings.
use constant DEFAULT_QUERY_NAME => '(Default query)';

# The column length for displayed (and wrapped) bug comments.
use constant COMMENT_COLS => 80;

# Used to indicate to User::new and User::new_from_login calls
# that the derive_groups tables are already locked
use constant DERIVE_GROUPS_TABLES_ALREADY_LOCKED => 1;

# used by Bugzilla::DB to indicate that tables are being unlocked
# because of error
use constant UNLOCK_ABORT => 1;

use constant REL_ASSIGNEE           => 0;
use constant REL_QA                 => 1;
use constant REL_REPORTER           => 2;
use constant REL_CC                 => 3;
use constant REL_VOTER              => 4;

use constant RELATIONSHIPS => REL_ASSIGNEE, REL_QA, REL_REPORTER, REL_CC, 
                              REL_VOTER;
                              
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

use constant POS_EVENTS => EVT_OTHER, EVT_ADDED_REMOVED, EVT_COMMENT, 
                           EVT_ATTACHMENT, EVT_ATTACHMENT_DATA, 
                           EVT_PROJ_MANAGEMENT, EVT_OPENED_CLOSED, EVT_KEYWORD,
                           EVT_CC;

use constant EVT_UNCONFIRMED        => 50;
use constant EVT_CHANGED_BY_ME      => 51;

use constant NEG_EVENTS => EVT_UNCONFIRMED, EVT_CHANGED_BY_ME;

# These are the "global" flags, which aren't tied to a particular relationship.
# and so use REL_ANY.
use constant EVT_FLAG_REQUESTED     => 100; # Flag has been requested of me
use constant EVT_REQUESTED_FLAG     => 101; # I have requested a flag

use constant GLOBAL_EVENTS => EVT_FLAG_REQUESTED, EVT_REQUESTED_FLAG;

#  Number of bugs to return in a buglist when performing
#  a fulltext search.
use constant FULLTEXT_BUGLIST_LIMIT => 200;

# Path to sendmail.exe (Windows only)
use constant SENDMAIL_EXE => '/usr/lib/sendmail.exe';

1;
