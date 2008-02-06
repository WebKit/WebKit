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
# Contributor(s): Myk Melez <myk@mozilla.org>
#                 Erik Stambaugh <erik@dasbistro.com>
#                 Bradley Baetz <bbaetz@acm.org>
#                 Joel Peshkin <bugreport@peshkin.net> 
#                 Byron Jones <bugzilla@glob.com.au>
#                 Shane H. W. Travis <travis@sedsystems.ca>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Gervase Markham <gerv@gerv.net>
#                 Justin C. De Vries <judevries@novell.com>

################################################################################
# Module Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

# This module implements utilities for dealing with Bugzilla users.
package Bugzilla::User;

use Bugzilla::Config;
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Constants;
use Bugzilla::User::Setting;

use base qw(Exporter);
@Bugzilla::User::EXPORT = qw(insert_new_user is_available_username
    login_to_id
    UserInGroup
    USER_MATCH_MULTIPLE USER_MATCH_FAILED USER_MATCH_SUCCESS
    MATCH_SKIP_CONFIRM
);

#####################################################################
# Constants
#####################################################################

use constant USER_MATCH_MULTIPLE => -1;
use constant USER_MATCH_FAILED   => 0;
use constant USER_MATCH_SUCCESS  => 1;

use constant MATCH_SKIP_CONFIRM  => 1;

################################################################################
# Functions
################################################################################

sub new {
    my $invocant = shift;
    my ($user_id, $tables_locked) = @_;

    if ($user_id) {
        my $uid = $user_id;
        detaint_natural($user_id)
          || ThrowCodeError('invalid_numeric_argument',
                            {argument => 'userID',
                             value    => $uid,
                             function => 'Bugzilla::User::new'});
        return $invocant->_create('userid = ?', $user_id, $tables_locked);
    }
    else {
        return $invocant->_create;
    }
}

# This routine is sort of evil. Nothing except the login stuff should
# be dealing with addresses as an input, and they can get the id as a
# side effect of the other sql they have to do anyway.
# Bugzilla::BugMail still does this, probably as a left over from the
# pre-id days. Provide this as a helper, but don't document it, and hope
# that it can go away.
# The request flag stuff also does this, but it really should be passing
# in the id its already had to validate (or the User.pm object, of course)
sub new_from_login {
    my $invocant = shift;
    my ($login, $tables_locked) = @_;

    my $dbh = Bugzilla->dbh;
    return $invocant->_create($dbh->sql_istrcmp('login_name', '?'),
                              $login, $tables_locked);
}

# Internal helper for the above |new| methods
# $cond is a string (including a placeholder ?) for the search
# requirement for the profiles table
sub _create {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;

    my $cond = shift;
    my $val = shift;

    # Allow invocation with no parameters to create a blank object
    my $self = {
        'id'             => 0,
        'name'           => '',
        'login'          => '',
        'showmybugslink' => 0,
        'disabledtext'   => '',
        'flags'          => {},
    };
    bless ($self, $class);
    return $self unless $cond && $val;

    # We're checking for validity here, so any value is OK
    trick_taint($val);

    my $tables_locked_for_derive_groups = shift;

    my $dbh = Bugzilla->dbh;

    my ($id,
        $login,
        $name,
        $disabledtext,
        $mybugslink) = $dbh->selectrow_array(qq{SELECT userid,
                                                       login_name,
                                                       realname,
                                                       disabledtext,
                                                       mybugslink
                                                 FROM profiles
                                                 WHERE $cond},
                                             undef,
                                             $val);

    return undef unless defined $id;

    $self->{'id'}             = $id;
    $self->{'name'}           = $name;
    $self->{'login'}          = $login;
    $self->{'disabledtext'}   = $disabledtext;
    $self->{'showmybugslink'} = $mybugslink;

    # Now update any old group information if needed
    my $result = $dbh->selectrow_array(q{SELECT 1
                                           FROM profiles, groups
                                          WHERE userid=?
                                            AND profiles.refreshed_when <=
                                                  groups.last_changed},
                                       undef,
                                       $id);

    if ($result) {
        my $is_main_db;
        unless ($is_main_db = Bugzilla->dbwritesallowed()) {
            $dbh = Bugzilla->switch_to_main_db();
        }
        $self->derive_groups($tables_locked_for_derive_groups);
        unless ($is_main_db) {
            $dbh = Bugzilla->switch_to_shadow_db();
        }
    }

    return $self;
}

# Accessors for user attributes
sub id { $_[0]->{id}; }
sub login { $_[0]->{login}; }
sub email { $_[0]->{login} . Param('emailsuffix'); }
sub name { $_[0]->{name}; }
sub disabledtext { $_[0]->{'disabledtext'}; }
sub is_disabled { $_[0]->disabledtext ? 1 : 0; }
sub showmybugslink { $_[0]->{showmybugslink}; }

sub set_flags {
    my $self = shift;
    while (my $key = shift) {
        $self->{'flags'}->{$key} = shift;
    }
}

sub get_flag {
    my $self = shift;
    my $key = shift;
    return $self->{'flags'}->{$key};
}

# Generate a string to identify the user by name + login if the user
# has a name or by login only if she doesn't.
sub identity {
    my $self = shift;

    return "" unless $self->id;

    if (!defined $self->{identity}) {
        $self->{identity} = 
          $self->{name} ? "$self->{name} <$self->{login}>" : $self->{login};
    }

    return $self->{identity};
}

sub nick {
    my $self = shift;

    return "" unless $self->id;

    if (!defined $self->{nick}) {
        $self->{nick} = (split(/@/, $self->{login}, 2))[0];
    }

    return $self->{nick};
}

sub queries {
    my $self = shift;

    return $self->{queries} if defined $self->{queries};
    return [] unless $self->id;

    my $dbh = Bugzilla->dbh;
    my $used_in_whine_ref = $dbh->selectcol_arrayref(q{
                    SELECT DISTINCT query_name
                      FROM whine_events we
                INNER JOIN whine_queries wq
                        ON we.id = wq.eventid
                     WHERE we.owner_userid = ?}, undef, $self->{id});
    
    my $queries_ref = $dbh->selectall_arrayref(q{
                    SELECT name, query, linkinfooter
                      FROM namedqueries 
                     WHERE userid = ?
                  ORDER BY UPPER(name)},{'Slice'=>{}}, $self->{id});

    foreach my $name (@$used_in_whine_ref) { 
        foreach my $queries_hash (@$queries_ref) {
            if ($queries_hash->{name} eq $name) {
                $queries_hash->{usedinwhine} = 1;
                last;
            }
        }
    }
    $self->{queries} = $queries_ref;

    return $self->{queries};
}

