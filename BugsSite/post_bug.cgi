#!/usr/bin/perl -wT
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
#                 Gervase Markham <gerv@gerv.net>

use strict;
use lib qw(.);

use Bugzilla;
use Bugzilla::Constants;
require "CGI.pl";

use Bugzilla::Bug;

use Bugzilla::User;

# Shut up misguided -w warnings about "used only once". For some reason,
# "use vars" chokes on me when I try it here.
sub sillyness {
    my $zz;
    $zz = $::buffer;
    $zz = %::components;
    $zz = %::versions;
    $zz = @::legal_opsys;
    $zz = @::legal_platform;
    $zz = @::legal_priority;
    $zz = @::legal_product;
    $zz = @::legal_severity;
    $zz = %::target_milestone;
}

# Use global template variables.
use vars qw($vars $template);

my $user = Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;

my $dbh = Bugzilla->dbh;

# do a match on the fields if applicable

&Bugzilla::User::match_field ($cgi, {
    'cc'            => { 'type' => 'multi'  },
    'assigned_to'   => { 'type' => 'single' },
    'qa_contact'    => { 'type' => 'single' },
});

# The format of the initial comment can be structured by adding fields to the
# enter_bug template and then referencing them in the comment template.
my $comment;

my $format = GetFormat("bug/create/comment",
                       scalar($cgi->param('format')), "txt");

$template->process($format->{'template'}, $vars, \$comment)
  || ThrowTemplateError($template->error());

ValidateComment($comment);

# Check that the product exists and that the user
# is allowed to enter bugs into this product.
my $product = $cgi->param('product');
CanEnterProductOrWarn($product);

my $product_id = get_product_id($product);

# Set cookies
if (defined $cgi->param('product')) {
    if (defined $cgi->param('version')) {
        $cgi->send_cookie(-name => "VERSION-$product",
                          -value => $cgi->param('version'),
                          -expires => "Fri, 01-Jan-2038 00:00:00 GMT");
    }
}

