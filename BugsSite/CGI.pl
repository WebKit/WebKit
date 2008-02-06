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
#                 Joe Robins <jmrobins@tgix.com>
#                 Dave Miller <justdave@syndicomm.com>
#                 Christopher Aillon <christopher@aillon.com>
#                 Gervase Markham <gerv@gerv.net>
#                 Christian Reis <kiko@async.com.br>

# Contains some global routines used throughout the CGI scripts of Bugzilla.

use strict;
use lib ".";

# use Carp;                       # for confess

BEGIN {
    if ($^O =~ /MSWin32/i) {
        # Help CGI find the correct temp directory as the default list
        # isn't Windows friendly (Bug 248988)
        $ENV{'TMPDIR'} = $ENV{'TEMP'} || $ENV{'TMP'} || "$ENV{'WINDIR'}\\TEMP";
    }
}

use Bugzilla::Util;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::BugMail;
use Bugzilla::Bug;
use Bugzilla::User;

# Used in LogActivityEntry(). Gives the max length of lines in the
# activity table.
use constant MAX_LINE_LENGTH => 254;

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub CGI_pl_sillyness {
    my $zz;
    $zz = $::buffer;
}

use CGI::Carp qw(fatalsToBrowser);

require 'globals.pl';

use vars qw($template $vars);

# If Bugzilla is shut down, do not go any further, just display a message
# to the user about the downtime and log out.  (do)editparams.cgi is exempted
# from this message, of course, since it needs to be available in order for
# the administrator to open Bugzilla back up.
if (Param("shutdownhtml") && $0 !~ m:(^|[\\/])(do)?editparams\.cgi$:) {
    # For security reasons, log out users when Bugzilla is down.
    # Bugzilla->login() is required to catch the logincookie, if any.
    my $user = Bugzilla->login(LOGIN_OPTIONAL);
    my $userid = $user->id;
    Bugzilla->logout();
    
    # Return the appropriate HTTP response headers.
    print Bugzilla->cgi->header();
    
    $::vars->{'message'} = "shutdown";
    $::vars->{'userid'} = $userid;
    # Generate and return an HTML message about the downtime.
    $::template->process("global/message.html.tmpl", $::vars)
      || ThrowTemplateError($::template->error());
    exit;
}

# Implementations of several of the below were blatently stolen from CGI.pm,
# by Lincoln D. Stein.

# Get rid of all the %xx encoding and the like from the given URL.
sub url_decode {
    my ($todecode) = (@_);
    $todecode =~ tr/+/ /;       # pluses become spaces
    $todecode =~ s/%([0-9a-fA-F]{2})/pack("c",hex($1))/ge;
    return $todecode;
}

# check and see if a given field exists, is non-empty, and is set to a 
# legal value.  assume a browser bug and abort appropriately if not.
# if $legalsRef is not passed, just check to make sure the value exists and 
# is non-NULL
sub CheckFormField ($$;\@) {
    my ($cgi,                    # a CGI object
        $fieldname,              # the fieldname to check
        $legalsRef               # (optional) ref to a list of legal values 
       ) = @_;

    if (!defined $cgi->param($fieldname)
        || trim($cgi->param($fieldname)) eq ""
        || (defined($legalsRef)
            && lsearch($legalsRef, $cgi->param($fieldname))<0))
    {
        SendSQL("SELECT description FROM fielddefs WHERE name=" . SqlQuote($fieldname));
        my $result = FetchOneColumn();
        my $field;
        if ($result) {
            $field = $result;
        }
        else {
            $field = $fieldname;
        }
        
        ThrowCodeError("illegal_field", { field => $field });
    }
}

# check and see if a given field is defined, and abort if not
sub CheckFormFieldDefined ($$) {
    my ($cgi,                    # a CGI object
        $fieldname,              # the fieldname to check
       ) = @_;

    if (!defined $cgi->param($fieldname)) {
        ThrowCodeError("undefined_field", { field => $fieldname });
    }
}