sub settings {
    my ($self) = @_;

    return $self->{'settings'} if (defined $self->{'settings'});

    # IF the user is logged in
    # THEN get the user's settings
    # ELSE get default settings
    if ($self->id) {
        $self->{'settings'} = get_all_settings($self->id);
    } else {
        $self->{'settings'} = get_defaults();
    }

    return $self->{'settings'};
}

sub flush_queries_cache {
    my $self = shift;

    delete $self->{queries};
}

sub groups {
    my $self = shift;

    return $self->{groups} if defined $self->{groups};
    return {} unless $self->id;

    my $dbh = Bugzilla->dbh;
    my $groups = $dbh->selectcol_arrayref(q{SELECT DISTINCT groups.name, group_id
                                              FROM groups, user_group_map
                                             WHERE groups.id=user_group_map.group_id
                                               AND user_id=?
                                               AND isbless=0},
                                          { Columns=>[1,2] },
                                          $self->{id});

    # The above gives us an arrayref [name, id, name, id, ...]
    # Convert that into a hashref
    my %groups = @$groups;
    $self->{groups} = \%groups;

    return $self->{groups};
}

sub bless_groups {
    my $self = shift;

    return $self->{'bless_groups'} if defined $self->{'bless_groups'};
    return [] unless $self->id;

    my $dbh = Bugzilla->dbh;
    my $query;
    my $connector;
    my @bindValues;

    if ($self->in_group('editusers')) {
        # Users having editusers permissions may bless all groups.
        $query = 'SELECT DISTINCT id, name, description FROM groups';
        $connector = 'WHERE';
    }
    else {
        # Get all groups for the user where:
        #    + They have direct bless privileges
        #    + They are a member of a group that inherits bless privs.
        # Because of the second requirement, derive_groups must be up-to-date
        # for this to function properly in all circumstances.
        $query = q{
            SELECT DISTINCT groups.id, groups.name, groups.description
                       FROM groups, user_group_map, group_group_map AS ggm
                      WHERE user_group_map.user_id = ?
                        AND ((user_group_map.isbless = 1
                              AND groups.id=user_group_map.group_id)
                             OR (groups.id = ggm.grantor_id
                                 AND ggm.grant_type = ?
                                 AND user_group_map.group_id = ggm.member_id
                                 AND user_group_map.isbless = 0))};
        $connector = 'AND';
        @bindValues = ($self->id, GROUP_BLESS);
    }

    # If visibilitygroups are used, restrict the set of groups.
    if ((!$self->in_group('editusers')) && Param('usevisibilitygroups')) {
        # Users need to see a group in order to bless it.
        my $visibleGroups = join(', ', @{$self->visible_groups_direct()})
            || return $self->{'bless_groups'} = [];
        $query .= " $connector id in ($visibleGroups)";
    }

    $query .= ' ORDER BY name';

    return $self->{'bless_groups'} =
        $dbh->selectall_arrayref($query, {'Slice' => {}}, @bindValues);
}

sub in_group {
    my ($self, $group) = @_;

    # If we already have the info, just return it.
    return defined($self->{groups}->{$group}) if defined $self->{groups};
    return 0 unless $self->id;

    # Otherwise, go check for it

    my $dbh = Bugzilla->dbh;

    my ($res) = $dbh->selectrow_array(q{SELECT 1
                                  FROM groups, user_group_map
                                 WHERE groups.id=user_group_map.group_id
                                   AND user_group_map.user_id=?
                                   AND isbless=0
                                   AND groups.name=?},
                              undef,
                              $self->id,
                              $group);

    return defined($res);
}