if (defined $cgi->param('maketemplate')) {
    $vars->{'url'} = $::buffer;
    
    print $cgi->header();
    $template->process("bug/create/make-template.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

umask 0;

# Some sanity checking
my $component_id = get_component_id($product_id,
                                    scalar($cgi->param('component')));
$component_id || ThrowUserError("require_component");

# Set the parameter to itself, but cleaned up
$cgi->param('short_desc', clean_text($cgi->param('short_desc')));

if (!defined $cgi->param('short_desc')
    || $cgi->param('short_desc') eq "") {
    ThrowUserError("require_summary");
}

# Check that if required a description has been provided
# This has to go somewhere after 'maketemplate' 
#  or it breaks bookmarks with no comments.
if (Param("commentoncreate") && !trim($cgi->param('comment'))) {
    ThrowUserError("description_required");
}

# If bug_file_loc is "http://", the default, use an empty value instead.
$cgi->param('bug_file_loc', '') if $cgi->param('bug_file_loc') eq 'http://';

my $sql_product = SqlQuote($cgi->param('product'));
my $sql_component = SqlQuote($cgi->param('component'));

# Default assignee is the component owner.
if (!UserInGroup("editbugs") || $cgi->param('assigned_to') eq "") {
    SendSQL("SELECT initialowner FROM components " .
            "WHERE id = $component_id");
    $cgi->param(-name => 'assigned_to', -value => FetchOneColumn());
} else {
    $cgi->param(-name => 'assigned_to',
                -value => DBNameToIdAndCheck(trim($cgi->param('assigned_to'))));
}

my @bug_fields = ("version", "rep_platform",
                  "bug_severity", "priority", "op_sys", "assigned_to",
                  "bug_status", "everconfirmed", "bug_file_loc", "short_desc",
                  "target_milestone", "status_whiteboard");

if (Param("usebugaliases")) {
   my $alias = trim($cgi->param('alias') || "");
   if ($alias ne "") {
       ValidateBugAlias($alias);
       $cgi->param('alias', $alias);
       push (@bug_fields,"alias");
   }
}

# Retrieve the default QA contact if the field is empty
if (Param("useqacontact")) {
    my $qa_contact;
    if (!UserInGroup("editbugs") || !defined $cgi->param('qa_contact')
        || trim($cgi->param('qa_contact')) eq "") {
        SendSQL("SELECT initialqacontact FROM components " .
                "WHERE id = $component_id");
        $qa_contact = FetchOneColumn();
    } else {
        $qa_contact = DBNameToIdAndCheck(trim($cgi->param('qa_contact')));
    }

    if ($qa_contact) {
        $cgi->param(-name => 'qa_contact', -value => $qa_contact);
        push(@bug_fields, "qa_contact");
    }
}

if (UserInGroup("editbugs") || UserInGroup("canconfirm")) {
    # Default to NEW if the user hasn't selected another status
    if (!defined $cgi->param('bug_status')) {
        $cgi->param(-name => 'bug_status', -value => "NEW");
    }
} else {
    # Default to UNCONFIRMED if we are using it, NEW otherwise
    $cgi->param(-name => 'bug_status', -value => 'UNCONFIRMED');
    SendSQL("SELECT votestoconfirm FROM products WHERE id = $product_id");
    if (!FetchOneColumn()) {   
        $cgi->param(-name => 'bug_status', -value => "NEW");
    }
}

if (!defined $cgi->param('target_milestone')) {
    SendSQL("SELECT defaultmilestone FROM products WHERE name=$sql_product");
    $cgi->param(-name => 'target_milestone', -value => FetchOneColumn());
}

if (!Param('letsubmitterchoosepriority')) {
    $cgi->param(-name => 'priority', -value => Param('defaultpriority'));
}

GetVersionTable();

# Some more sanity checking
CheckFormField($cgi, 'product',      \@::legal_product);
CheckFormField($cgi, 'rep_platform', \@::legal_platform);
CheckFormField($cgi, 'bug_severity', \@::legal_severity);
CheckFormField($cgi, 'priority',     \@::legal_priority);
CheckFormField($cgi, 'op_sys',       \@::legal_opsys);
CheckFormField($cgi, 'bug_status',   ['UNCONFIRMED', 'NEW']);
CheckFormField($cgi, 'version',          $::versions{$product});
CheckFormField($cgi, 'component',        $::components{$product});
CheckFormField($cgi, 'target_milestone', $::target_milestone{$product});
CheckFormFieldDefined($cgi, 'assigned_to');
CheckFormFieldDefined($cgi, 'bug_file_loc');
CheckFormFieldDefined($cgi, 'comment');

my $everconfirmed = ($cgi->param('bug_status') eq 'UNCONFIRMED') ? 0 : 1;
$cgi->param(-name => 'everconfirmed', -value => $everconfirmed);

my @used_fields;
foreach my $field (@bug_fields) {
    if (defined $cgi->param($field)) {
        push (@used_fields, $field);
    }
}

$cgi->param(-name => 'product_id', -value => $product_id);
push(@used_fields, "product_id");
$cgi->param(-name => 'component_id', -value => $component_id);
push(@used_fields, "component_id");

my %ccids;

# Create the ccid hash for inserting into the db
# use a hash rather than a list to avoid adding users twice
if (defined $cgi->param('cc')) {
    foreach my $person ($cgi->param('cc')) {
        next unless $person;
        my $ccid = DBNameToIdAndCheck($person);
        if ($ccid && !$ccids{$ccid}) {
           $ccids{$ccid} = 1;
        }
    }
}
# Check for valid keywords and create list of keywords to be added to db
# (validity routine copied from process_bug.cgi)
my @keywordlist;
my %keywordseen;

if ($cgi->param('keywords') && UserInGroup("editbugs")) {
    foreach my $keyword (split(/[\s,]+/, $cgi->param('keywords'))) {
        if ($keyword eq '') {
           next;
        }
        my $i = GetKeywordIdFromName($keyword);
        if (!$i) {
            ThrowUserError("unknown_keyword",
                           { keyword => $keyword });
        }
        if (!$keywordseen{$i}) {
            push(@keywordlist, $i);
            $keywordseen{$i} = 1;
        }
    }
}

# Check for valid dependency info. 
foreach my $field ("dependson", "blocked") {
    if (UserInGroup("editbugs") && $cgi->param($field)) {
        my @validvalues;
        foreach my $id (split(/[\s,]+/, $cgi->param($field))) {
            next unless $id;
            # $field is not passed to ValidateBugID to prevent adding new 
            # dependencies on inacessible bugs.
            ValidateBugID($id);
            push(@validvalues, $id);
        }
        $cgi->param(-name => $field, -value => join(",", @validvalues));
    }
}
# Gather the dependency list, and make sure there are no circular refs
my %deps;
if (UserInGroup("editbugs")) {
    %deps = Bugzilla::Bug::ValidateDependencies($cgi->param('dependson'),
                                                $cgi->param('blocked'),
                                                undef);
}

# get current time
SendSQL("SELECT NOW()");
my $timestamp = FetchOneColumn();
my $sql_timestamp = SqlQuote($timestamp);

# Build up SQL string to add bug.
# creation_ts will only be set when all other fields are defined.
my $sql = "INSERT INTO bugs " . 
  "(" . join(",", @used_fields) . ", reporter, delta_ts, " .
  "estimated_time, remaining_time, deadline) " .
  "VALUES (";

foreach my $field (@used_fields) {
    $sql .= SqlQuote($cgi->param($field)) . ",";
}

$comment =~ s/\r\n?/\n/g;     # Get rid of \r.
$comment = trim($comment);
# If comment is all whitespace, it'll be null at this point. That's
# OK except for the fact that it causes e-mail to be suppressed.
$comment = $comment ? $comment : " ";

$sql .= "$::userid, $sql_timestamp, ";

# Time Tracking
if (UserInGroup(Param("timetrackinggroup")) &&
    defined $cgi->param('estimated_time')) {

    my $est_time = $cgi->param('estimated_time');
    Bugzilla::Bug::ValidateTime($est_time, 'estimated_time');
    $sql .= SqlQuote($est_time) . "," . SqlQuote($est_time) . ",";
} else {
    $sql .= "0, 0, ";
}

if ((UserInGroup(Param("timetrackinggroup"))) && ($cgi->param('deadline'))) {
    Bugzilla::Util::ValidateDate($cgi->param('deadline'), 'YYYY-MM-DD');
    $sql .= SqlQuote($cgi->param('deadline'));  
} else {
    $sql .= "NULL";
}

$sql .= ")";

# Groups
my @groupstoadd = ();
foreach my $b (grep(/^bit-\d*$/, $cgi->param())) {
    if ($cgi->param($b)) {
        my $v = substr($b, 4);
        detaint_natural($v)
          || ThrowUserError("invalid_group_ID");
        if (!GroupIsActive($v)) {
            # Prevent the user from adding the bug to an inactive group.
            # Should only happen if there is a bug in Bugzilla or the user
            # hacked the "enter bug" form since otherwise the UI 
            # for adding the bug to the group won't appear on that form.
            $vars->{'bit'} = $v;
            ThrowCodeError("inactive_group");
        }
        SendSQL("SELECT user_id FROM user_group_map 
                 WHERE user_id = $::userid
                 AND group_id = $v
                 AND isbless = 0");
        my ($permit) = FetchSQLData();
        if (!$permit) {
            SendSQL("SELECT othercontrol FROM group_control_map
                     WHERE group_id = $v AND product_id = $product_id");
            my ($othercontrol) = FetchSQLData();
            $permit = (($othercontrol == CONTROLMAPSHOWN)
                       || ($othercontrol == CONTROLMAPDEFAULT));
        }
        if ($permit) {
            push(@groupstoadd, $v)
        }
    }
}

SendSQL("SELECT DISTINCT groups.id, groups.name, " .
        "membercontrol, othercontrol, description " .
        "FROM groups LEFT JOIN group_control_map " .
        "ON group_id = id AND product_id = $product_id " .
        " WHERE isbuggroup != 0 AND isactive != 0 ORDER BY description");
while (MoreSQLData()) {
    my ($id, $groupname, $membercontrol, $othercontrol ) = FetchSQLData();
    $membercontrol ||= 0;
    $othercontrol ||= 0;
    # Add groups required
    if (($membercontrol == CONTROLMAPMANDATORY)
       || (($othercontrol == CONTROLMAPMANDATORY) 
            && (!UserInGroup($groupname)))) {
        # User had no option, bug needs to be in this group.
        push(@groupstoadd, $id)
    }
}

# Add the bug report to the DB.
$dbh->bz_lock_tables('bugs WRITE', 'bug_group_map WRITE', 'longdescs WRITE',
                     'cc WRITE', 'keywords WRITE', 'dependencies WRITE',
                     'bugs_activity WRITE', 'groups READ', 'user_group_map READ',
                     'keyworddefs READ', 'fielddefs READ');

SendSQL($sql);

# Get the bug ID back.
my $id = $dbh->bz_last_key('bugs', 'bug_id');

# Add the group restrictions
foreach my $grouptoadd (@groupstoadd) {
    SendSQL("INSERT INTO bug_group_map (bug_id, group_id)
             VALUES ($id, $grouptoadd)");
}

# Add the initial comment, allowing for the fact that it may be private
my $privacy = 0;
if (Param("insidergroup") && UserInGroup(Param("insidergroup"))) {
    $privacy = $cgi->param('commentprivacy') ? 1 : 0;
}

SendSQL("INSERT INTO longdescs (bug_id, who, bug_when, thetext, isprivate) 
         VALUES ($id, " . SqlQuote($user->id) . ", $sql_timestamp, " .
        SqlQuote($comment) . ", $privacy)");

# Insert the cclist into the database
foreach my $ccid (keys(%ccids)) {
    SendSQL("INSERT INTO cc (bug_id, who) VALUES ($id, $ccid)");
}

my @all_deps;
if (UserInGroup("editbugs")) {
    foreach my $keyword (@keywordlist) {
        SendSQL("INSERT INTO keywords (bug_id, keywordid) 
                 VALUES ($id, $keyword)");
    }
    if (@keywordlist) {
        # Make sure that we have the correct case for the kw
        SendSQL("SELECT name FROM keyworddefs WHERE id IN ( " .
                join(',', @keywordlist) . ")");
        my @list;
        while (MoreSQLData()) {
            push (@list, FetchOneColumn());
        }
        SendSQL("UPDATE bugs SET delta_ts = $sql_timestamp," .
                " keywords = " . SqlQuote(join(', ', @list)) .
                " WHERE bug_id = $id");
    }
    if ($cgi->param('dependson') || $cgi->param('blocked')) {
        foreach my $pair (["blocked", "dependson"], ["dependson", "blocked"]) {
            my ($me, $target) = @{$pair};

            foreach my $i (@{$deps{$target}}) {
                SendSQL("INSERT INTO dependencies ($me, $target) values " .
                        "($id, $i)");
                push(@all_deps, $i); # list for mailing dependent bugs
                # Log the activity for the other bug:
                LogActivityEntry($i, $me, "", $id, $user->id, $timestamp);
            }
        }
    }
}

# All fields related to the newly created bug are set.
# The bug can now be made accessible.
$dbh->do("UPDATE bugs SET creation_ts = ? WHERE bug_id = ?",
          undef, ($timestamp, $id));

$dbh->bz_unlock_tables();

# Email everyone the details of the new bug 
$vars->{'mailrecipients'} = {'changer' => Bugzilla->user->login};

$vars->{'id'} = $id;
my $bug = new Bugzilla::Bug($id, $::userid);
$vars->{'bug'} = $bug;

ThrowCodeError("bug_error", { bug => $bug }) if $bug->error;

$vars->{'sentmail'} = [];

push (@{$vars->{'sentmail'}}, { type => 'created',
                                id => $id,
                              });

foreach my $i (@all_deps) {
    push (@{$vars->{'sentmail'}}, { type => 'dep', id => $i, });
}

my @bug_list;
if ($cgi->cookie("BUGLIST")) {
    @bug_list = split(/:/, $cgi->cookie("BUGLIST"));
}
$vars->{'bug_list'} = \@bug_list;

print $cgi->header();
$template->process("bug/create/created.html.tmpl", $vars)
  || ThrowTemplateError($template->error());