sub ValidateBugID {
    # Validates and verifies a bug ID, making sure the number is a 
    # positive integer, that it represents an existing bug in the
    # database, and that the user is authorized to access that bug.
    # We detaint the number here, too

    my ($id, $field) = @_;
    
    # Get rid of white-space around the ID.
    $id = trim($id);
    
    # If the ID isn't a number, it might be an alias, so try to convert it.
    my $alias = $id;
    if (!detaint_natural($id)) {
        $id = bug_alias_to_id($alias);
        $id || ThrowUserError("invalid_bug_id_or_alias",
                              {'bug_id' => $alias,
                               'field'  => $field });
    }
    
    # Modify the calling code's original variable to contain the trimmed,
    # converted-from-alias ID.
    $_[0] = $id;
    
    # First check that the bug exists
    SendSQL("SELECT bug_id FROM bugs WHERE bug_id = $id");

    FetchOneColumn()
      || ThrowUserError("invalid_bug_id_non_existent", {'bug_id' => $id});

    return if (defined $field && ($field eq "dependson" || $field eq "blocked"));
    
    return if Bugzilla->user->can_see_bug($id);

    # The user did not pass any of the authorization tests, which means they
    # are not authorized to see the bug.  Display an error and stop execution.
    # The error the user sees depends on whether or not they are logged in
    # (i.e. $::userid contains the user's positive integer ID).
    if ($::userid) {
        ThrowUserError("bug_access_denied", {'bug_id' => $id});
    } else {
        ThrowUserError("bug_access_query", {'bug_id' => $id});
    }
}

