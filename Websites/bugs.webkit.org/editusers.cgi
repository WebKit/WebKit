#!/usr/bin/perl -T
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Bug;
use Bugzilla::BugMail;
use Bugzilla::Flag;
use Bugzilla::Field;
use Bugzilla::Group;
use Bugzilla::Token;
use Bugzilla::Mailer;

my $user = Bugzilla->login(LOGIN_REQUIRED);

my $cgi       = Bugzilla->cgi;
my $template  = Bugzilla->template;
my $dbh       = Bugzilla->dbh;
my $userid    = $user->id;
my $editusers = $user->in_group('editusers');
local our $vars     = {};

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
my $token          = $cgi->param('token');

# Prefill template vars with data used in all or nearly all templates
$vars->{'editusers'} = $editusers;
mirrorListSelectionValues();

Bugzilla::Hook::process('admin_editusers_action',
    { vars => $vars, user => $user, action => $action });

###########################################################################
if ($action eq 'search') {
    # Allow to restrict the search to any group the user is allowed to bless.
    $vars->{'restrictablegroups'} = $user->bless_groups();
    $template->process('admin/users/search.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

###########################################################################
} elsif ($action eq 'list') {
    my $matchvalue    = $cgi->param('matchvalue') || '';
    my $matchstr      = trim($cgi->param('matchstr'));
    my $matchtype     = $cgi->param('matchtype');
    my $grouprestrict = $cgi->param('grouprestrict') || '0';
    # 0 = disabled only, 1 = enabled only, 2 = everyone
    my $is_enabled    = $cgi->param('is_enabled') // 2;
    my $query = 'SELECT DISTINCT userid, login_name, realname, is_enabled, ' .
                $dbh->sql_date_format('last_seen_date', '%Y-%m-%d') . ' AS last_seen_date ' .
                'FROM profiles';
    my @bindValues;
    my $nextCondition;
    my $visibleGroups;

    # If a group ID is given, make sure it is a valid one.
    my $group;
    if ($grouprestrict) {
        $group = new Bugzilla::Group(scalar $cgi->param('groupid'));
        $group || ThrowUserError('invalid_group_ID');
    }

    if (!$editusers && Bugzilla->params->{'usevisibilitygroups'}) {
        # Show only users in visible groups.
        $visibleGroups = $user->visible_groups_as_string();

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
            $query .= qq{, user_group_map AS ugm
                         WHERE ugm.user_id = profiles.userid
                           AND ugm.isbless = 0
                        };
            $nextCondition = 'AND';
        }
        else {
            $nextCondition = 'WHERE';
        }
    }

    if (!$visibleGroups) {
        $vars->{'users'} = {};
    }
    else {
        # Handle selection by login name, real name, or userid.
        if (defined($matchtype)) {
            $query .= " $nextCondition ";
            my $expr = "";
            if ($matchvalue eq 'userid') {
                if ($matchstr) {
                    my $stored_matchstr = $matchstr;
                    detaint_natural($matchstr) 
                        || ThrowUserError('illegal_user_id', {userid => $stored_matchstr});
                }
                $expr = "profiles.userid";
            } elsif ($matchvalue eq 'realname') {
                $expr = "profiles.realname";
            } else {
                $expr = "profiles.login_name";
            }

            if ($matchtype =~ /^(regexp|notregexp|exact)$/) {
                $matchstr ||= '.';
            }
            else {
                $matchstr = '' unless defined $matchstr;
            }
            # We can trick_taint because we use the value in a SELECT only,
            # using a placeholder.
            trick_taint($matchstr);

            if ($matchtype eq 'regexp') {
                $query .= $dbh->sql_regexp($expr, '?', 0, $dbh->quote($matchstr));
            } elsif ($matchtype eq 'notregexp') {
                $query .= $dbh->sql_not_regexp($expr, '?', 0, $dbh->quote($matchstr));
            } elsif ($matchtype eq 'exact') {
                $query .= $expr . ' = ?';
            } else { # substr or unknown
                $query .= $dbh->sql_iposition('?', $expr) . ' > 0';
            }
            $nextCondition = 'AND';
            push(@bindValues, $matchstr);
        }

        # Handle selection by group.
        if ($grouprestrict eq '1') {
            my $grouplist = join(',',
                @{Bugzilla::Group->flatten_group_membership($group->id)});
            $query .= " $nextCondition ugm.group_id IN($grouplist) ";
        }

        detaint_natural($is_enabled);
        if ($is_enabled && ($is_enabled == 0 || $is_enabled == 1)) {
            $query .= " $nextCondition profiles.is_enabled = ?";
            $nextCondition = 'AND';
            push(@bindValues, $is_enabled);
        }

        $query .= ' ORDER BY profiles.login_name';

        $vars->{'users'} = $dbh->selectall_arrayref($query,
                                                    {'Slice' => {}},
                                                    @bindValues);

    }

    if ($matchtype && $matchtype eq 'exact' && scalar(@{$vars->{'users'}}) == 1) {
        my $match_user_id = $vars->{'users'}[0]->{'userid'};
        my $match_user = check_user($match_user_id);
        edit_processing($match_user);
    } else {
        $template->process('admin/users/list.html.tmpl', $vars)
            || ThrowTemplateError($template->error());
    }

###########################################################################
} elsif ($action eq 'add') {
    $editusers || ThrowUserError("auth_failure", {group  => "editusers",
                                                  action => "add",
                                                  object => "users"});

    $vars->{'token'} = issue_session_token('add_user');

    $template->process('admin/users/create.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

###########################################################################
} elsif ($action eq 'new') {
    $editusers || ThrowUserError("auth_failure", {group  => "editusers",
                                                  action => "add",
                                                  object => "users"});

    check_token_data($token, 'add_user');

    # When e.g. the 'Env' auth method is used, the password field
    # is not displayed. In that case, set the password to *.
    my $password = $cgi->param('password');
    $password = '*' if !defined $password;

    my $new_user = Bugzilla::User->create({
        login_name    => scalar $cgi->param('login'),
        cryptpassword => $password,
        realname      => scalar $cgi->param('name'),
        disabledtext  => scalar $cgi->param('disabledtext'),
        disable_mail  => scalar $cgi->param('disable_mail'),
        extern_id     => scalar $cgi->param('extern_id'),
        });

    userDataToVars($new_user->id);

    delete_token($token);

    if ($cgi->param('notify_user')) {
        $vars->{'new_user'} = $new_user;
        my $message;
      
        $template->process('email/new-user-details.txt.tmpl', $vars, \$message)
            || ThrowTemplateError($template->error());
        MessageToMTA($message);
    }

    # We already display the updated page. We have to recreate a token now.
    $vars->{'token'} = issue_session_token('edit_user');
    $vars->{'message'} = 'account_created';
    $template->process('admin/users/edit.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

###########################################################################
} elsif ($action eq 'edit') {
    my $otherUser = check_user($otherUserID, $otherUserLogin);
    edit_processing($otherUser);

###########################################################################
} elsif ($action eq 'update') {
    check_token_data($token, 'edit_user');
    my $otherUser = check_user($otherUserID, $otherUserLogin);
    $otherUserID = $otherUser->id;

    # Lock tables during the check+update session.
    $dbh->bz_start_transaction();

    $editusers || $user->can_see_user($otherUser)
        || ThrowUserError('auth_failure', {reason => "not_visible",
                                           action => "modify",
                                           object => "user"});

    $vars->{'loginold'} = $otherUser->login;

    # Update groups
    my @group_ids = grep { s/group_// } keys %{ Bugzilla->cgi->Vars };
    $otherUser->set_groups({ set => \@group_ids });

    # Update profiles table entry; silently skip doing this if the user
    # is not authorized.
    my $changes = {};
    if ($editusers) {
        $otherUser->set_login($cgi->param('login'));
        $otherUser->set_name($cgi->param('name'));
        $otherUser->set_password($cgi->param('password'))
            if $cgi->param('password');
        $otherUser->set_disabledtext($cgi->param('disabledtext'));
        $otherUser->set_disable_mail($cgi->param('disable_mail'));
        $otherUser->set_extern_id($cgi->param('extern_id'))
            if defined($cgi->param('extern_id'));

        # Update bless groups
        my @bless_ids = grep { s/bless_// } keys %{ Bugzilla->cgi->Vars };
        $otherUser->set_bless_groups({ set => \@bless_ids });
    }
    $changes = $otherUser->update();

    $dbh->bz_commit_transaction();

    # XXX: userDataToVars may be off when editing ourselves.
    userDataToVars($otherUserID);
    delete_token($token);

    $vars->{'message'} = 'account_updated';
    $vars->{'changes'} = \%$changes;
    # We already display the updated page. We have to recreate a token now.
    $vars->{'token'} = issue_session_token('edit_user');

    $template->process('admin/users/edit.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

###########################################################################
} elsif ($action eq 'del') {
    my $otherUser = check_user($otherUserID, $otherUserLogin);
    $otherUserID = $otherUser->id;

    Bugzilla->params->{'allowuserdeletion'} 
        || ThrowUserError('users_deletion_disabled');
    $editusers || ThrowUserError('auth_failure', {group  => "editusers",
                                                  action => "delete",
                                                  object => "users"});
    $vars->{'otheruser'}      = $otherUser;

    # Find other cross references.
    $vars->{'attachments'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM attachments WHERE submitter_id = ?',
        undef, $otherUserID);
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
    $vars->{'component_cc'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM component_cc WHERE user_id = ?',
        undef, $otherUserID);
    $vars->{'email_setting'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM email_setting WHERE user_id = ?',
        undef, $otherUserID);
    $vars->{'flags'}{'requestee'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM flags WHERE requestee_id = ?',
        undef, $otherUserID);
    $vars->{'flags'}{'setter'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM flags WHERE setter_id = ?',
        undef, $otherUserID);
    $vars->{'longdescs'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM longdescs WHERE who = ?',
        undef, $otherUserID);
    my $namedquery_ids = $dbh->selectcol_arrayref(
        'SELECT id FROM namedqueries WHERE userid = ?',
        undef, $otherUserID);
    $vars->{'namedqueries'} = scalar(@$namedquery_ids);
    if (scalar(@$namedquery_ids)) {
        $vars->{'namedquery_group_map'} = $dbh->selectrow_array(
            'SELECT COUNT(*) FROM namedquery_group_map WHERE namedquery_id IN' .
            ' (' . join(', ', @$namedquery_ids) . ')');
    }
    else {
        $vars->{'namedquery_group_map'} = 0;
    }
    $vars->{'profile_setting'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM profile_setting WHERE user_id = ?',
        undef, $otherUserID);
    $vars->{'profiles_activity'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM profiles_activity WHERE who = ? AND userid != ?',
        undef, ($otherUserID, $otherUserID));
    $vars->{'quips'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM quips WHERE userid = ?',
        undef, $otherUserID);
    $vars->{'series'} = $dbh->selectrow_array(
        'SELECT COUNT(*) FROM series WHERE creator = ?',
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
    $vars->{'token'} = issue_session_token('delete_user');

    $template->process('admin/users/confirm-delete.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

###########################################################################
} elsif ($action eq 'delete') {
    check_token_data($token, 'delete_user');
    my $otherUser = check_user($otherUserID, $otherUserLogin);
    $otherUserID = $otherUser->id;

    # Cache for user accounts.
    my %usercache = (0 => new Bugzilla::User());
    my %updatedbugs;

    # Lock tables during the check+removal session.
    # XXX: if there was some change on these tables after the deletion
    #      confirmation checks, we may do something here we haven't warned
    #      about.
    $dbh->bz_start_transaction();

    Bugzilla->params->{'allowuserdeletion'}
        || ThrowUserError('users_deletion_disabled');
    $editusers || ThrowUserError('auth_failure',
                                 {group  => "editusers",
                                  action => "delete",
                                  object => "users"});
    @{$otherUser->product_responsibilities()}
        && ThrowUserError('user_has_responsibility');

    Bugzilla->logout_user($otherUser);

    # Get the named query list so we can delete namedquery_group_map entries.
    my $namedqueries_as_string = join(', ', @{$dbh->selectcol_arrayref(
        'SELECT id FROM namedqueries WHERE userid = ?', undef, $otherUserID)});

    # Get the timestamp for LogActivityEntry.
    my $timestamp = $dbh->selectrow_array('SELECT NOW()');

    # When we update a bug_activity entry, we update the bug timestamp, too.
    my $sth_set_bug_timestamp =
        $dbh->prepare('UPDATE bugs SET delta_ts = ? WHERE bug_id = ?');

    # Flags
    my $flag_ids =
      $dbh->selectcol_arrayref('SELECT id FROM flags WHERE requestee_id = ?',
                                undef, $otherUserID);

    my $flags = Bugzilla::Flag->new_from_list($flag_ids);

    $dbh->do('UPDATE flags SET requestee_id = NULL, modification_date = ?
              WHERE requestee_id = ?', undef, ($timestamp, $otherUserID));

    # We want to remove the requestee but leave the requester alone,
    # so we have to log these changes manually.
    my %bugs;
    push(@{$bugs{$_->bug_id}->{$_->attach_id || 0}}, $_) foreach @$flags;
    foreach my $bug_id (keys %bugs) {
        foreach my $attach_id (keys %{$bugs{$bug_id}}) {
            my @old_summaries = Bugzilla::Flag->snapshot($bugs{$bug_id}->{$attach_id});
            $_->_set_requestee() foreach @{$bugs{$bug_id}->{$attach_id}};
            my @new_summaries = Bugzilla::Flag->snapshot($bugs{$bug_id}->{$attach_id});
            my ($removed, $added) =
                Bugzilla::Flag->update_activity(\@old_summaries, \@new_summaries);
            LogActivityEntry($bug_id, 'flagtypes.name', $removed, $added,
                $userid, $timestamp, undef, $attach_id);
        }
        $sth_set_bug_timestamp->execute($timestamp, $bug_id);
        $updatedbugs{$bug_id} = 1;
    }

    # Simple deletions in referred tables.
    $dbh->do('DELETE FROM email_setting WHERE user_id = ?', undef,
             $otherUserID);
    $dbh->do('DELETE FROM logincookies WHERE userid = ?', undef, $otherUserID);
    $dbh->do('DELETE FROM namedqueries WHERE userid = ?', undef, $otherUserID);
    $dbh->do('DELETE FROM namedqueries_link_in_footer WHERE user_id = ?', undef,
             $otherUserID);
    if ($namedqueries_as_string) {
        $dbh->do('DELETE FROM namedquery_group_map WHERE namedquery_id IN ' .
                 "($namedqueries_as_string)");
    }
    $dbh->do('DELETE FROM profile_setting WHERE user_id = ?', undef,
             $otherUserID);
    $dbh->do('DELETE FROM profiles_activity WHERE userid = ? OR who = ?', undef,
             ($otherUserID, $otherUserID));
    $dbh->do('DELETE FROM tokens WHERE userid = ?', undef, $otherUserID);
    $dbh->do('DELETE FROM user_group_map WHERE user_id = ?', undef,
             $otherUserID);
    $dbh->do('DELETE FROM watch WHERE watcher = ? OR watched = ?', undef,
             ($otherUserID, $otherUserID));

    # Deletions in referred tables which need LogActivityEntry.
    my $buglist = $dbh->selectcol_arrayref('SELECT bug_id FROM cc WHERE who = ?',
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
    my $sth_whineidFromEvents = $dbh->prepare(
           'SELECT id FROM whine_events WHERE owner_userid = ?');
    my $sth_deleteWhineEvent = $dbh->prepare(
           'DELETE FROM whine_events WHERE id = ?');
    my $sth_deleteWhineQuery = $dbh->prepare(
           'DELETE FROM whine_queries WHERE eventid = ?');
    my $sth_deleteWhineSchedule = $dbh->prepare(
           'DELETE FROM whine_schedules WHERE eventid = ?');

    $dbh->do('DELETE FROM whine_schedules WHERE mailto = ? AND mailto_type = ?',
             undef, ($otherUserID, MAILTO_USER));

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

    $dbh->bz_commit_transaction();
    delete_token($token);

    # It's complex to determine which items now need to be flushed from
    # memcached.  As user deletion is expected to be a rare event, we just
    # flush the entire cache when a user is deleted.
    Bugzilla->memcached->clear_all();

    $vars->{'message'} = 'account_deleted';
    $vars->{'otheruser'}{'login'} = $otherUser->login;
    $vars->{'restrictablegroups'} = $user->bless_groups();
    $template->process('admin/users/search.html.tmpl', $vars)
       || ThrowTemplateError($template->error());

    # Send mail about what we've done to bugs.
    # The deleted user is not notified of the changes.
    foreach (keys(%updatedbugs)) {
        Bugzilla::BugMail::Send($_, {'changer' => $user} );
    }

###########################################################################
} elsif ($action eq 'activity') {
    my $otherUser = check_user($otherUserID, $otherUserLogin);

    $vars->{'profile_changes'} = $dbh->selectall_arrayref(
        "SELECT profiles.login_name AS who, " .
                $dbh->sql_date_format('profiles_activity.profiles_when') . " AS activity_when,
                fielddefs.name AS what,
                profiles_activity.oldvalue AS removed,
                profiles_activity.newvalue AS added
         FROM profiles_activity
         INNER JOIN profiles ON profiles_activity.who = profiles.userid
         INNER JOIN fielddefs ON fielddefs.id = profiles_activity.fieldid
         WHERE profiles_activity.userid = ?
         ORDER BY profiles_activity.profiles_when",
        {'Slice' => {}},
        $otherUser->id);

    $vars->{'otheruser'} = $otherUser;

    $template->process("account/profile-activity.html.tmpl", $vars)
        || ThrowTemplateError($template->error());

###########################################################################
} else {
    ThrowUserError('unknown_action', {action => $action});
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
        $otherUser = new Bugzilla::User({ name => $otherUserLogin });
        $vars->{'user_login'} = $otherUserLogin;
    }
    ($otherUser && $otherUser->id) || ThrowUserError('invalid_user', $vars);

    return $otherUser;
}

# Copy incoming list selection values from CGI params to template variables.
sub mirrorListSelectionValues {
    my $cgi = Bugzilla->cgi;
    if (defined($cgi->param('matchtype'))) {
        foreach ('matchvalue', 'matchstr', 'matchtype',
                 'grouprestrict', 'groupid', 'is_enabled')
        {
            $vars->{'listselectionvalues'}{$_} = $cgi->param($_);
        }
    }
}

# Retrieve user data for the user editing form. User creation and user
# editing code rely on this to call derive_groups().
sub userDataToVars {
    my $otheruserid = shift;
    my $otheruser = new Bugzilla::User($otheruserid);
    my $query;
    my $user = Bugzilla->user;
    my $dbh = Bugzilla->dbh;

    my $grouplist = $otheruser->groups_as_string;

    $vars->{'otheruser'} = $otheruser;
    $vars->{'groups'} = $user->bless_groups();

    $vars->{'permissions'} = $dbh->selectall_hashref(
        qq{SELECT id,
                  COUNT(directmember.group_id) AS directmember,
                  COUNT(regexpmember.group_id) AS regexpmember,
                  (CASE WHEN (groups.id IN ($grouplist)
                              AND COUNT(directmember.group_id) = 0
                              AND COUNT(regexpmember.group_id) = 0
                             ) THEN 1 ELSE 0 END) 
                      AS derivedmember,
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
           LEFT JOIN user_group_map AS directbless
                  ON directbless.group_id = id
                 AND directbless.user_id = ?
                 AND directbless.isbless = 1
                 AND directbless.grant_type = ?
          } . $dbh->sql_group_by('id'),
        'id', undef,
        ($otheruserid, GRANT_DIRECT,
         $otheruserid, GRANT_REGEXP,
         $otheruserid, GRANT_DIRECT));

    # Find indirect bless permission.
    $query = qq{SELECT groups.id
                FROM groups, group_group_map AS ggm
                WHERE groups.id = ggm.grantor_id
                  AND ggm.member_id IN ($grouplist)
                  AND ggm.grant_type = ?
               } . $dbh->sql_group_by('id');
    foreach (@{$dbh->selectall_arrayref($query, undef,
                                        (GROUP_BLESS))}) {
        # Merge indirect bless permissions into permission variable.
        $vars->{'permissions'}{${$_}[0]}{'indirectbless'} = 1;
    }
}

sub edit_processing {
    my $otherUser = shift;
    my $user = Bugzilla->user;
    my $template = Bugzilla->template;

    $user->in_group('editusers') || $user->can_see_user($otherUser)
        || ThrowUserError('auth_failure', {reason => "not_visible",
                                           action => "modify",
                                           object => "user"});

    userDataToVars($otherUser->id);
    $vars->{'token'} = issue_session_token('edit_user');

    $template->process('admin/users/edit.html.tmpl', $vars)
       || ThrowTemplateError($template->error());
}
