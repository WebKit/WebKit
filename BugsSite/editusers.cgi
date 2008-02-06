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
# Contributor(s): Marc Schumann <wurblzap@gmail.com>
#                 Frédéric Buclin <LpSolit@gmail.com>

use strict;
use lib ".";

require "CGI.pl";
require "globals.pl";

use vars qw( $vars );

use Bugzilla;
use Bugzilla::User;
use Bugzilla::Flag;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Util;

my $user = Bugzilla->login(LOGIN_REQUIRED);

my $cgi       = Bugzilla->cgi;
my $template  = Bugzilla->template;
my $dbh       = Bugzilla->dbh;
my $userid    = $user->id;
my $editusers = $user->in_group('editusers');

# Reject access if there is no sense in continuing.
$editusers
    || $user->can_bless()
    || ThrowUserError("auth_failure", {group  => "editusers",
                                       reason => "cant_bless",
                                       action => "edit",
                                       object => "users"});

print $cgi->header();

# Common CGI params
my $action         = $cgi->param('action') || 'search';
my $otherUserID    = $cgi->param('userid');
my $otherUserLogin = $cgi->param('user');

# Prefill template vars with data used in all or nearly all templates
$vars->{'editusers'} = $editusers;
mirrorListSelectionValues();

###########################################################################
if ($action eq 'search') {
    # Allow to restrict the search to any group the user is allowed to bless.
    $vars->{'restrictablegroups'} = $user->bless_groups();
    $template->process('admin/users/search.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

###########################################################################
} elsif ($action eq 'list') {
    my $matchstr      = $cgi->param('matchstr');
    my $matchtype     = $cgi->param('matchtype');
    my $grouprestrict = $cgi->param('grouprestrict') || '0';
    my $groupid       = $cgi->param('groupid');
    my $query = 'SELECT DISTINCT userid, login_name, realname, disabledtext ' .
                'FROM profiles';
    my @bindValues;
    my $nextCondition;
    my $visibleGroups;

    # If a group ID is given, make sure it is a valid one.
    if ($grouprestrict) {
        (detaint_natural($groupid) && GroupIdToName($groupid))
          || ThrowUserError('invalid_group_ID');
    }

    if (!$editusers && Param('usevisibilitygroups')) {
        # Show only users in visible groups.
        $visibleGroups = visibleGroupsAsString();

        if ($visibleGroups) {
            $query .= qq{, user_group_map AS ugm
                         WHERE ugm.user_id = profiles.userid
                           AND ugm.isbless = 0
                           AND ugm.group_id IN ($visibleGroups)
                        };
            $nextCondition = 'AND';
        }
    } else {
        $visibleGroups = 1;
        if ($grouprestrict eq '1') {
            $query .= ', user_group_map AS ugm';
        }
        $nextCondition = 'WHERE';
    }

    if (!$visibleGroups) {
        $vars->{'users'} = {};
    }
    else {
        # Handle selection by user name.
        if (defined($matchtype)) {
            $query .= " $nextCondition profiles.login_name ";
            if ($matchtype eq 'regexp') {
                $query .= $dbh->sql_regexp . ' ?';
                $matchstr = '.' unless $matchstr;
            } elsif ($matchtype eq 'notregexp') {
                $query .= $dbh->sql_not_regexp . ' ?';
                $matchstr = '.' unless $matchstr;
            } else { # substr or unknown
                $query .= 'like ?';
                $matchstr = "%$matchstr%";
            }
            $nextCondition = 'AND';
            # We can trick_taint because we use the value in a SELECT only,
            # using a placeholder.
            trick_taint($matchstr);
            push(@bindValues, $matchstr);
        }

        # Handle selection by group.
        if ($grouprestrict eq '1') {
            $query .= " $nextCondition profiles.userid = ugm.user_id " .
                      'AND ugm.group_id = ?';
            # We can trick_taint because we use the value in a SELECT only,
            # using a placeholder.
            push(@bindValues, $groupid);
        }
        $query .= ' ORDER BY profiles.login_name';

        $vars->{'users'} = $dbh->selectall_arrayref($query,
                                                    {'Slice' => {}},
                                                    @bindValues);
    }

    $template->process('admin/users/list.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

###########################################################################
} elsif ($action eq 'add') {
    $editusers || ThrowUserError("auth_failure", {group  => "editusers",
                                                  action => "add",
                                                  object => "users"});

    $template->process('admin/users/create.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

###########################################################################
} elsif ($action eq 'new') {
    $editusers || ThrowUserError("auth_failure", {group  => "editusers",
                                                  action => "add",
                                                  object => "users"});

    my $login        = $cgi->param('login');
    my $password     = $cgi->param('password');
    my $realname     = trim($cgi->param('name')         || '');
    my $disabledtext = trim($cgi->param('disabledtext') || '');
    
    # Lock tables during the check+creation session.
    $dbh->bz_lock_tables('profiles WRITE',
                         'profiles_activity WRITE',
                         'email_setting WRITE',
                         'namedqueries READ',
                         'whine_queries READ',
                         'tokens READ');

    # Validity checks
    $login || ThrowUserError('user_login_required');
    CheckEmailSyntax($login);
    is_available_username($login) || ThrowUserError('account_exists',
                                                    {'email' => $login});
    ValidatePassword($password);

    # Login and password are validated now, and realname and disabledtext
    # are allowed to contain anything
    trick_taint($login);
    trick_taint($realname);
    trick_taint($password);
    trick_taint($disabledtext);

    insert_new_user($login, $realname, $password, $disabledtext);
    my $userid = $dbh->bz_last_key('profiles', 'userid');
    $dbh->bz_unlock_tables();
    userDataToVars($userid);

    $vars->{'message'} = 'account_created';
    $template->process('admin/users/edit.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

###########################################################################
} elsif ($action eq 'edit') {
    my $otherUser = check_user($otherUserID, $otherUserLogin);
    $otherUserID = $otherUser->id;

    $editusers || canSeeUser($otherUserID)
        || ThrowUserError('auth_failure', {reason => "not_visible",
                                           action => "modify",
                                           object => "user"});

    userDataToVars($otherUserID);

    $template->process('admin/users/edit.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

###########################################################################
} elsif ($action eq 'update') {
    my $otherUser = check_user($otherUserID, $otherUserLogin);
    $otherUserID = $otherUser->id;

    my $logoutNeeded = 0;
    my @changedFields;

    # Lock tables during the check+update session.
    $dbh->bz_lock_tables('profiles WRITE',
                         'profiles_activity WRITE',
                         'fielddefs READ',
                         'namedqueries READ',
                         'whine_queries READ',
                         'tokens WRITE',
                         'logincookies WRITE',
                         'groups READ',
                         'user_group_map WRITE',
                         'user_group_map AS ugm READ',
                         'group_group_map READ',
                         'group_group_map AS ggm READ');
 
    $editusers || canSeeUser($otherUserID)
        || ThrowUserError('auth_failure', {reason => "not_visible",
                                           action => "modify",
                                           object => "user"});

    # Cleanups
    my $loginold        = $cgi->param('loginold')        || '';
    my $realnameold     = $cgi->param('nameold')         || '';
    my $disabledtextold = $cgi->param('disabledtextold') || '';

    my $login        = $cgi->param('login');
    my $password     = $cgi->param('password');
    my $realname     = trim($cgi->param('name')         || '');
    my $disabledtext = trim($cgi->param('disabledtext') || '');

    # Update profiles table entry; silently skip doing this if the user
    # is not authorized.
    if ($editusers) {
        my @values;

        if ($login ne $loginold) {
            # Validate, then trick_taint.
            $login || ThrowUserError('user_login_required');
            CheckEmailSyntax($login);
            is_available_username($login) || ThrowUserError('account_exists',
                                                            {'email' => $login});
            trick_taint($login);
            push(@changedFields, 'login_name');
            push(@values, $login);
            $logoutNeeded = 1;

            # Since we change the login, silently delete any tokens.
            $dbh->do('DELETE FROM tokens WHERE userid = ?', {}, $otherUserID);
        }
        if ($realname ne $realnameold) {
            # The real name may be anything; we use a placeholder for our
            # INSERT, and we rely on displaying code to FILTER html.
            trick_taint($realname);
            push(@changedFields, 'realname');
            push(@values, $realname);
        }
        if ($password) {
            # Validate, then trick_taint.
            ValidatePassword($password) if $password;
            trick_taint($password);
            push(@changedFields, 'cryptpassword');
            push(@values, bz_crypt($password));
            $logoutNeeded = 1;
        }
        if ($disabledtext ne $disabledtextold) {
            # The disable text may be anything; we use a placeholder for our
            # INSERT, and we rely on displaying code to FILTER html.
            trick_taint($disabledtext);
            push(@changedFields, 'disabledtext');
            push(@values, $disabledtext);
            $logoutNeeded = 1;
        }
        if (@changedFields) {
            push (@values, $otherUserID);
            $logoutNeeded && Bugzilla->logout_user($otherUser);
            $dbh->do('UPDATE profiles SET ' .
                     join(' = ?,', @changedFields).' = ? ' .
                     'WHERE userid = ?',
                     undef, @values);
            # XXX: should create profiles_activity entries.
        }
    }

    # Update group settings.
    my $sth_add_mapping = $dbh->prepare(
        qq{INSERT INTO user_group_map (
                  user_id, group_id, isbless, grant_type
                 ) VALUES (
                  ?, ?, ?, ?
                 )
          });
    my $sth_remove_mapping = $dbh->prepare(
        qq{DELETE FROM user_group_map
            WHERE user_id = ?
              AND group_id = ?
              AND isbless = ?
              AND grant_type = ?
          });

    my @groupsAddedTo;
    my @groupsRemovedFrom;
    my @groupsGrantedRightsToBless;
    my @groupsDeniedRightsToBless;

    # Regard only groups the user is allowed to bless and skip all others
    # silently.
    # XXX: checking for existence of each user_group_map entry
    #      would allow to display a friendlier error message on page reloads.
    foreach (@{$user->bless_groups()}) {
        my $id = $$_{'id'};
        my $name = $$_{'name'};

        # Change memberships.
        my $oldgroupid = $cgi->param("oldgroup_$id") || '0';
        my $groupid    = $cgi->param("group_$id")    || '0';
        if ($groupid ne $oldgroupid) {
            if ($groupid eq '0') {
                $sth_remove_mapping->execute(
                    $otherUserID, $id, 0, GRANT_DIRECT);
                push(@groupsRemovedFrom, $name);
            } else {
                $sth_add_mapping->execute(
                    $otherUserID, $id, 0, GRANT_DIRECT);
                push(@groupsAddedTo, $name);
            }
        }

        # Only members of the editusers group may change bless grants.
        # Skip silently if this is not the case.
        if ($editusers) {
            my $oldgroupid = $cgi->param("oldbless_$id") || '0';
            my $groupid    = $cgi->param("bless_$id")    || '0';
            if ($groupid ne $oldgroupid) {
                if ($groupid eq '0') {
                    $sth_remove_mapping->execute(
                        $otherUserID, $id, 1, GRANT_DIRECT);
                    push(@groupsDeniedRightsToBless, $name);
                } else {
                    $sth_add_mapping->execute(
                        $otherUserID, $id, 1, GRANT_DIRECT);
                    push(@groupsGrantedRightsToBless, $name);
                }
            }
        }
    }
    if (@groupsAddedTo || @groupsRemovedFrom) {
        $dbh->do(qq{INSERT INTO profiles_activity (
                           userid, who,
                           profiles_when, fieldid,
                           oldvalue, newvalue
                          ) VALUES (
                           ?, ?, now(), ?, ?, ?
                          )
                   },
                 undef,
                 ($otherUserID, $userid,
                  GetFieldID('bug_group'),
                  join(', ', @groupsRemovedFrom), join(', ', @groupsAddedTo)));
        $dbh->do('UPDATE profiles SET refreshed_when=? WHERE userid = ?',
                 undef, ('1900-01-01 00:00:00', $otherUserID));
    }
    # XXX: should create profiles_activity entries for blesser changes.

    $dbh->bz_unlock_tables();

    # XXX: userDataToVars may be off when editing ourselves.
    userDataToVars($otherUserID);

    $vars->{'message'} = 'account_updated';
    $vars->{'loginold'} = $loginold;
    $vars->{'changed_fields'} = \@changedFields;
    $vars->{'groups_added_to'} = \@groupsAddedTo;
    $vars->{'groups_removed_from'} = \@groupsRemovedFrom;
    $vars->{'groups_granted_rights_to_bless'} = \@groupsGrantedRightsToBless;
    $vars->{'groups_denied_rights_to_bless'} = \@groupsDeniedRightsToBless;
    $template->process('admin/users/edit.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

###########################################################################
} elsif ($action eq 'del') {
    my $otherUser = check_user($otherUserID, $otherUserLogin);
    $otherUserID = $otherUser->id;

    Param('allowuserdeletion') || ThrowUserError('users_deletion_disabled');
    $editusers || ThrowUserError('auth_failure', {group  => "editusers",
                                                  action => "delete",
                                                  object => "users"});
    $vars->{'otheruser'}      = $otherUser;
    $vars->{'editcomponents'} = UserInGroup('editcomponents');

    # Find other cross references.
    $vars->{'assignee_or_qa'} = $dbh->selectrow_array(
        qq{SELECT COUNT(*)
           FROM bugs
           WHERE assigned_to = ? OR qa_contact = ?},
        undef, ($otherUserID, $otherUserID));
    $vars->{'reporter'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM bugs WHERE reporter = ?',
        undef, $otherUserID);
    $vars->{'cc'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM cc WHERE who = ?',
        undef, $otherUserID);
    $vars->{'bugs_activity'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM bugs_activity WHERE who = ?',
        undef, $otherUserID);
    $vars->{'email_setting'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM email_setting WHERE user_id = ?',
        undef, $otherUserID);
    $vars->{'flags'}{'requestee'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM flags WHERE requestee_id = ? AND is_active = 1',
        undef, $otherUserID);
    $vars->{'flags'}{'setter'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM flags WHERE setter_id = ?',
        undef, $otherUserID);
    $vars->{'longdescs'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM longdescs WHERE who = ?',
        undef, $otherUserID);
    $vars->{'namedqueries'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM namedqueries WHERE userid = ?',
        undef, $otherUserID);
    $vars->{'profile_setting'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM profile_setting WHERE user_id = ?',
        undef, $otherUserID);
    $vars->{'profiles_activity'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM profiles_activity WHERE who = ? AND userid != ?',
        undef, ($otherUserID, $otherUserID));
    $vars->{'series'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM series WHERE creator = ?',
        undef, $otherUserID);
    $vars->{'votes'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM votes WHERE who = ?',
        undef, $otherUserID);
    $vars->{'watch'}{'watched'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM watch WHERE watched = ?',
        undef, $otherUserID);
    $vars->{'watch'}{'watcher'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM watch WHERE watcher = ?',
        undef, $otherUserID);
    $vars->{'whine_events'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM whine_events WHERE owner_userid = ?',
        undef, $otherUserID);
    $vars->{'whine_schedules'} = $dbh->selectrow_array(
        qq{SELECT COUNT(distinct eventid)
           FROM whine_schedules
           WHERE mailto = ?
           AND mailto_type = ?
          },
        undef, ($otherUserID, MAILTO_USER));

    $template->process('admin/users/confirm-delete.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

###########################################################################
} elsif ($action eq 'delete') {
    my $otherUser = check_user($otherUserID, $otherUserLogin);
    $otherUserID = $otherUser->id;

    # Cache for user accounts.
    my %usercache = (0 => new Bugzilla::User());
    my %updatedbugs;

    # Lock tables during the check+removal session.
    # XXX: if there was some change on these tables after the deletion
    #      confirmation checks, we may do something here we haven't warned
    #      about.
    $dbh->bz_lock_tables('bugs WRITE',
                         'bugs_activity WRITE',
                         'attachments READ',
                         'fielddefs READ',
                         'products READ',
                         'components READ',
                         'logincookies WRITE',
                         'profiles WRITE',
                         'profiles_activity WRITE',
                         'email_setting WRITE',
                         'profile_setting WRITE',
                         'groups READ',
                         'bug_group_map READ',
                         'user_group_map WRITE',
                         'group_group_map READ',
                         'flags WRITE',
                         'flagtypes READ',
                         'cc WRITE',
                         'namedqueries WRITE',
                         'tokens WRITE',
                         'votes WRITE',
                         'watch WRITE',
                         'series WRITE',
                         'series_data WRITE',
                         'whine_schedules WRITE',
                         'whine_queries WRITE',
                         'whine_events WRITE');

    Param('allowuserdeletion')
        || ThrowUserError('users_deletion_disabled');
    $editusers || ThrowUserError('auth_failure',
                                 {group  => "editusers",
                                  action => "delete",
                                  object => "users"});
    @{$otherUser->product_responsibilities()}
        && ThrowUserError('user_has_responsibility');

    Bugzilla->logout_user($otherUser);

    # Get the timestamp for LogActivityEntry.
    my $timestamp = $dbh->selectrow_array('SELECT NOW()');

    # When we update a bug_activity entry, we update the bug timestamp, too.
    my $sth_set_bug_timestamp =
        $dbh->prepare('UPDATE bugs SET delta_ts = ? WHERE bug_id = ?');

    # Reference removals which need LogActivityEntry.
    my $statement_flagupdate = 'UPDATE flags set requestee_id = NULL
                                 WHERE bug_id = ?
                                   AND attach_id %s
                                   AND requestee_id = ?';
    my $sth_flagupdate_attachment =
        $dbh->prepare(sprintf($statement_flagupdate, '= ?'));
    my $sth_flagupdate_bug =
        $dbh->prepare(sprintf($statement_flagupdate, 'IS NULL'));

    my $buglist = $dbh->selectall_arrayref('SELECT DISTINCT bug_id, attach_id
                                              FROM flags
                                             WHERE requestee_id = ?',
                                           undef, $otherUserID);

    foreach (@$buglist) {
        my ($bug_id, $attach_id) = @$_;
        my @old_summaries = Bugzilla::Flag::snapshot($bug_id, $attach_id);
        if ($attach_id) {
            $sth_flagupdate_attachment->execute($bug_id, $attach_id, $otherUserID);
        }
        else {
            $sth_flagupdate_bug->execute($bug_id, $otherUserID);
        }
        my @new_summaries = Bugzilla::Flag::snapshot($bug_id, $attach_id);
        # Let update_activity do all the dirty work, including setting
        # the bug timestamp.
        Bugzilla::Flag::update_activity($bug_id, $attach_id,
                                        $dbh->quote($timestamp),
                                        \@old_summaries, \@new_summaries);
        $updatedbugs{$bug_id} = 1;
    }

    # Simple deletions in referred tables.
    $dbh->do('DELETE FROM email_setting WHERE user_id = ?', undef,
             $otherUserID);
    $dbh->do('DELETE FROM logincookies WHERE userid = ?', undef, $otherUserID);
    $dbh->do('DELETE FROM namedqueries WHERE userid = ?', undef, $otherUserID);
    $dbh->do('DELETE FROM profile_setting WHERE user_id = ?', undef,
             $otherUserID);
    $dbh->do('DELETE FROM profiles_activity WHERE userid = ? OR who = ?', undef,
             ($otherUserID, $otherUserID));
    $dbh->do('DELETE FROM tokens WHERE userid = ?', undef, $otherUserID);
    $dbh->do('DELETE FROM user_group_map WHERE user_id = ?', undef,
             $otherUserID);
    $dbh->do('DELETE FROM votes WHERE who = ?', undef, $otherUserID);
    $dbh->do('DELETE FROM watch WHERE watcher = ? OR watched = ?', undef,
             ($otherUserID, $otherUserID));

    # Deletions in referred tables which need LogActivityEntry.
    $buglist = $dbh->selectcol_arrayref('SELECT bug_id FROM cc
                                          WHERE who = ?',
                                        undef, $otherUserID);
    $dbh->do('DELETE FROM cc WHERE who = ?', undef, $otherUserID);
    foreach my $bug_id (@$buglist) {
        LogActivityEntry($bug_id, 'cc', $otherUser->login, '', $userid,
                         $timestamp);
        $sth_set_bug_timestamp->execute($timestamp, $bug_id);
        $updatedbugs{$bug_id} = 1;
    }

    # Even more complex deletions in referred tables.
    my $id;

    # 1) Series
    my $sth_seriesid = $dbh->prepare(
           'SELECT series_id FROM series WHERE creator = ?');
    my $sth_deleteSeries = $dbh->prepare(
           'DELETE FROM series WHERE series_id = ?');
    my $sth_deleteSeriesData = $dbh->prepare(
           'DELETE FROM series_data WHERE series_id = ?');

    $sth_seriesid->execute($otherUserID);
    while ($id = $sth_seriesid->fetchrow_array()) {
        $sth_deleteSeriesData->execute($id);
        $sth_deleteSeries->execute($id);
    }

    # 2) Whines
    my $sth_whineidFromSchedules = $dbh->prepare(
           qq{SELECT eventid FROM whine_schedules
              WHERE mailto = ? AND mailto_type = ?});
    my $sth_whineidFromEvents = $dbh->prepare(
           'SELECT id FROM whine_events WHERE owner_userid = ?');
    my $sth_deleteWhineEvent = $dbh->prepare(
           'DELETE FROM whine_events WHERE id = ?');
    my $sth_deleteWhineQuery = $dbh->prepare(
           'DELETE FROM whine_queries WHERE eventid = ?');
    my $sth_deleteWhineSchedule = $dbh->prepare(
           'DELETE FROM whine_schedules WHERE eventid = ?');

    $sth_whineidFromSchedules->execute($otherUserID, MAILTO_USER);
    while ($id = $sth_whineidFromSchedules->fetchrow_array()) {
        $sth_deleteWhineQuery->execute($id);
        $sth_deleteWhineSchedule->execute($id);
        $sth_deleteWhineEvent->execute($id);
    }

    $sth_whineidFromEvents->execute($otherUserID);
    while ($id = $sth_whineidFromEvents->fetchrow_array()) {
        $sth_deleteWhineQuery->execute($id);
        $sth_deleteWhineSchedule->execute($id);
        $sth_deleteWhineEvent->execute($id);
    }

    # 3) Bugs
    # 3.1) fall back to the default assignee
    $buglist = $dbh->selectall_arrayref(
        'SELECT bug_id, initialowner
         FROM bugs
         INNER JOIN components ON components.id = bugs.component_id
         WHERE assigned_to = ?', undef, $otherUserID);

    my $sth_updateAssignee = $dbh->prepare(
        'UPDATE bugs SET assigned_to = ?, delta_ts = ? WHERE bug_id = ?');

    foreach my $bug (@$buglist) {
        my ($bug_id, $default_assignee_id) = @$bug;
        $sth_updateAssignee->execute($default_assignee_id,
                                     $timestamp, $bug_id);
        $updatedbugs{$bug_id} = 1;
        $default_assignee_id ||= 0;
        $usercache{$default_assignee_id} ||=
            new Bugzilla::User($default_assignee_id);
        LogActivityEntry($bug_id, 'assigned_to', $otherUser->login,
                         $usercache{$default_assignee_id}->login,
                         $userid, $timestamp);
    }

    # 3.2) fall back to the default QA contact
    $buglist = $dbh->selectall_arrayref(
        'SELECT bug_id, initialqacontact
         FROM bugs
         INNER JOIN components ON components.id = bugs.component_id
         WHERE qa_contact = ?', undef, $otherUserID);

    my $sth_updateQAcontact = $dbh->prepare(
        'UPDATE bugs SET qa_contact = ?, delta_ts = ? WHERE bug_id = ?');

    foreach my $bug (@$buglist) {
        my ($bug_id, $default_qa_contact_id) = @$bug;
        $sth_updateQAcontact->execute($default_qa_contact_id,
                                      $timestamp, $bug_id);
        $updatedbugs{$bug_id} = 1;
        $default_qa_contact_id ||= 0;
        $usercache{$default_qa_contact_id} ||=
            new Bugzilla::User($default_qa_contact_id);
        LogActivityEntry($bug_id, 'qa_contact', $otherUser->login,
                         $usercache{$default_qa_contact_id}->login,
                         $userid, $timestamp);
    }

    # Finally, remove the user account itself.
    $dbh->do('DELETE FROM profiles WHERE userid = ?', undef, $otherUserID);

    $dbh->bz_unlock_tables();

    $vars->{'message'} = 'account_deleted';
    $vars->{'otheruser'}{'login'} = $otherUser->login;
    $vars->{'restrictablegroups'} = $user->bless_groups();
    $template->process('admin/users/search.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

    # Send mail about what we've done to bugs.
    # The deleted user is not notified of the changes.
    foreach (keys(%updatedbugs)) {
        Bugzilla::BugMail::Send($_);
    }

###########################################################################
} else {
    $vars->{'action'} = $action;
    ThrowCodeError('action_unrecognized', $vars);
}

exit;

###########################################################################
# Helpers
###########################################################################

# Try to build a user object using its ID, else its login name, and throw
# an error if the user does not exist.
sub check_user {
    my ($otherUserID, $otherUserLogin) = @_;

    my $otherUser;
    my $vars = {};

    if ($otherUserID) {
        $otherUser = Bugzilla::User->new($otherUserID);
        $vars->{'user_id'} = $otherUserID;
    }
    elsif ($otherUserLogin) {
        $otherUser = Bugzilla::User->new_from_login($otherUserLogin);
        $vars->{'user_login'} = $otherUserLogin;
    }
    ($otherUser && $otherUser->id) || ThrowCodeError('invalid_user', $vars);

    return $otherUser;
}

# Copy incoming list selection values from CGI params to template variables.
sub mirrorListSelectionValues {
    if (defined($cgi->param('matchtype'))) {
        foreach ('matchstr', 'matchtype', 'grouprestrict', 'groupid') {
            $vars->{'listselectionvalues'}{$_} = $cgi->param($_);
        }
    }
}

# Give a list of IDs of groups the user can see.
sub visibleGroupsAsString {
    return join(', ', @{$user->visible_groups_direct()});
}

# Determine whether the user can see a user. (Checks for existence, too.)
sub canSeeUser {
    my $otherUserID = shift;
    my $query;

    if (Param('usevisibilitygroups')) {
        # If the user can see no groups, then no users are visible either.
        my $visibleGroups = visibleGroupsAsString() || return 0;

        $query = qq{SELECT COUNT(DISTINCT userid)
                    FROM profiles, user_group_map
                    WHERE userid = ?
                    AND user_id = userid
                    AND isbless = 0
                    AND group_id IN ($visibleGroups)
                   };
    } else {
        $query = qq{SELECT COUNT(userid)
                    FROM profiles
                    WHERE userid = ?
                   };
    }
    return $dbh->selectrow_array($query, undef, $otherUserID);
}

# Retrieve user data for the user editing form. User creation and user
# editing code rely on this to call derive_groups().
sub userDataToVars {
    my $otheruserid = shift;
    my $otheruser = new Bugzilla::User($otheruserid);
    my $query;
    my $dbh = Bugzilla->dbh;

    $otheruser->derive_groups();

    $vars->{'otheruser'} = $otheruser;
    $vars->{'groups'} = $user->bless_groups();

    $vars->{'permissions'} = $dbh->selectall_hashref(
        qq{SELECT id,
                  COUNT(directmember.group_id) AS directmember,
                  COUNT(regexpmember.group_id) AS regexpmember,
                  COUNT(derivedmember.group_id) AS derivedmember,
                  COUNT(directbless.group_id) AS directbless
           FROM groups
           LEFT JOIN user_group_map AS directmember
                  ON directmember.group_id = id
                 AND directmember.user_id = ?
                 AND directmember.isbless = 0
                 AND directmember.grant_type = ?
           LEFT JOIN user_group_map AS regexpmember
                  ON regexpmember.group_id = id
                 AND regexpmember.user_id = ?
                 AND regexpmember.isbless = 0
                 AND regexpmember.grant_type = ?
           LEFT JOIN user_group_map AS derivedmember
                  ON derivedmember.group_id = id
                 AND derivedmember.user_id = ?
                 AND derivedmember.isbless = 0
                 AND derivedmember.grant_type = ?
           LEFT JOIN user_group_map AS directbless
                  ON directbless.group_id = id
                 AND directbless.user_id = ?
                 AND directbless.isbless = 1
                 AND directbless.grant_type = ?
          } . $dbh->sql_group_by('id'),
        'id', undef,
        ($otheruserid, GRANT_DIRECT,
         $otheruserid, GRANT_REGEXP,
         $otheruserid, GRANT_DERIVED,
         $otheruserid, GRANT_DIRECT));

    # Find indirect bless permission.
    $query = qq{SELECT groups.id
                FROM groups, user_group_map AS ugm, group_group_map AS ggm
                WHERE ugm.user_id = ?
                  AND groups.id = ggm.grantor_id
                  AND ggm.member_id = ugm.group_id
                  AND ugm.isbless = 0
                  AND ggm.grant_type = ?
               } . $dbh->sql_group_by('id');
    foreach (@{$dbh->selectall_arrayref($query, undef,
                                        ($otheruserid, GROUP_BLESS))}) {
        # Merge indirect bless permissions into permission variable.
        $vars->{'permissions'}{${$_}[0]}{'indirectbless'} = 1;
    }
}
