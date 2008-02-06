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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Alan Raetz <al_raetz@yahoo.com>
#                 David Miller <justdave@syndicomm.com>
#                 Christopher Aillon <christopher@aillon.com>
#                 Gervase Markham <gerv@gerv.net>
#                 Vlad Dascalu <jocuri@softhome.net>
#                 Shane H. W. Travis <travis@sedsystems.ca>

use strict;

use lib qw(.);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Search;
use Bugzilla::Util;
use Bugzilla::User;

require "CGI.pl";

# Use global template variables.
use vars qw($template $vars $userid);

###############################################################################
# Each panel has two functions - panel Foo has a DoFoo, to get the data 
# necessary for displaying the panel, and a SaveFoo, to save the panel's 
# contents from the form data (if appropriate.) 
# SaveFoo may be called before DoFoo.    
###############################################################################
sub DoAccount {
    my $dbh = Bugzilla->dbh;
    SendSQL("SELECT realname FROM profiles WHERE userid = $userid");
    $vars->{'realname'} = FetchSQLData();

    if(Param('allowemailchange')) {
        SendSQL("SELECT tokentype, issuedate + " . $dbh->sql_interval(3, 'DAY') .
                ", eventdata
                    FROM tokens
                    WHERE userid = $userid
                    AND tokentype LIKE 'email%' 
                    ORDER BY tokentype ASC " . $dbh->sql_limit(1));
        if(MoreSQLData()) {
            my ($tokentype, $change_date, $eventdata) = &::FetchSQLData();
            $vars->{'login_change_date'} = $change_date;

            if($tokentype eq 'emailnew') {
                my ($oldemail,$newemail) = split(/:/,$eventdata);
                $vars->{'new_login_name'} = $newemail;
            }
        }
    }
}

sub SaveAccount {
    my $cgi = Bugzilla->cgi;

    my $pwd1 = $cgi->param('new_password1');
    my $pwd2 = $cgi->param('new_password2');

    if ($cgi->param('Bugzilla_password') ne "" || 
        $pwd1 ne "" || $pwd2 ne "") 
    {
        my $old = SqlQuote($cgi->param('Bugzilla_password'));
        SendSQL("SELECT cryptpassword FROM profiles WHERE userid = $userid");
        my $oldcryptedpwd = FetchOneColumn();
        $oldcryptedpwd || ThrowCodeError("unable_to_retrieve_password");

        if (crypt($cgi->param('Bugzilla_password'), $oldcryptedpwd) ne 
                  $oldcryptedpwd) 
        {
            ThrowUserError("old_password_incorrect");
        }

        if ($pwd1 ne "" || $pwd2 ne "")
        {
            $cgi->param('new_password1')
              || ThrowUserError("new_password_missing");
            ValidatePassword($pwd1, $pwd2);
        
            my $cryptedpassword = SqlQuote(bz_crypt($pwd1));
            SendSQL("UPDATE profiles 
                     SET    cryptpassword = $cryptedpassword 
                     WHERE  userid = $userid");

            # Invalidate all logins except for the current one
            Bugzilla->logout(LOGOUT_KEEP_CURRENT);
        }
    }

    if(Param("allowemailchange") && $cgi->param('new_login_name')) {
        my $old_login_name = $cgi->param('Bugzilla_login');
        my $new_login_name = trim($cgi->param('new_login_name'));

        if($old_login_name ne $new_login_name) {
            $cgi->param('Bugzilla_password') 
              || ThrowUserError("old_password_required");

            use Bugzilla::Token;
            # Block multiple email changes for the same user.
            if (Bugzilla::Token::HasEmailChangeToken($userid)) {
                ThrowUserError("email_change_in_progress");
            }

            # Before changing an email address, confirm one does not exist.
            CheckEmailSyntax($new_login_name);
            trick_taint($new_login_name);
            is_available_username($new_login_name)
              || ThrowUserError("account_exists", {email => $new_login_name});

            Bugzilla::Token::IssueEmailChangeToken($userid,$old_login_name,
                                                 $new_login_name);

            $vars->{'email_changes_saved'} = 1;
        }
    }

    SendSQL("UPDATE profiles SET " .
            "realname = " . SqlQuote(trim($cgi->param('realname'))) .
            " WHERE userid = $userid");
}


sub DoSettings {
    $vars->{'settings'} = Bugzilla->user->settings;

    my @setting_list = keys %{Bugzilla->user->settings};
    $vars->{'setting_names'} = \@setting_list;
}

sub SaveSettings {
    my $cgi = Bugzilla->cgi;

    my $settings = Bugzilla->user->settings;
    my @setting_list = keys %{Bugzilla->user->settings};

    foreach my $name (@setting_list) {
        next if ! ($settings->{$name}->{'is_enabled'});
        my $value = $cgi->param($name);
        my $setting = new Bugzilla::User::Setting($name);

        if ($value eq "${name}-isdefault" ) {
            if (! $settings->{$name}->{'is_default'}) {
                $settings->{$name}->reset_to_default;
            }
        }
        else {
            $setting->validate_value($value);
            $settings->{$name}->set($value);
        }
    }
    $vars->{'settings'} = Bugzilla->user->settings(1);
}

sub DoEmail {
    my $dbh = Bugzilla->dbh;
    
    ###########################################################################
    # User watching
    ###########################################################################
    if (Param("supportwatchers")) {
        my $watched_ref = $dbh->selectcol_arrayref(
            "SELECT profiles.login_name FROM watch INNER JOIN profiles" .
            " ON watch.watched = profiles.userid" .
            " WHERE watcher = ?",
            undef, $userid);
        $vars->{'watchedusers'} = join(',', @$watched_ref);

        my $watcher_ids = $dbh->selectcol_arrayref(
            "SELECT watcher FROM watch WHERE watched = ?",
            undef, $userid);

        my @watchers;
        foreach my $watcher_id (@$watcher_ids) {
            my $watcher = new Bugzilla::User($watcher_id);
            push (@watchers, Bugzilla::User::identity($watcher));
        }

        @watchers = sort { lc($a) cmp lc($b) } @watchers;
        $vars->{'watchers'} = \@watchers;
    }

    ###########################################################################
    # Role-based preferences
    ###########################################################################
    my $sth = Bugzilla->dbh->prepare("SELECT relationship, event " . 
                                     "FROM email_setting " . 
                                     "WHERE user_id = $userid");
    $sth->execute();

    my %mail;
    while (my ($relationship, $event) = $sth->fetchrow_array()) {
        $mail{$relationship}{$event} = 1;
    }

    $vars->{'mail'} = \%mail;      
}

sub SaveEmail {
    my $dbh = Bugzilla->dbh;
    my $cgi = Bugzilla->cgi;
    
    ###########################################################################
    # Role-based preferences
    ###########################################################################
    $dbh->bz_lock_tables("email_setting WRITE");

    # Delete all the user's current preferences
    $dbh->do("DELETE FROM email_setting WHERE user_id = $userid");

    # Repopulate the table - first, with normal events in the 
    # relationship/event matrix.
    # Note: the database holds only "off" email preferences, as can be implied 
    # from the name of the table - profiles_nomail.
    foreach my $rel (RELATIONSHIPS) {
        # Positive events: a ticked box means "send me mail."
        foreach my $event (POS_EVENTS) {
            if (defined($cgi->param("email-$rel-$event"))
                && $cgi->param("email-$rel-$event") == 1)
            {
                $dbh->do("INSERT INTO email_setting " . 
                         "(user_id, relationship, event) " . 
                         "VALUES ($userid, $rel, $event)");
            }
        }
        
        # Negative events: a ticked box means "don't send me mail."
        foreach my $event (NEG_EVENTS) {
            if (!defined($cgi->param("neg-email-$rel-$event")) ||
                $cgi->param("neg-email-$rel-$event") != 1) 
            {
                $dbh->do("INSERT INTO email_setting " . 
                         "(user_id, relationship, event) " . 
                         "VALUES ($userid, $rel, $event)");
            }
        }
    }

    # Global positive events: a ticked box means "send me mail."
    foreach my $event (GLOBAL_EVENTS) {
        if (defined($cgi->param("email-" . REL_ANY . "-$event"))
            && $cgi->param("email-" . REL_ANY . "-$event") == 1)
        {
            $dbh->do("INSERT INTO email_setting " . 
                     "(user_id, relationship, event) " . 
                     "VALUES ($userid, " . REL_ANY . ", $event)");
        }
    }

    $dbh->bz_unlock_tables();

    ###########################################################################
    # User watching
    ###########################################################################
    if (Param("supportwatchers") && defined $cgi->param('watchedusers')) {
        # Just in case.  Note that this much locking is actually overkill:
        # we don't really care if anyone reads the watch table.  So 
        # some small amount of contention could be gotten rid of by
        # using user-defined locks rather than table locking.
        $dbh->bz_lock_tables('watch WRITE', 'profiles READ');

        # what the db looks like now
        my $old_watch_ids =
            $dbh->selectcol_arrayref("SELECT watched FROM watch"
                                   . " WHERE watcher = ?", undef, $userid);
 
       # The new information given to us by the user.
        my @new_watch_names = split(/[,\s]+/, $cgi->param('watchedusers'));
        my %new_watch_ids;
        foreach my $username (@new_watch_names) {
            my $watched_userid = DBNameToIdAndCheck(trim($username));
            $new_watch_ids{$watched_userid} = 1;
        }
        my ($removed, $added) = diff_arrays($old_watch_ids, [keys %new_watch_ids]);

        # Remove people who were removed.
        my $delete_sth = $dbh->prepare('DELETE FROM watch WHERE watched = ?'
                                     . ' AND watcher = ?');
        foreach my $remove_me (@$removed) {
            $delete_sth->execute($remove_me, $userid);
        }

        # Add people who were added.
        my $insert_sth = $dbh->prepare('INSERT INTO watch (watched, watcher)'
                                     . ' VALUES (?, ?)');
        foreach my $add_me (@$added) {
            $insert_sth->execute($add_me, $userid);
        }

        $dbh->bz_unlock_tables();
    }
}


sub DoPermissions {
    my (@has_bits, @set_bits);
    
    SendSQL("SELECT DISTINCT name, description FROM groups " .
            "INNER JOIN user_group_map " .
            "ON user_group_map.group_id = groups.id " .
            "WHERE user_id = $::userid " .
            "AND isbless = 0 " .
            "ORDER BY name");
    while (MoreSQLData()) {
        my ($nam, $desc) = FetchSQLData();
        push(@has_bits, {"desc" => $desc, "name" => $nam});
    }
    my @set_ids = ();
    SendSQL("SELECT DISTINCT name, description FROM groups " .
            "ORDER BY name");
    while (MoreSQLData()) {
        my ($nam, $desc) = FetchSQLData();
        if (Bugzilla->user->can_bless($nam)) {
            push(@set_bits, {"desc" => $desc, "name" => $nam});
        }
    }
    
    $vars->{'has_bits'} = \@has_bits;
    $vars->{'set_bits'} = \@set_bits;    
}

# No SavePermissions() because this panel has no changeable fields.


sub DoSavedSearches() {
    # 2004-12-13 - colin.ogilvie@gmail.com, bug 274397
    # Need to work around the possibly missing query_format=advanced
    $vars->{'user'} = Bugzilla->user;
    my @queries = @{Bugzilla->user->queries};
    my @newqueries;
    foreach my $q (@queries) {
        if ($q->{'query'} =~ /query_format=([^&]*)/) {
            my $format = $1;
            if (!IsValidQueryType($format)) {
                if ($format eq "") {
                    $q->{'query'} =~ s/query_format=/query_format=advanced/;
                }
                else {
                    $q->{'query'} .= '&query_format=advanced';
                }
            }
        } else {
            $q->{'query'} .= '&query_format=advanced';
        }
        push @newqueries, $q;
    }
    $vars->{'queries'} = \@newqueries;
}

sub SaveSavedSearches() {
    my $cgi = Bugzilla->cgi;
    my $dbh = Bugzilla->dbh;
    my @queries = @{Bugzilla->user->queries};
    my $sth = $dbh->prepare("UPDATE namedqueries SET linkinfooter = ?
                          WHERE userid = ?
                          AND name = ?");
    foreach my $q (@queries) {
        my $linkinfooter = 
            defined($cgi->param("linkinfooter_$q->{'name'}")) ? 1 : 0;
            $sth->execute($linkinfooter, $userid, $q->{'name'});
    }

    Bugzilla->user->flush_queries_cache;
    
    my $showmybugslink = defined($cgi->param("showmybugslink")) ? 1 : 0;
    $dbh->do("UPDATE profiles SET mybugslink = $showmybugslink " . 
             "WHERE userid = " . Bugzilla->user->id);    
    Bugzilla->user->{'showmybugslink'} = $showmybugslink;
}


###############################################################################
# Live code (not subroutine definitions) starts here
###############################################################################

my $cgi = Bugzilla->cgi;

# This script needs direct access to the username and password CGI variables,
# so we save them before their removal in Bugzilla->login
my $bugzilla_login    = $cgi->param('Bugzilla_login');
my $bugzilla_password = $cgi->param('Bugzilla_password');

Bugzilla->login(LOGIN_REQUIRED);
$cgi->param('Bugzilla_login', $bugzilla_login);
$cgi->param('Bugzilla_password', $bugzilla_password);

GetVersionTable();

$vars->{'changes_saved'} = $cgi->param('dosave');

my $current_tab_name = $cgi->param('tab') || "account";

# The SWITCH below makes sure that this is valid
trick_taint($current_tab_name);

$vars->{'current_tab_name'} = $current_tab_name;

# Do any saving, and then display the current tab.
SWITCH: for ($current_tab_name) {
    /^account$/ && do {
        SaveAccount() if $cgi->param('dosave');
        DoAccount();
        last SWITCH;
    };
    /^settings$/ && do {
        SaveSettings() if $cgi->param('dosave');
        DoSettings();
        last SWITCH;
    };
    /^email$/ && do {
        SaveEmail() if $cgi->param('dosave');
        DoEmail();
        last SWITCH;
    };
    /^permissions$/ && do {
        DoPermissions();
        last SWITCH;
    };
    /^saved-searches$/ && do {
        SaveSavedSearches() if $cgi->param('dosave');
        DoSavedSearches();
        last SWITCH;
    };
    ThrowUserError("unknown_tab",
                   { current_tab_name => $current_tab_name });
}

# Generate and return the UI (HTML page) from the appropriate template.
print $cgi->header();
$template->process("account/prefs/prefs.html.tmpl", $vars)
  || ThrowTemplateError($template->error());