sub can_see_bug {
    my ($self, $bugid) = @_;
    my $dbh = Bugzilla->dbh;
    my $sth  = $self->{sthCanSeeBug};
    my $userid  = $self->{id};
    # Get fields from bug, presence of user on cclist, and determine if
    # the user is missing any groups required by the bug. The prepared query
    # is cached because this may be called for every row in buglists or
    # every bug in a dependency list.
    unless ($sth) {
        $sth = $dbh->prepare("SELECT 1, reporter, assigned_to, qa_contact,
                             reporter_accessible, cclist_accessible,
                             COUNT(cc.who), COUNT(bug_group_map.bug_id)
                             FROM bugs
                             LEFT JOIN cc 
                               ON cc.bug_id = bugs.bug_id
                               AND cc.who = $userid
                             LEFT JOIN bug_group_map 
                               ON bugs.bug_id = bug_group_map.bug_id
                               AND bug_group_map.group_ID NOT IN(" .
                               join(',',(-1, values(%{$self->groups}))) .
                               ") WHERE bugs.bug_id = ? 
                               AND creation_ts IS NOT NULL " .
                             $dbh->sql_group_by('bugs.bug_id', 'reporter, ' .
                             'assigned_to, qa_contact, reporter_accessible, ' .
                             'cclist_accessible'));
    }
    $sth->execute($bugid);
    my ($ready, $reporter, $owner, $qacontact, $reporter_access, $cclist_access,
        $isoncclist, $missinggroup) = $sth->fetchrow_array();
    $sth->finish;
    $self->{sthCanSeeBug} = $sth;
    return ($ready
            && ((($reporter == $userid) && $reporter_access)
                || (Param('useqacontact') && $qacontact && ($qacontact == $userid))
                || ($owner == $userid)
                || ($isoncclist && $cclist_access)
                || (!$missinggroup)));
}

sub get_selectable_products {
    my ($self, $by_id) = @_;

    if (defined $self->{SelectableProducts}) {
        my %list = @{$self->{SelectableProducts}};
        return \%list if $by_id;
        return values(%list);
    }

    my $query = "SELECT id, name " .
                "FROM products " .
                "LEFT JOIN group_control_map " .
                "ON group_control_map.product_id = products.id ";
    if (Param('useentrygroupdefault')) {
        $query .= "AND group_control_map.entry != 0 ";
    } else {
        $query .= "AND group_control_map.membercontrol = " .
                  CONTROLMAPMANDATORY . " ";
    }
    $query .= "AND group_id NOT IN(" . 
               join(',', (-1,values(%{Bugzilla->user->groups}))) . ") " .
              "WHERE group_id IS NULL ORDER BY name";
    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare($query);
    $sth->execute();
    my @products = ();
    while (my @row = $sth->fetchrow_array) {
        push(@products, @row);
    }
    $self->{SelectableProducts} = \@products;
    my %list = @products;
    return \%list if $by_id;
    return values(%list);
}

# visible_groups_inherited returns a reference to a list of all the groups
# whose members are visible to this user.
sub visible_groups_inherited {
    my $self = shift;
    return $self->{visible_groups_inherited} if defined $self->{visible_groups_inherited};
    return [] unless $self->id;
    my @visgroups = @{$self->visible_groups_direct};
    @visgroups = @{$self->flatten_group_membership(@visgroups)};
    $self->{visible_groups_inherited} = \@visgroups;
    return $self->{visible_groups_inherited};
}

# visible_groups_direct returns a reference to a list of all the groups that
# are visible to this user.
sub visible_groups_direct {
    my $self = shift;
    my @visgroups = ();
    return $self->{visible_groups_direct} if defined $self->{visible_groups_direct};
    return [] unless $self->id;

    my $dbh = Bugzilla->dbh;
    my $glist = join(',',(-1,values(%{$self->groups})));
    my $sth = $dbh->prepare("SELECT DISTINCT grantor_id
                                FROM group_group_map
                               WHERE member_id IN($glist)
                                 AND grant_type=" . GROUP_VISIBLE);
    $sth->execute();

    while (my ($row) = $sth->fetchrow_array) {
        push @visgroups,$row;
    }
    $self->{visible_groups_direct} = \@visgroups;

    return $self->{visible_groups_direct};
}

sub derive_groups {
    my ($self, $already_locked) = @_;

    my $id = $self->id;
    return unless $id;

    my $dbh = Bugzilla->dbh;

    my $sth;

    $dbh->bz_lock_tables('profiles WRITE', 'user_group_map WRITE',
                         'group_group_map READ',
                         'groups READ') unless $already_locked;

    # avoid races, we are only up to date as of the BEGINNING of this process
    my $time = $dbh->selectrow_array("SELECT NOW()");

    # first remove any old derived stuff for this user
    $dbh->do(q{DELETE FROM user_group_map
                      WHERE user_id = ?
                        AND grant_type != ?},
             undef,
             $id,
             GRANT_DIRECT);

    my %groupidsadded = ();
    # add derived records for any matching regexps

    $sth = $dbh->prepare("SELECT id, userregexp FROM groups WHERE userregexp != ''");
    $sth->execute;

    my $group_insert;
    while (my $row = $sth->fetch) {
        if ($self->{login} =~ m/$row->[1]/i) {
            $group_insert ||= $dbh->prepare(q{INSERT INTO user_group_map
                                              (user_id, group_id, isbless, grant_type)
                                              VALUES (?, ?, 0, ?)});
            $groupidsadded{$row->[0]} = 1;
            $group_insert->execute($id, $row->[0], GRANT_REGEXP);
        }
    }

    # Get a list of the groups of which the user is a member.
    my %groupidschecked = ();

    my @groupidstocheck = @{$dbh->selectcol_arrayref(q{SELECT group_id
                                                         FROM user_group_map
                                                        WHERE user_id=?},
                                                     undef,
                                                     $id)};

    # Each group needs to be checked for inherited memberships once.
    my $group_sth;
    while (@groupidstocheck) {
        my $group = shift @groupidstocheck;
        if (!defined($groupidschecked{"$group"})) {
            $groupidschecked{"$group"} = 1;
            $group_sth ||= $dbh->prepare(q{SELECT grantor_id
                                             FROM group_group_map
                                            WHERE member_id=?
                                              AND grant_type = } .
                                                  GROUP_MEMBERSHIP);
            $group_sth->execute($group);
            while (my ($groupid) = $group_sth->fetchrow_array) {
                if (!defined($groupidschecked{"$groupid"})) {
                    push(@groupidstocheck,$groupid);
                }
                if (!$groupidsadded{$groupid}) {
                    $groupidsadded{$groupid} = 1;
                    $group_insert ||= $dbh->prepare(q{INSERT INTO user_group_map
                                                      (user_id, group_id, isbless, grant_type)
                                                      VALUES (?, ?, 0, ?)});
                    $group_insert->execute($id, $groupid, GRANT_DERIVED);
                }
            }
        }
    }

    $dbh->do(q{UPDATE profiles
                  SET refreshed_when = ?
                WHERE userid=?},
             undef,
             $time,
             $id);
    $dbh->bz_unlock_tables() unless $already_locked;
}

sub product_responsibilities {
    my $self = shift;

    return $self->{'product_resp'} if defined $self->{'product_resp'};
    return [] unless $self->id;

    my $h = Bugzilla->dbh->selectall_arrayref(
        qq{SELECT products.name AS productname,
                  components.name AS componentname,
                  initialowner,
                  initialqacontact
           FROM products, components
           WHERE products.id = components.product_id
             AND ? IN (initialowner, initialqacontact)
          },
        {'Slice' => {}}, $self->id);
    $self->{'product_resp'} = $h;

    return $h;
}

sub can_bless {
    my $self = shift;

    if (!scalar(@_)) {
        # If we're called without an argument, just return 
        # whether or not we can bless at all.
        return scalar(@{$self->bless_groups}) ? 1 : 0;
    }

    # Otherwise, we're checking a specific group
    my $group_name = shift;
    return (grep {$$_{'name'} eq $group_name} (@{$self->bless_groups})) ? 1 : 0;
}

sub flatten_group_membership {
    my ($self, @groups) = @_;

    my $dbh = Bugzilla->dbh;
    my $sth;
    my @groupidstocheck = @groups;
    my %groupidschecked = ();
    $sth = $dbh->prepare("SELECT member_id FROM group_group_map
                             WHERE grantor_id = ? 
                               AND grant_type = " . GROUP_MEMBERSHIP);
    while (my $node = shift @groupidstocheck) {
        $sth->execute($node);
        my $member;
        while (($member) = $sth->fetchrow_array) {
            if (!$groupidschecked{$member}) {
                $groupidschecked{$member} = 1;
                push @groupidstocheck, $member;
                push @groups, $member unless grep $_ == $member, @groups;
            }
        }
    }
    return \@groups;
}

sub match {
    # Generates a list of users whose login name (email address) or real name
    # matches a substring or wildcard.
    # This is also called if matches are disabled (for error checking), but
    # in this case only the exact match code will end up running.

    # $str contains the string to match, while $limit contains the
    # maximum number of records to retrieve.
    my ($str, $limit, $exclude_disabled) = @_;
    
    my @users = ();

    return \@users if $str =~ /^\s*$/;

    # The search order is wildcards, then exact match, then substring search.
    # Wildcard matching is skipped if there is no '*', and exact matches will
    # not (?) have a '*' in them.  If any search comes up with something, the
    # ones following it will not execute.

    # first try wildcards

    my $wildstr = $str;
    my $user = Bugzilla->user;
    my $dbh = Bugzilla->dbh;

    if ($wildstr =~ s/\*/\%/g && # don't do wildcards if no '*' in the string
        Param('usermatchmode') ne 'off') { # or if we only want exact matches

        # Build the query.
        my $sqlstr = &::SqlQuote($wildstr);
        my $query  = "SELECT DISTINCT userid, realname, login_name, " .
                     "LENGTH(login_name) AS namelength " .
                     "FROM profiles ";
        if (&::Param('usevisibilitygroups')) {
            $query .= ", user_group_map ";
        }
        $query .= "WHERE ("  
            . $dbh->sql_istrcmp('login_name', $sqlstr, "LIKE") . " OR " .
              $dbh->sql_istrcmp('realname', $sqlstr, "LIKE") . ") ";
        if (&::Param('usevisibilitygroups')) {
            $query .= "AND user_group_map.user_id = userid " .
                      "AND isbless = 0 " .
                      "AND group_id IN(" .
                      join(', ', (-1, @{$user->visible_groups_inherited})) . ") " .
                      "AND grant_type <> " . GRANT_DERIVED;
        }
        $query    .= " AND disabledtext = '' " if $exclude_disabled;
        $query    .= "ORDER BY namelength ";
        $query    .= $dbh->sql_limit($limit) if $limit;

        # Execute the query, retrieve the results, and make them into
        # User objects.

        &::PushGlobalSQLState();
        &::SendSQL($query);
        push(@users, new Bugzilla::User(&::FetchSQLData())) while &::MoreSQLData();
        &::PopGlobalSQLState();

    }
    else {    # try an exact match

        my $sqlstr = &::SqlQuote($str);
        my $query  = "SELECT userid, realname, login_name " .
                     "FROM profiles " .
                     "WHERE " . $dbh->sql_istrcmp('login_name', $sqlstr);
        # Exact matches don't care if a user is disabled.

        &::PushGlobalSQLState();
        &::SendSQL($query);
        push(@users, new Bugzilla::User(&::FetchSQLData())) if &::MoreSQLData();
        &::PopGlobalSQLState();
    }

    # then try substring search

    if ((scalar(@users) == 0)
        && (&::Param('usermatchmode') eq 'search')
        && (length($str) >= 3))
    {

        my $sqlstr = &::SqlQuote(lc($str));

        my $query   = "SELECT DISTINCT userid, realname, login_name, " .
                      "LENGTH(login_name) AS namelength " .
                      "FROM  profiles";
        if (&::Param('usevisibilitygroups')) {
            $query .= ", user_group_map";
        }
        $query     .= " WHERE (" .
                $dbh->sql_position($sqlstr, 'LOWER(login_name)') . " > 0" .
                      " OR " .
                $dbh->sql_position($sqlstr, 'LOWER(realname)') . " > 0)";
        if (&::Param('usevisibilitygroups')) {
            $query .= " AND user_group_map.user_id = userid" .
                      " AND isbless = 0" .
                      " AND group_id IN(" .
                join(', ', (-1, @{$user->visible_groups_inherited})) . ")" .
                      " AND grant_type <> " . GRANT_DERIVED;
        }
        $query     .= " AND disabledtext = ''" if $exclude_disabled;
        $query     .= " ORDER BY namelength";
        $query     .= " " . $dbh->sql_limit($limit) if $limit;
        &::PushGlobalSQLState();
        &::SendSQL($query);
        push(@users, new Bugzilla::User(&::FetchSQLData())) while &::MoreSQLData();
        &::PopGlobalSQLState();
    }

    # order @users by alpha

    @users = sort { uc($a->login) cmp uc($b->login) } @users;

    return \@users;
}

# match_field() is a CGI wrapper for the match() function.
#
# Here's what it does:
#
# 1. Accepts a list of fields along with whether they may take multiple values
# 2. Takes the values of those fields from the first parameter, a $cgi object 
#    and passes them to match()
# 3. Checks the results of the match and displays confirmation or failure
#    messages as appropriate.
#
# The confirmation screen functions the same way as verify-new-product and
# confirm-duplicate, by rolling all of the state information into a
# form which is passed back, but in this case the searched fields are
# replaced with the search results.
#
# The act of displaying the confirmation or failure messages means it must
# throw a template and terminate.  When confirmation is sent, all of the
# searchable fields have been replaced by exact fields and the calling script
# is executed as normal.
#
# You also have the choice of *never* displaying the confirmation screen.
# In this case, match_field will return one of the three USER_MATCH 
# constants described in the POD docs. To make match_field behave this
# way, pass in MATCH_SKIP_CONFIRM as the third argument.
#
# match_field must be called early in a script, before anything external is
# done with the form data.
#
# In order to do a simple match without dealing with templates, confirmation,
# or globals, simply calling Bugzilla::User::match instead will be
# sufficient.

# How to call it:
#
# Bugzilla::User::match_field($cgi, {
#   'field_name'    => { 'type' => fieldtype },
#   'field_name2'   => { 'type' => fieldtype },
#   [...]
# });
#
# fieldtype can be either 'single' or 'multi'.
#

sub match_field {
    my $cgi          = shift;   # CGI object to look up fields in
    my $fields       = shift;   # arguments as a hash
    my $behavior     = shift || 0; # A constant that tells us how to act
    my $matches      = {};      # the values sent to the template
    my $matchsuccess = 1;       # did the match fail?
    my $need_confirm = 0;       # whether to display confirmation screen
    my $match_multiple = 0;     # whether we ever matched more than one user

    # prepare default form values

    my $vars = $::vars;

    # What does a "--do_not_change--" field look like (if any)?
    my $dontchange = $cgi->param('dontchange');

    # Fields can be regular expressions matching multiple form fields
    # (f.e. "requestee-(\d+)"), so expand each non-literal field
    # into the list of form fields it matches.
    my $expanded_fields = {};
    foreach my $field_pattern (keys %{$fields}) {
        # Check if the field has any non-word characters.  Only those fields
        # can be regular expressions, so don't expand the field if it doesn't
        # have any of those characters.
        if ($field_pattern =~ /^\w+$/) {
            $expanded_fields->{$field_pattern} = $fields->{$field_pattern};
        }
        else {
            my @field_names = grep(/$field_pattern/, $cgi->param());
            foreach my $field_name (@field_names) {
                $expanded_fields->{$field_name} = 
                  { type => $fields->{$field_pattern}->{'type'} };
                
                # The field is a requestee field; in order for its name 
                # to show up correctly on the confirmation page, we need 
                # to find out the name of its flag type.
                if ($field_name =~ /^requestee-(\d+)$/) {
                    my $flag = Bugzilla::Flag::get($1);
                    $expanded_fields->{$field_name}->{'flag_type'} = 
                      $flag->{'type'};
                }
                elsif ($field_name =~ /^requestee_type-(\d+)$/) {
                    $expanded_fields->{$field_name}->{'flag_type'} = 
                      Bugzilla::FlagType::get($1);
                }
            }
        }
    }
    $fields = $expanded_fields;

    for my $field (keys %{$fields}) {

        # Tolerate fields that do not exist.
        #
        # This is so that fields like qa_contact can be specified in the code
        # and it won't break if the CGI object does not know about them.
        #
        # It has the side-effect that if a bad field name is passed it will be
        # quietly ignored rather than raising a code error.

        next if !defined $cgi->param($field);

        # Skip it if this is a --do_not_change-- field
        next if $dontchange && $dontchange eq $cgi->param($field);

        # We need to move the query to $raw_field, where it will be split up,
        # modified by the search, and put back into the CGI environment
        # incrementally.

        my $raw_field = join(" ", $cgi->param($field));

        # When we add back in values later, it matters that we delete
        # the field here, and not set it to '', so that we will add
        # things to an empty list, and not to a list containing one
        # empty string.
        # If the field accepts only one match (type eq "single") and
        # no match or more than one match is found for this field,
        # we will set it back to '' so that the field remains defined
        # outside this function (it was if we came here; else we would
        # have returned earlier above).
        # If the field accepts several matches (type eq "multi") and no match
        # is found, we leave this field undefined (= empty array).
        $cgi->delete($field);

        my @queries = ();

        # Now we either split $raw_field by spaces/commas and put the list
        # into @queries, or in the case of fields which only accept single
        # entries, we simply use the verbatim text.

        $raw_field =~ s/^\s+|\s+$//sg;  # trim leading/trailing space

        # single field
        if ($fields->{$field}->{'type'} eq 'single') {
            @queries = ($raw_field) unless $raw_field =~ /^\s*$/;

        # multi-field
        }
        elsif ($fields->{$field}->{'type'} eq 'multi') {
            @queries =  split(/[\s,]+/, $raw_field);

        }
        else {
            # bad argument
            ThrowCodeError('bad_arg',
                           { argument => $fields->{$field}->{'type'},
                             function =>  'Bugzilla::User::match_field',
                           });
        }

        my $limit = 0;
        if (&::Param('maxusermatches')) {
            $limit = &::Param('maxusermatches') + 1;
        }

        for my $query (@queries) {

            my $users = match(
                $query,   # match string
                $limit,   # match limit
                1         # exclude_disabled
            );

            # skip confirmation for exact matches
            if ((scalar(@{$users}) == 1)
                && (@{$users}[0]->{'login'} eq $query))
            {
                $cgi->append(-name=>$field,
                             -values=>[@{$users}[0]->{'login'}]);

                next;
            }

            $matches->{$field}->{$query}->{'users'}  = $users;
            $matches->{$field}->{$query}->{'status'} = 'success';

            # here is where it checks for multiple matches

            if (scalar(@{$users}) == 1) { # exactly one match

                $cgi->append(-name=>$field,
                             -values=>[@{$users}[0]->{'login'}]);

                $need_confirm = 1 if &::Param('confirmuniqueusermatch');

            }
            elsif ((scalar(@{$users}) > 1)
                    && (&::Param('maxusermatches') != 1)) {
                $need_confirm = 1;
                $match_multiple = 1;

                if ((&::Param('maxusermatches'))
                   && (scalar(@{$users}) > &::Param('maxusermatches')))
                {
                    $matches->{$field}->{$query}->{'status'} = 'trunc';
                    pop @{$users};  # take the last one out
                }

            }
            else {
                # everything else fails
                $matchsuccess = 0; # fail
                $matches->{$field}->{$query}->{'status'} = 'fail';
                $need_confirm = 1;  # confirmation screen shows failures
            }
        }
        # Above, we deleted the field before adding matches. If no match
        # or more than one match has been found for a field expecting only
        # one match (type eq "single"), we set it back to '' so
        # that the caller of this function can still check whether this
        # field was defined or not (and it was if we came here).
        if (!defined $cgi->param($field)
            && $fields->{$field}->{'type'} eq 'single') {
            $cgi->param($field, '');
        }
    }

    my $retval;
    if (!$matchsuccess) {
        $retval = USER_MATCH_FAILED;
    }
    elsif ($match_multiple) {
        $retval = USER_MATCH_MULTIPLE;
    }
    else {
        $retval = USER_MATCH_SUCCESS;
    }

    # Skip confirmation if we were told to, or if we don't need to confirm.
    return $retval if ($behavior == MATCH_SKIP_CONFIRM || !$need_confirm);

    $vars->{'script'}        = $ENV{'SCRIPT_NAME'}; # for self-referencing URLs
    $vars->{'fields'}        = $fields; # fields being matched
    $vars->{'matches'}       = $matches; # matches that were made
    $vars->{'matchsuccess'}  = $matchsuccess; # continue or fail

    print Bugzilla->cgi->header();

    $::template->process("global/confirm-user-match.html.tmpl", $vars)
      || ThrowTemplateError($::template->error());

    exit;

}

# Changes in some fields automatically trigger events. The 'field names' are
# from the fielddefs table. We really should be using proper field names 
# throughout.
our %names_to_events = (
    'Resolution'             => EVT_OPENED_CLOSED,
    'Keywords'               => EVT_KEYWORD,
    'CC'                     => EVT_CC,
    'Severity'               => EVT_PROJ_MANAGEMENT,
    'Priority'               => EVT_PROJ_MANAGEMENT,
    'Status'                 => EVT_PROJ_MANAGEMENT,
    'Target Milestone'       => EVT_PROJ_MANAGEMENT,
    'Attachment description' => EVT_ATTACHMENT_DATA,
    'Attachment mime type'   => EVT_ATTACHMENT_DATA,
    'Attachment is patch'    => EVT_ATTACHMENT_DATA);

# Returns true if the user wants mail for a given bug change.
# Note: the "+" signs before the constants suppress bareword quoting.
sub wants_bug_mail {
    my $self = shift;
    my ($bug_id, $relationship, $fieldDiffs, $commentField, $changer) = @_;

    # Don't send any mail, ever, if account is disabled 
    # XXX Temporary Compatibility Change 1 of 2:
    # This code is disabled for the moment to make the behaviour like the old
    # system, which sent bugmail to disabled accounts.
    # return 0 if $self->{'disabledtext'};
    
    # Make a list of the events which have happened during this bug change,
    # from the point of view of this user.    
    my %events;    
    foreach my $ref (@$fieldDiffs) {
        my ($who, $fieldName, $when, $old, $new) = @$ref;
        # A change to any of the above fields sets the corresponding event
        if (defined($names_to_events{$fieldName})) {
            $events{$names_to_events{$fieldName}} = 1;
        }
        else {
            # Catch-all for any change not caught by a more specific event
            # XXX: Temporary Compatibility Change 2 of 2:
            # This code is disabled, and replaced with the code a few lines
            # below, in order to make the behaviour more like the original, 
            # which only added this event if _all_ changes were of "other" type.
            # $events{+EVT_OTHER} = 1;            
        }

        # If the user is in a particular role and the value of that role
        # changed, we need the ADDED_REMOVED event.
        if (($fieldName eq "AssignedTo" && $relationship == REL_ASSIGNEE) ||
            ($fieldName eq "QAContact" && $relationship == REL_QA)) 
        {
            $events{+EVT_ADDED_REMOVED} = 1;
        }
        
        if ($fieldName eq "CC") {
            my $login = $self->login;
            my $inold = ($old =~ /^(.*,)?\Q$login\E(,.*)?$/);
            my $innew = ($new =~ /^(.*,)?\Q$login\E(,.*)?$/);
            if ($inold != $innew)
            {
                $events{+EVT_ADDED_REMOVED} = 1;
            }
        }
    }

    if ($commentField =~ /Created an attachment \(/) {
        $events{+EVT_ATTACHMENT} = 1;
    }
    elsif ($commentField ne '') {
        $events{+EVT_COMMENT} = 1;
    }
    
    my @event_list = keys %events;
    
    # XXX Temporary Compatibility Change 2 of 2:
    # See above comment.
    if (!scalar(@event_list)) {
      @event_list = (EVT_OTHER);
    }
    
    my $wants_mail = $self->wants_mail(\@event_list, $relationship);

    # The negative events are handled separately - they can't be incorporated
    # into the first wants_mail call, because they are of the opposite sense.
    # 
    # We do them separately because if _any_ of them are set, we don't want
    # the mail.
    if ($wants_mail && $changer && ($self->{'login'} eq $changer)) {
        $wants_mail &= $self->wants_mail([EVT_CHANGED_BY_ME], $relationship);
    }    
    
    if ($wants_mail) {
        my $dbh = Bugzilla->dbh;
        # We don't create a Bug object from the bug_id here because we only
        # need one piece of information, and doing so (as of 2004-11-23) slows
        # down bugmail sending by a factor of 2. If Bug creation was more
        # lazy, this might not be so bad.
        my $bug_status = $dbh->selectrow_array("SELECT bug_status 
                                                FROM bugs 
                                                WHERE bug_id = $bug_id"); 
         
        if ($bug_status eq "UNCONFIRMED") {
            $wants_mail &= $self->wants_mail([EVT_UNCONFIRMED], $relationship);
        }
    }
    
    return $wants_mail;
}

# Returns true if the user wants mail for a given set of events.
sub wants_mail {
    my $self = shift;
    my ($events, $relationship) = @_;
    
    # Don't send any mail, ever, if account is disabled 
    # XXX Temporary Compatibility Change 1 of 2:
    # This code is disabled for the moment to make the behaviour like the old
    # system, which sent bugmail to disabled accounts.
    # return 0 if $self->{'disabledtext'};
    
    # No mail if there are no events
    return 0 if !scalar(@$events);
    
    my $dbh = Bugzilla->dbh;
    
    # If a relationship isn't given, default to REL_ANY.
    if (!defined($relationship)) {
        $relationship = REL_ANY;
    }
    
    my $wants_mail = 
        $dbh->selectrow_array("SELECT 1 
                              FROM email_setting
                              WHERE user_id = $self->{'id'}
                              AND relationship = $relationship 
                              AND event IN (" . join(",", @$events) . ") 
                              LIMIT 1");
                              
    return defined($wants_mail) ? 1 : 0;
}

sub is_mover {
    my $self = shift;

    if (!defined $self->{'is_mover'}) {
        my @movers = map { trim($_) } split(',', Param('movers'));
        $self->{'is_mover'} = ($self->id
                               && lsearch(\@movers, $self->login) != -1);
    }
    return $self->{'is_mover'};
}

sub get_userlist {
    my $self = shift;

    return $self->{'userlist'} if defined $self->{'userlist'};

    my $dbh = Bugzilla->dbh;
    my $query  = "SELECT DISTINCT login_name, realname,";
    if (&::Param('usevisibilitygroups')) {
        $query .= " COUNT(group_id) ";
    } else {
        $query .= " 1 ";
    }
    $query     .= "FROM profiles ";
    if (&::Param('usevisibilitygroups')) {
        $query .= "LEFT JOIN user_group_map " .
                  "ON user_group_map.user_id = userid AND isbless = 0 " .
                  "AND group_id IN(" .
                  join(', ', (-1, @{$self->visible_groups_inherited})) . ") " .
                  "AND grant_type <> " . GRANT_DERIVED;
    }
    $query    .= " WHERE disabledtext = '' ";
    $query    .= $dbh->sql_group_by('userid', 'login_name, realname');

    my $sth = $dbh->prepare($query);
    $sth->execute;

    my @userlist;
    while (my($login, $name, $visible) = $sth->fetchrow_array) {
        push @userlist, {
            login => $login,
            identity => $name ? "$name <$login>" : $login,
            visible => $visible,
        };
    }
    @userlist = sort { lc $$a{'identity'} cmp lc $$b{'identity'} } @userlist;

    $self->{'userlist'} = \@userlist;
    return $self->{'userlist'};
}

sub insert_new_user ($$;$$) {
    my ($username, $realname, $password, $disabledtext) = (@_);
    my $dbh = Bugzilla->dbh;

    $disabledtext ||= '';

    # If not specified, generate a new random password for the user.
    $password ||= &::GenerateRandomPassword();
    my $cryptpassword = bz_crypt($password);

    # XXX - These should be moved into ValidateNewUser or CheckEmailSyntax
    #       At the least, they shouldn't be here. They're safe for now, though.
    trick_taint($username);
    trick_taint($realname);

    # Insert the new user record into the database.
    $dbh->do("INSERT INTO profiles 
                          (login_name, realname, cryptpassword, disabledtext,
                           refreshed_when) 
                   VALUES (?, ?, ?, ?, '1901-01-01 00:00:00')",
             undef, 
             ($username, $realname, $cryptpassword, $disabledtext));

    # Turn on all email for the new user
    my $userid = $dbh->bz_last_key('profiles', 'userid');

    foreach my $rel (RELATIONSHIPS) {
        foreach my $event (POS_EVENTS, NEG_EVENTS) {
            # These "exceptions" define the default email preferences.
            # 
            # We enable mail unless the change was made by the user, or it's
            # just a CC list addition and the user is not the reporter.
            next if ($event == EVT_CHANGED_BY_ME);
            next if (($event == EVT_CC) && ($rel != REL_REPORTER));

            $dbh->do("INSERT INTO email_setting " . 
                     "(user_id, relationship, event) " . 
                     "VALUES ($userid, $rel, $event)");
        }        
    }

    foreach my $event (GLOBAL_EVENTS) {
        $dbh->do("INSERT INTO email_setting " . 
                 "(user_id, relationship, event) " . 
                 "VALUES ($userid, " . REL_ANY . ", $event)");
    }
    
    # Return the password to the calling code so it can be included
    # in an email sent to the user.
    return $password;
}

sub is_available_username ($;$) {
    my ($username, $old_username) = @_;

    if(login_to_id($username) != 0) {
        return 0;
    }

    my $dbh = Bugzilla->dbh;
    # $username is safe because it is only used in SELECT placeholders.
    trick_taint($username);
    # Reject if the new login is part of an email change which is
    # still in progress
    #
    # substring/locate stuff: bug 165221; this used to use regexes, but that
    # was unsafe and required weird escaping; using substring to pull out
    # the new/old email addresses and sql_position() to find the delimiter (':')
    # is cleaner/safer
    my $sth = $dbh->prepare(
        "SELECT eventdata FROM tokens WHERE tokentype = 'emailold'
        AND SUBSTRING(eventdata, 1, (" 
        . $dbh->sql_position(q{':'}, 'eventdata') . "-  1)) = ?
        OR SUBSTRING(eventdata, (" 
        . $dbh->sql_position(q{':'}, 'eventdata') . "+ 1)) = ?");
    $sth->execute($username, $username);

    if (my ($eventdata) = $sth->fetchrow_array()) {
        # Allow thru owner of token
        if($old_username && ($eventdata eq "$old_username:$username")) {
            return 1;
        }
        return 0;
    }

    return 1;
}

sub login_to_id ($) {
    my ($login) = (@_);
    my $dbh = Bugzilla->dbh;
    # $login will only be used by the following SELECT statement, so it's safe.
    trick_taint($login);
    my $user_id = $dbh->selectrow_array("SELECT userid FROM profiles WHERE " .
                                        $dbh->sql_istrcmp('login_name', '?'),
                                        undef, $login);
    if ($user_id) {
        return $user_id;
    } else {
        return 0;
    }
}

sub UserInGroup ($) {
    return defined Bugzilla->user->groups->{$_[0]} ? 1 : 0;
}

1;

__END__

=head1 NAME

Bugzilla::User - Object for a Bugzilla user

=head1 SYNOPSIS

  use Bugzilla::User;

  my $user = new Bugzilla::User($id);

  # Class Functions
  $password = insert_new_user($username, $realname, $password, $disabledtext);

=head1 DESCRIPTION

This package handles Bugzilla users. Data obtained from here is read-only;
there is currently no way to modify a user from this package.

Note that the currently logged in user (if any) is available via
L<Bugzilla-E<gt>user|Bugzilla/"user">.

=head1 CONSTANTS

=over

=item C<USER_MATCH_MULTIPLE>

Returned by C<match_field()> when at least one field matched more than 
one user, but no matches failed.

=item C<USER_MATCH_FAILED>

Returned by C<match_field()> when at least one field failed to match 
anything.

=item C<USER_MATCH_SUCCESS>

Returned by C<match_field()> when all fields successfully matched only one
user.

=item C<MATCH_SKIP_CONFIRM>

Passed in to match_field to tell match_field to never display a 
confirmation screen.

=back

=head1 METHODS

=over 4

=item C<new($userid)>

Creates a new C{Bugzilla::User> object for the given user id.  If no user
id was given, a blank object is created with no user attributes.

If an id was given but there was no matching user found, undef is returned.

=begin undocumented

=item C<new_from_login($login)>

Creates a new C<Bugzilla::User> object given the provided login. Returns
C<undef> if no matching user is found.

This routine should not be required in general; most scripts should be using
userids instead.

This routine and C<new> both take an extra optional argument, which is
passed as the argument to C<derive_groups> to avoid locking. See that
routine's documentation for details.

=end undocumented

=item C<id>

Returns the userid for this user.

=item C<login>

Returns the login name for this user.

=item C<email>

Returns the user's email address. Currently this is the same value as the
login.

=item C<name>

Returns the 'real' name for this user, if any.

=item C<showmybugslink>

Returns C<1> if the user has set his preference to show the 'My Bugs' link in
the page footer, and C<0> otherwise.

=item C<identity>

Retruns a string for the identity of the user. This will be of the form
C<name E<lt>emailE<gt>> if the user has specified a name, and C<email>
otherwise.

=item C<nick>

Returns a user "nickname" -- i.e. a shorter, not-necessarily-unique name by
which to identify the user. Currently the part of the user's email address
before the at sign (@), but that could change, especially if we implement
usernames not dependent on email address.

=item C<queries>

Returns an array of the user's named queries, sorted in a case-insensitive
order by name. Each entry is a hash with three keys:

=over

=item *

name - The name of the query

=item *

query - The text for the query

=item *

linkinfooter - Whether or not the query should be displayed in the footer.

=back

=item C<disabledtext>

Returns the disable text of the user, if any.

=item C<settings>

Returns a hash of hashes which holds the user's settings. The first key is
the name of the setting, as found in setting.name. The second key is one of:
is_enabled     - true if the user is allowed to set the preference themselves;
                 false to force the site defaults
                 for themselves or must accept the global site default value
default_value  - the global site default for this setting
value          - the value of this setting for this user. Will be the same
                 as the default_value if the user is not logged in, or if 
                 is_default is true.
is_default     - a boolean to indicate whether the user has chosen to make
                 a preference for themself or use the site default.

=item C<flush_queries_cache>

Some code modifies the set of stored queries. Because C<Bugzilla::User> does
not handle these modifications, but does cache the result of calling C<queries>
internally, such code must call this method to flush the cached result.

=item C<groups>

Returns a hashref of group names for groups the user is a member of. The keys
are the names of the groups, whilst the values are the respective group ids.
(This is so that a set of all groupids for groups the user is in can be
obtained by C<values(%{$user-E<gt>groups})>.)

=item C<in_group>

Determines whether or not a user is in the given group. This method is mainly
intended for cases where we are not looking at the currently logged in user,
and only need to make a quick check for the group, where calling C<groups>
and getting all of the groups would be overkill.

=item C<bless_groups>

Returns an arrayref of hashes of C<groups> entries, where the keys of each hash
are the names of C<id>, C<name> and C<description> columns of the C<groups>
table.
The arrayref consists of the groups the user can bless, taking into account
that having editusers permissions means that you can bless all groups, and
that you need to be aware of a group in order to bless a group.

=item C<can_see_bug(bug_id)>

Determines if the user can see the specified bug.

=item C<derive_groups>

Bugzilla allows for group inheritance. When data about the user (or any of the
groups) changes, the database must be updated. Handling updated groups is taken
care of by the constructor. However, when updating the email address, the
user may be placed into different groups, based on a new email regexp. This
method should be called in such a case to force reresolution of these groups.

=item C<get_selectable_products(by_id)>

Returns an alphabetical list of product names from which
the user can select bugs.  If the $by_id parameter is true, it returns
a hash where the keys are the product ids and the values are the
product names.

=item C<get_userlist>

Returns a reference to an array of users.  The array is populated with hashrefs
containing the login, identity and visibility.  Users that are not visible to this
user will have 'visible' set to zero.

=item C<flatten_group_membership>

Accepts a list of groups and returns a list of all the groups whose members 
inherit membership in any group on the list.  So, we can determine if a user
is in any of the groups input to flatten_group_membership by querying the
user_group_map for any user with DIRECT or REGEXP membership IN() the list
of groups returned.

=item C<visible_groups_inherited>

Returns a list of all groups whose members should be visible to this user.
Since this list is flattened already, there is no need for all users to
be have derived groups up-to-date to select the users meeting this criteria.

=item C<visible_groups_direct>

Returns a list of groups that the user is aware of.

=begin undocumented

This routine takes an optional argument. If true, then this routine will not
lock the tables, but will rely on the caller to have done so itsself.

This is required because mysql will only execute a query if all of the tables
are locked, or if none of them are, not a mixture. If the caller has already
done some locking, then this routine would fail. Thus the caller needs to lock
all the tables required by this method, and then C<derive_groups> won't do
any locking.

This is a really ugly solution, and when Bugzilla supports transactions
instead of using the explicit table locking we were forced to do when thats
all MySQL supported, this will go away.

=end undocumented

=item C<product_responsibilities>

Retrieve user's product responsibilities as a list of hashes.
One hash per Bugzilla component the user has a responsibility for.
These are the hash keys:

=over

=item productname

Name of the product.

=item componentname

Name of the component.

=item initialowner

User ID of default assignee.

=item initialqacontact

User ID of default QA contact.

=back

=item C<can_bless>

When called with no arguments:
Returns C<1> if the user can bless at least one group, returns C<0> otherwise.

When called with one argument:
Returns C<1> if the user can bless the group with that name, returns
C<0> otherwise.

=item C<set_flags>
=item C<get_flag>

User flags are template-accessible user status information, stored in the form
of a hash.  For an example of use, when the current user is authenticated in
such a way that they are allowed to log out, the 'can_logout' flag is set to
true (1).  The template then checks this flag before displaying the "Log Out"
link.

C<set_flags> is called with any number of key,value pairs.  Flags for each key
will be set to the specified value.

C<get_flag> is called with a single key name, which returns the associated
value.

=item C<wants_bug_mail>

Returns true if the user wants mail for a given bug change.

=item C<wants_mail>

Returns true if the user wants mail for a given set of events. This method is
more general than C<wants_bug_mail>, allowing you to check e.g. permissions
for flag mail.

=item C<is_mover>

Returns true if the user is in the list of users allowed to move bugs
to another database. Note that this method doesn't check whether bug
moving is enabled.

=back

=head1 CLASS FUNCTIONS

These are functions that are not called on a User object, but instead are
called "statically," just like a normal procedural function.

=over 4

=item C<insert_new_user>

Creates a new user in the database.

Params: $username (scalar, string) - The login name for the new user.
        $realname (scalar, string) - The full name for the new user.
        $password (scalar, string) - Optional. The password for the new user;
                                     if not given, a random password will be
                                     generated.
        $disabledtext (scalar, string) - Optional. The disable text for the new
                                         user; if not given, it will be empty.
                                         If given, the user will be disabled,
                                         meaning the account will be
                                         unavailable for login.

Returns: The password for this user, in plain text, so it can be included
         in an e-mail sent to the user.

=item C<is_available_username>

Returns a boolean indicating whether or not the supplied username is
already taken in Bugzilla.

Params: $username (scalar, string) - The full login name of the username 
            that you are checking.
        $old_username (scalar, string) - If you are checking an email-change
            token, insert the "old" username that the user is changing from,
            here. Then, as long as it's the right user for that token, he 
            can change his username to $username. (That is, this function
            will return a boolean true value).

=item C<login_to_id($login)>

Takes a login name of a Bugzilla user and changes that into a numeric
ID for that user. This ID can then be passed to Bugzilla::User::new to
create a new user.

If no valid user exists with that login name, then the function will return 0.

This function can also be used when you want to just find out the userid
of a user, but you don't want the full weight of Bugzilla::User.

However, consider using a Bugzilla::User object instead of this function
if you need more information about the user than just their ID.

=item C<UserInGroup($groupname)>

Takes a name of a group, and returns 1 if a user is in the group, 0 otherwise.

=back

=head1 SEE ALSO

L<Bugzilla|Bugzilla>
