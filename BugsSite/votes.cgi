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
#                 Stephan Niemz  <st.n@gmx.net>
#                 Christopher Aillon <christopher@aillon.com>
#                 Gervase Markham <gerv@gerv.net>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Bug;

require "CGI.pl";

# Use global template variables
use vars qw($template $vars);

my $cgi = Bugzilla->cgi;

# If the action is show_bug, you need a bug_id.
# If the action is show_user, you can supply a userid to show the votes for
# another user, otherwise you see your own.
# If the action is vote, your votes are set to those encoded in the URL as 
# <bug_id>=<votes>.
#
# If no action is defined, we default to show_bug if a bug_id is given,
# otherwise to show_user.
my $bug_id = $cgi->param('bug_id');
my $action = $cgi->param('action') || ($bug_id ? "show_bug" : "show_user");

if ($action eq "show_bug" ||
    ($action eq "show_user" && defined $cgi->param('user')))
{
    Bugzilla->login();
}
else {
    Bugzilla->login(LOGIN_REQUIRED);
}

################################################################################
# Begin Data/Security Validation
################################################################################

# Make sure the bug ID is a positive integer representing an existing
# bug that the user is authorized to access.

ValidateBugID($bug_id) if defined $bug_id;

################################################################################
# End Data/Security Validation
################################################################################

if ($action eq "show_bug") {
    show_bug();
} 
elsif ($action eq "show_user") {
    show_user();
}
elsif ($action eq "vote") {
    record_votes() if Param('usevotes');
    show_user();
}
else {
    ThrowCodeError("unknown_action", {action => $action});
}

exit;