sub CheckEmailSyntax {
    my ($addr) = (@_);
    my $match = Param('emailregexp');
    if ($addr !~ /$match/ || $addr =~ /[\\\(\)<>&,;:"\[\] \t\r\n]/) {
        ThrowUserError("illegal_email_address", { addr => $addr });
    }
}

sub MailPassword {
    my ($login, $password) = (@_);
    my $urlbase = Param("urlbase");
    my $template = Param("passwordmail");
    my $msg = PerformSubsts($template,
                            {"mailaddress" => $login . Param('emailsuffix'),
                             "login" => $login,
                             "password" => $password});

    Bugzilla::BugMail::MessageToMTA($msg);
}

sub PutHeader {
    ($vars->{'title'}, $vars->{'h1'}, $vars->{'h2'}) = (@_);
     
    $::template->process("global/header.html.tmpl", $::vars)
      || ThrowTemplateError($::template->error());
    $vars->{'header_done'} = 1;
}

sub PutFooter {
    $::template->process("global/footer.html.tmpl", $::vars)
      || ThrowTemplateError($::template->error());
}

sub LogActivityEntry {
    my ($i,$col,$removed,$added,$whoid,$timestamp) = @_;
    # in the case of CCs, deps, and keywords, there's a possibility that someone
    # might try to add or remove a lot of them at once, which might take more
    # space than the activity table allows.  We'll solve this by splitting it
    # into multiple entries if it's too long.
    while ($removed || $added) {
        my ($removestr, $addstr) = ($removed, $added);
        if (length($removestr) > MAX_LINE_LENGTH) {
            my $commaposition = find_wrap_point($removed, MAX_LINE_LENGTH);
            $removestr = substr($removed,0,$commaposition);
            $removed = substr($removed,$commaposition);
            $removed =~ s/^[,\s]+//; # remove any comma or space
        } else {
            $removed = ""; # no more entries
        }
        if (length($addstr) > MAX_LINE_LENGTH) {
            my $commaposition = find_wrap_point($added, MAX_LINE_LENGTH);
            $addstr = substr($added,0,$commaposition);
            $added = substr($added,$commaposition);
            $added =~ s/^[,\s]+//; # remove any comma or space
        } else {
            $added = ""; # no more entries
        }
        $addstr = SqlQuote($addstr);
        $removestr = SqlQuote($removestr);
        my $fieldid = GetFieldID($col);
        SendSQL("INSERT INTO bugs_activity " .
                "(bug_id,who,bug_when,fieldid,removed,added) VALUES " .
                "($i,$whoid," . SqlQuote($timestamp) . ",$fieldid,$removestr,$addstr)");
    }
}

sub GetBugActivity {
    my ($id, $starttime) = (@_);
    my $datepart = "";
    my $dbh = Bugzilla->dbh;

    die "Invalid id: $id" unless $id=~/^\s*\d+\s*$/;

    if (defined $starttime) {
        $datepart = "AND bugs_activity.bug_when > " . SqlQuote($starttime);
    }
    my $suppjoins = "";
    my $suppwhere = "";
    if (Param("insidergroup") && !UserInGroup(Param('insidergroup'))) {
        $suppjoins = "LEFT JOIN attachments 
                   ON attachments.attach_id = bugs_activity.attach_id";
        $suppwhere = "AND COALESCE(attachments.isprivate, 0) = 0";
    }
    my $query = "
        SELECT COALESCE(fielddefs.description, " 
               # This is a hack - PostgreSQL requires both COALESCE
               # arguments to be of the same type, and this is the only
               # way supported by both MySQL 3 and PostgreSQL to convert
               # an integer to a string. MySQL 4 supports CAST.
               . $dbh->sql_string_concat('bugs_activity.fieldid', q{''}) .
               "), fielddefs.name, bugs_activity.attach_id, " .
        $dbh->sql_date_format('bugs_activity.bug_when', '%Y.%m.%d %H:%i:%s') .
            ", bugs_activity.removed, bugs_activity.added, profiles.login_name
          FROM bugs_activity
               $suppjoins
     LEFT JOIN fielddefs
            ON bugs_activity.fieldid = fielddefs.fieldid
    INNER JOIN profiles
            ON profiles.userid = bugs_activity.who
         WHERE bugs_activity.bug_id = $id
               $datepart
               $suppwhere
      ORDER BY bugs_activity.bug_when";

    SendSQL($query);
    
    my @operations;
    my $operation = {};
    my $changes = [];
    my $incomplete_data = 0;
    
    while (my ($field, $fieldname, $attachid, $when, $removed, $added, $who) 
                                                               = FetchSQLData())
    {
        my %change;
        my $activity_visible = 1;
        
        # check if the user should see this field's activity
        if ($fieldname eq 'remaining_time' ||
            $fieldname eq 'estimated_time' ||
            $fieldname eq 'work_time' ||
            $fieldname eq 'deadline') {

            if (!UserInGroup(Param('timetrackinggroup'))) {
                $activity_visible = 0;
            } else {
                $activity_visible = 1;
            }
        } else {
            $activity_visible = 1;
        }
                
        if ($activity_visible) {
            # This gets replaced with a hyperlink in the template.
            $field =~ s/^Attachment// if $attachid;

            # Check for the results of an old Bugzilla data corruption bug
            $incomplete_data = 1 if ($added =~ /^\?/ || $removed =~ /^\?/);
        
            # An operation, done by 'who' at time 'when', has a number of
            # 'changes' associated with it.
            # If this is the start of a new operation, store the data from the
            # previous one, and set up the new one.
            if ($operation->{'who'} 
                && ($who ne $operation->{'who'} 
                    || $when ne $operation->{'when'})) 
            {
                $operation->{'changes'} = $changes;
                push (@operations, $operation);
            
                # Create new empty anonymous data structures.
                $operation = {};
                $changes = [];
            }  
            
            $operation->{'who'} = $who;
            $operation->{'when'} = $when;            
        
            $change{'field'} = $field;
            $change{'fieldname'} = $fieldname;
            $change{'attachid'} = $attachid;
            $change{'removed'} = $removed;
            $change{'added'} = $added;
            push (@$changes, \%change);
        }   
    }
    
    if ($operation->{'who'}) {
        $operation->{'changes'} = $changes;
        push (@operations, $operation);
    }
    
    return(\@operations, $incomplete_data);
}

############# Live code below here (that is, not subroutine defs) #############

use Bugzilla;

# XXX - mod_perl - reset this between runs
$::cgi = Bugzilla->cgi;

$::buffer = $::cgi->query_string();

# This could be needed in any CGI, so we set it here.
$vars->{'help'} = $::cgi->param('help') ? 1 : 0;

1;