# Display the names of all the people voting for this one bug.
sub show_bug {
    my $cgi = Bugzilla->cgi;

    ThrowCodeError("missing_bug_id") unless defined $bug_id;

    my $total = 0;
    my @users;
    
    SendSQL("SELECT profiles.login_name, votes.who, votes.vote_count 
               FROM votes INNER JOIN profiles 
                 ON profiles.userid = votes.who
              WHERE votes.bug_id = $bug_id");
                   
    while (MoreSQLData()) {
        my ($name, $userid, $count) = (FetchSQLData());
        push (@users, { name => $name, id => $userid, count => $count });
        $total += $count;
    }
    
    $vars->{'bug_id'} = $bug_id;
    $vars->{'users'} = \@users;
    $vars->{'total'} = $total;
    
    print $cgi->header();
    $template->process("bug/votes/list-for-bug.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

# Display all the votes for a particular user. If it's the user
# doing the viewing, give them the option to edit them too.
sub show_user {
    GetVersionTable();

    my $cgi = Bugzilla->cgi;
    my $dbh = Bugzilla->dbh;

    # If a bug_id is given, and we're editing, we'll add it to the votes list.
    $bug_id ||= "";
    
    my $name = $cgi->param('user') || Bugzilla->user->login;
    my $who = DBNameToIdAndCheck($name);
    my $userid = Bugzilla->user->id;
    
    my $canedit = (Param('usevotes') && $userid == $who) ? 1 : 0;

    $dbh->bz_lock_tables('bugs READ', 'products READ', 'votes WRITE',
             'cc READ', 'bug_group_map READ', 'user_group_map READ',
             'cc AS selectVisible_cc READ', 'groups READ');

    if ($canedit && $bug_id) {
        # Make sure there is an entry for this bug
        # in the vote table, just so that things display right.
        SendSQL("SELECT votes.vote_count FROM votes 
                 WHERE votes.bug_id = $bug_id AND votes.who = $who");
        if (!FetchOneColumn()) {
            SendSQL("INSERT INTO votes (who, bug_id, vote_count) 
                     VALUES ($who, $bug_id, 0)");
        }
    }
    
    # Calculate the max votes per bug for each product; doing it here means
    # we can do it all in one query.
    my %maxvotesperbug;
    if($canedit) {
        SendSQL("SELECT products.name, products.maxvotesperbug 
                 FROM products");
        while (MoreSQLData()) {
            my ($prod, $max) = FetchSQLData();
            $maxvotesperbug{$prod} = $max;
        }
    }
    
    my @products;
    
    # Read the votes data for this user for each product
    foreach my $product (sort(keys(%::prodmaxvotes))) {
        next if $::prodmaxvotes{$product} <= 0;
        
        my @bugs;
        my $total = 0;
        my $onevoteonly = 0;
        
        SendSQL("SELECT votes.bug_id, votes.vote_count, bugs.short_desc,
                        bugs.bug_status 
                  FROM  votes
                  INNER JOIN bugs ON votes.bug_id = bugs.bug_id
                  INNER JOIN products ON bugs.product_id = products.id 
                  WHERE votes.who = $who 
                    AND products.name = " . SqlQuote($product) . 
                 "ORDER BY votes.bug_id");        
        
        while (MoreSQLData()) {
            my ($id, $count, $summary, $status) = FetchSQLData();
            next if !defined($status);
            $total += $count;
             
            # Next if user can't see this bug. So, the totals will be correct
            # and they can see there are votes 'missing', but not on what bug
            # they are. This seems a reasonable compromise; the alternative is
            # to lie in the totals.
            next if !Bugzilla->user->can_see_bug($id);            
            
            push (@bugs, { id => $id, 
                           summary => $summary,
                           count => $count,
                           opened => IsOpenedState($status) });
        }
        
        # In case we didn't populate this earlier (i.e. an error, or
        # a not logged in user viewing a users votes)
        $maxvotesperbug{$product} ||= 0;

        $onevoteonly = 1 if (min($::prodmaxvotes{$product},
                                 $maxvotesperbug{$product}) == 1);
        
        # Only add the product for display if there are any bugs in it.
        if ($#bugs > -1) {                         
            push (@products, { name => $product,
                               bugs => \@bugs,
                               onevoteonly => $onevoteonly,
                               total => $total,
                               maxvotes => $::prodmaxvotes{$product},
                               maxperbug => $maxvotesperbug{$product} });
        }
    }

    SendSQL("DELETE FROM votes WHERE vote_count <= 0");
    $dbh->bz_unlock_tables();

    $vars->{'canedit'} = $canedit;
    $vars->{'voting_user'} = { "login" => $name };
    $vars->{'products'} = \@products;
    $vars->{'bug_id'} = $bug_id;

    print $cgi->header();
    $template->process("bug/votes/list-for-user.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

# Update the user's votes in the database.
sub record_votes {
    ############################################################################
    # Begin Data/Security Validation
    ############################################################################

    my $cgi = Bugzilla->cgi;
    my $dbh = Bugzilla->dbh;

    # Build a list of bug IDs for which votes have been submitted.  Votes
    # are submitted in form fields in which the field names are the bug 
    # IDs and the field values are the number of votes.

    my @buglist = grep {/^[1-9][0-9]*$/} $cgi->param();

    # If no bugs are in the buglist, let's make sure the user gets notified
    # that their votes will get nuked if they continue.
    if (scalar(@buglist) == 0) {
        if (!defined $cgi->param('delete_all_votes')) {
            print $cgi->header();
            $template->process("bug/votes/delete-all.html.tmpl", $vars)
              || ThrowTemplateError($template->error());
            exit();
        }
        elsif ($cgi->param('delete_all_votes') == 0) {
            print $cgi->redirect("votes.cgi");
            exit();
        }
    }

    # Call ValidateBugID on each bug ID to make sure it is a positive
    # integer representing an existing bug that the user is authorized 
    # to access, and make sure the number of votes submitted is also
    # a non-negative integer (a series of digits not preceded by a
    # minus sign).
    my %votes;
    foreach my $id (@buglist) {
      ValidateBugID($id);
      $votes{$id} = $cgi->param($id);
      detaint_natural($votes{$id}) 
        || ThrowUserError("votes_must_be_nonnegative");
    }

    ############################################################################
    # End Data/Security Validation
    ############################################################################

    GetVersionTable();

    my $who = Bugzilla->user->id;

    # If the user is voting for bugs, make sure they aren't overstuffing
    # the ballot box.
    if (scalar(@buglist)) {
        SendSQL("SELECT bugs.bug_id, products.name, products.maxvotesperbug
                   FROM bugs
             INNER JOIN products
                     ON products.id = bugs.product_id
                  WHERE bugs.bug_id IN (" . join(", ", @buglist) . ")");

        my %prodcount;

        while (MoreSQLData()) {
            my ($id, $prod, $max) = FetchSQLData();
            $prodcount{$prod} ||= 0;
            $prodcount{$prod} += $votes{$id};
            
            # Make sure we haven't broken the votes-per-bug limit
            ($votes{$id} <= $max)               
              || ThrowUserError("too_many_votes_for_bug",
                                {max => $max, 
                                 product => $prod, 
                                 votes => $votes{$id}});
        }

        # Make sure we haven't broken the votes-per-product limit
        foreach my $prod (keys(%prodcount)) {
            ($prodcount{$prod} <= $::prodmaxvotes{$prod})
              || ThrowUserError("too_many_votes_for_product",
                                {max => $::prodmaxvotes{$prod}, 
                                 product => $prod, 
                                 votes => $prodcount{$prod}});
        }
    }

    # Update the user's votes in the database.  If the user did not submit 
    # any votes, they may be using a form with checkboxes to remove all their
    # votes (checkboxes are not submitted along with other form data when
    # they are not checked, and Bugzilla uses them to represent single votes
    # for products that only allow one vote per bug).  In that case, we still
    # need to clear the user's votes from the database.
    my %affected;
    $dbh->bz_lock_tables('bugs WRITE', 'bugs_activity WRITE',
                         'votes WRITE', 'longdescs WRITE',
                         'products READ', 'fielddefs READ');
    
    # Take note of, and delete the user's old votes from the database.
    SendSQL("SELECT bug_id FROM votes WHERE who = $who");
    while (MoreSQLData()) {
        my $id = FetchOneColumn();
        $affected{$id} = 1;
    }
    
    SendSQL("DELETE FROM votes WHERE who = $who");
    
    # Insert the new values in their place
    foreach my $id (@buglist) {
        if ($votes{$id} > 0) {
            SendSQL("INSERT INTO votes (who, bug_id, vote_count) 
                     VALUES ($who, $id, ".$votes{$id}.")");
        }
        $affected{$id} = 1;
    }
    
    # Update the cached values in the bugs table
    print $cgi->header();
    my @updated_bugs = ();

    my $sth_getVotes = $dbh->prepare("SELECT SUM(vote_count) FROM votes
                                      WHERE bug_id = ?");

    my $sth_updateVotes = $dbh->prepare("UPDATE bugs SET votes = ?
                                         WHERE bug_id = ?");

    foreach my $id (keys %affected) {
        $sth_getVotes->execute($id);
        my $v = $sth_getVotes->fetchrow_array || 0;
        $sth_updateVotes->execute($v, $id);

        my $confirmed = CheckIfVotedConfirmed($id, $who);
        push (@updated_bugs, $id) if $confirmed;
    }
    $dbh->bz_unlock_tables();

    $vars->{'type'} = "votes";
    $vars->{'mailrecipients'} = { 'changer' => $who };

    foreach my $bug_id (@updated_bugs) {
        $vars->{'id'} = $bug_id;
        $template->process("bug/process/results.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        # Set header_done to 1 only after the first bug.
        $vars->{'header_done'} = 1;
    }
    $vars->{'votes_recorded'} = 1;
}
