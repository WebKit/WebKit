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
# Contributor(s): Dawn Endico    <endico@mozilla.org>
#                 Terry Weissman <terry@mozilla.org>
#                 Chris Yeh      <cyeh@bluemartini.com>
#                 Bradley Baetz  <bbaetz@acm.org>
#                 Dave Miller    <justdave@bugzilla.org>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Frédéric Buclin <LpSolit@gmail.com>

package Bugzilla::Bug;

use strict;

use vars qw($legal_keywords @legal_platform
            @legal_priority @legal_severity @legal_opsys @legal_bug_status
            @settable_resolution %components %versions %target_milestone
            @enterable_products %milestoneurl %prodmaxvotes);

use CGI::Carp qw(fatalsToBrowser);

use Bugzilla;
use Bugzilla::Attachment;
use Bugzilla::BugMail;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Flag;
use Bugzilla::FlagType;
use Bugzilla::User;
use Bugzilla::Util;
use Bugzilla::Error;

use base qw(Exporter);
@Bugzilla::Bug::EXPORT = qw(
    AppendComment ValidateComment
    bug_alias_to_id ValidateBugAlias
    RemoveVotes CheckIfVotedConfirmed
);

use constant MAX_COMMENT_LENGTH => 65535;

sub fields {
    # Keep this ordering in sync with bugzilla.dtd
    my @fields = qw(bug_id alias creation_ts short_desc delta_ts
                    reporter_accessible cclist_accessible
                    classification_id classification
                    product component version rep_platform op_sys
                    bug_status resolution
                    bug_file_loc status_whiteboard keywords
                    priority bug_severity target_milestone
                    dependson blocked votes
                    reporter assigned_to cc
                   );

    if (Param('useqacontact')) {
        push @fields, "qa_contact";
    }

    if (Param('timetrackinggroup')) {
        push @fields, qw(estimated_time remaining_time actual_time deadline);
    }

    return @fields;
}

my %ok_field;
foreach my $key (qw(error groups
                    longdescs milestoneurl attachments
                    isopened isunconfirmed
                    flag_types num_attachment_flag_types
                    show_attachment_flags use_keywords any_flags_requesteeble
                   ),
                 fields()) {
    $ok_field{$key}++;
}

# create a new empty bug
#
sub new {
  my $type = shift();
  my %bug;

  # create a ref to an empty hash and bless it
  #
  my $self = {%bug};
  bless $self, $type;

  # construct from a hash containing a bug's info
  #
  if ($#_ == 1) {
    $self->initBug(@_);
  } else {
    confess("invalid number of arguments \($#_\)($_)");
  }

  # bless as a Bug
  #
  return $self;
}

# dump info about bug into hash unless user doesn't have permission
# user_id 0 is used when person is not logged in.
#
sub initBug  {
  my $self = shift();
  my ($bug_id, $user_id) = (@_);
  my $dbh = Bugzilla->dbh;

  $bug_id = trim($bug_id);

  my $old_bug_id = $bug_id;

  # If the bug ID isn't numeric, it might be an alias, so try to convert it.
  $bug_id = bug_alias_to_id($bug_id) if $bug_id !~ /^0*[1-9][0-9]*$/;

  if ((! defined $bug_id) || (!$bug_id) || (!detaint_natural($bug_id))) {
      # no bug number given or the alias didn't match a bug
      $self->{'bug_id'} = $old_bug_id;
      $self->{'error'} = "InvalidBugId";
      return $self;
  }

  # If the user is not logged in, sets $user_id to 0.
  # Else gets $user_id from the user login name if this
  # argument is not numeric.
  my $stored_user_id = $user_id;
  if (!defined $user_id) {
    $user_id = 0;
  } elsif (!detaint_natural($user_id)) {
    $user_id = login_to_id($stored_user_id); 
  }

  $self->{'who'} = new Bugzilla::User($user_id);

  my $query = "
    SELECT
      bugs.bug_id, alias, products.classification_id, classifications.name,
      bugs.product_id, products.name, version,
      rep_platform, op_sys, bug_status, resolution, priority,
      bug_severity, bugs.component_id, components.name, 
      assigned_to AS assigned_to_id, reporter AS reporter_id,
      bug_file_loc, short_desc, target_milestone,
      qa_contact AS qa_contact_id, status_whiteboard, " .
      $dbh->sql_date_format('creation_ts', '%Y.%m.%d %H:%i') . ",
      delta_ts, COALESCE(SUM(votes.vote_count), 0),
      reporter_accessible, cclist_accessible,
      estimated_time, remaining_time, " .
      $dbh->sql_date_format('deadline', '%Y-%m-%d') . "
    FROM bugs
       LEFT JOIN votes
           USING (bug_id)
      INNER JOIN components
              ON components.id = bugs.component_id
      INNER JOIN products
              ON products.id = bugs.product_id
      INNER JOIN classifications
              ON classifications.id = products.classification_id
      WHERE bugs.bug_id = ? " .
    $dbh->sql_group_by('bugs.bug_id', 'alias, products.classification_id,
      classifications.name, bugs.product_id, products.name, version,
      rep_platform, op_sys, bug_status, resolution, priority,
      bug_severity, bugs.component_id, components.name, assigned_to,
      reporter, bug_file_loc, short_desc, target_milestone,
      qa_contact, status_whiteboard, creation_ts, 
      delta_ts, reporter_accessible, cclist_accessible,
      estimated_time, remaining_time, deadline');

  my $bug_sth = $dbh->prepare($query);
  $bug_sth->execute($bug_id);
  my @row;

  if ((@row = $bug_sth->fetchrow_array()) 
      && $self->{'who'}->can_see_bug($bug_id)) {
    my $count = 0;
    my %fields;
    foreach my $field ("bug_id", "alias", "classification_id", "classification",
                       "product_id", "product", "version", 
                       "rep_platform", "op_sys", "bug_status", "resolution", 
                       "priority", "bug_severity", "component_id", "component",
                       "assigned_to_id", "reporter_id", 
                       "bug_file_loc", "short_desc",
                       "target_milestone", "qa_contact_id", "status_whiteboard",
                       "creation_ts", "delta_ts", "votes",
                       "reporter_accessible", "cclist_accessible",
                       "estimated_time", "remaining_time", "deadline")
      {
        $fields{$field} = shift @row;
        if (defined $fields{$field}) {
            $self->{$field} = $fields{$field};
        }
        $count++;
    }
  } elsif (@row) {
      $self->{'bug_id'} = $bug_id;
      $self->{'error'} = "NotPermitted";
      return $self;
  } else {
      $self->{'bug_id'} = $bug_id;
      $self->{'error'} = "NotFound";
      return $self;
  }

  $self->{'isunconfirmed'} = ($self->{bug_status} eq 'UNCONFIRMED');
  $self->{'isopened'} = &::IsOpenedState($self->{bug_status});
  
  return $self;
}

# This is the correct way to delete bugs from the DB.
# No bug should be deleted from anywhere else except from here.
#
sub remove_from_db {
    my ($self) = @_;
    my $dbh = Bugzilla->dbh;

    if ($self->{'error'}) {
        ThrowCodeError("bug_error", { bug => $self });
    }

    my $bug_id = $self->{'bug_id'};

    # tables having 'bugs.bug_id' as a foreign key:
    # - attachments
    # - bug_group_map
    # - bugs
    # - bugs_activity
    # - cc
    # - dependencies
    # - duplicates
    # - flags
    # - keywords
    # - longdescs
    # - votes

    $dbh->bz_lock_tables('attachments WRITE', 'bug_group_map WRITE',
                         'bugs WRITE', 'bugs_activity WRITE', 'cc WRITE',
                         'dependencies WRITE', 'duplicates WRITE',
                         'flags WRITE', 'keywords WRITE',
                         'longdescs WRITE', 'votes WRITE');

    $dbh->do("DELETE FROM bug_group_map WHERE bug_id = ?", undef, $bug_id);
    $dbh->do("DELETE FROM bugs_activity WHERE bug_id = ?", undef, $bug_id);
    $dbh->do("DELETE FROM cc WHERE bug_id = ?", undef, $bug_id);
    $dbh->do("DELETE FROM dependencies WHERE blocked = ? OR dependson = ?",
             undef, ($bug_id, $bug_id));
    $dbh->do("DELETE FROM duplicates WHERE dupe = ? OR dupe_of = ?",
             undef, ($bug_id, $bug_id));
    $dbh->do("DELETE FROM flags WHERE bug_id = ?", undef, $bug_id);
    $dbh->do("DELETE FROM keywords WHERE bug_id = ?", undef, $bug_id);
    $dbh->do("DELETE FROM longdescs WHERE bug_id = ?", undef, $bug_id);
    $dbh->do("DELETE FROM votes WHERE bug_id = ?", undef, $bug_id);
    # Several of the previous tables also depend on attach_id.
    $dbh->do("DELETE FROM attachments WHERE bug_id = ?", undef, $bug_id);
    $dbh->do("DELETE FROM bugs WHERE bug_id = ?", undef, $bug_id);

    $dbh->bz_unlock_tables();

    # Now this bug no longer exists
    $self->DESTROY;
    return $self;
}

#####################################################################
# Accessors
#####################################################################

# These subs are in alphabetical order, as much as possible.
# If you add a new sub, please try to keep it in alphabetical order
# with the other ones.

# Note: If you add a new method, remember that you must check the error
# state of the bug before returning any data. If $self->{error} is
# defined, then return something empty. Otherwise you risk potential
# security holes.

sub dup_id {
    my ($self) = @_;
    return $self->{'dup_id'} if exists $self->{'dup_id'};

    $self->{'dup_id'} = undef;
    return if $self->{'error'};

    if ($self->{'resolution'} eq 'DUPLICATE') { 
        my $dbh = Bugzilla->dbh;
        $self->{'dup_id'} =
          $dbh->selectrow_array(q{SELECT dupe_of 
                                  FROM duplicates
                                  WHERE dupe = ?},
                                undef,
                                $self->{'bug_id'});
    }
    return $self->{'dup_id'};
}

sub actual_time {
    my ($self) = @_;
    return $self->{'actual_time'} if exists $self->{'actual_time'};

    if ( $self->{'error'} || 
         !Bugzilla->user->in_group(Param("timetrackinggroup")) ) {
        $self->{'actual_time'} = undef;
        return $self->{'actual_time'};
    }

    my $sth = Bugzilla->dbh->prepare("SELECT SUM(work_time)
                                      FROM longdescs 
                                      WHERE longdescs.bug_id=?");
    $sth->execute($self->{bug_id});
    $self->{'actual_time'} = $sth->fetchrow_array();
    return $self->{'actual_time'};
}

sub any_flags_requesteeble () {
    my ($self) = @_;
    return $self->{'any_flags_requesteeble'} 
        if exists $self->{'any_flags_requesteeble'};
    return 0 if $self->{'error'};

    $self->{'any_flags_requesteeble'} = 
        grep($_->{'is_requesteeble'}, @{$self->flag_types});

    return $self->{'any_flags_requesteeble'};
}

sub attachments () {
    my ($self) = @_;
    return $self->{'attachments'} if exists $self->{'attachments'};
    return [] if $self->{'error'};
    $self->{'attachments'} = Bugzilla::Attachment::query($self->{bug_id});
    return $self->{'attachments'};
}

sub assigned_to () {
    my ($self) = @_;
    return $self->{'assigned_to'} if exists $self->{'assigned_to'};
    $self->{'assigned_to_id'} = 0 if $self->{'error'};
    $self->{'assigned_to'} = new Bugzilla::User($self->{'assigned_to_id'});
    return $self->{'assigned_to'};
}

sub blocked () {
    my ($self) = @_;
    return $self->{'blocked'} if exists $self->{'blocked'};
    return [] if $self->{'error'};
    $self->{'blocked'} = EmitDependList("dependson", "blocked", $self->bug_id);
    return $self->{'blocked'};
}

# Even bugs in an error state always have a bug_id.
sub bug_id { $_[0]->{'bug_id'}; }

sub cc () {
    my ($self) = @_;
    return $self->{'cc'} if exists $self->{'cc'};
    return [] if $self->{'error'};

    my $dbh = Bugzilla->dbh;
    $self->{'cc'} = $dbh->selectcol_arrayref(
        q{SELECT profiles.login_name FROM cc, profiles
           WHERE bug_id = ?
             AND cc.who = profiles.userid
        ORDER BY profiles.login_name},
      undef, $self->bug_id);

    $self->{'cc'} = undef if !scalar(@{$self->{'cc'}});

    return $self->{'cc'};
}

sub dependson () {
    my ($self) = @_;
    return $self->{'dependson'} if exists $self->{'dependson'};
    return [] if $self->{'error'};
    $self->{'dependson'} = 
        EmitDependList("blocked", "dependson", $self->bug_id);
    return $self->{'dependson'};
}

sub flag_types () {
    my ($self) = @_;
    return $self->{'flag_types'} if exists $self->{'flag_types'};
    return [] if $self->{'error'};

    # The types of flags that can be set on this bug.
    # If none, no UI for setting flags will be displayed.
    my $flag_types = Bugzilla::FlagType::match(
        {'target_type'  => 'bug',
         'product_id'   => $self->{'product_id'}, 
         'component_id' => $self->{'component_id'} });

    foreach my $flag_type (@$flag_types) {
        $flag_type->{'flags'} = Bugzilla::Flag::match(
            { 'bug_id'      => $self->bug_id,
              'type_id'     => $flag_type->{'id'},
              'target_type' => 'bug',
              'is_active'   => 1 });
    }

    $self->{'flag_types'} = $flag_types;

    return $self->{'flag_types'};
}

sub keywords () {
    my ($self) = @_;
    return $self->{'keywords'} if exists $self->{'keywords'};
    return () if $self->{'error'};

    my $dbh = Bugzilla->dbh;
    my $list_ref = $dbh->selectcol_arrayref(
         "SELECT keyworddefs.name
            FROM keyworddefs, keywords
           WHERE keywords.bug_id = ?
             AND keyworddefs.id = keywords.keywordid
        ORDER BY keyworddefs.name",
        undef, ($self->bug_id));

    $self->{'keywords'} = join(', ', @$list_ref);
    return $self->{'keywords'};
}

sub longdescs {
    my ($self) = @_;
    return $self->{'longdescs'} if exists $self->{'longdescs'};
    return [] if $self->{'error'};
    $self->{'longdescs'} = GetComments($self->{bug_id});
    return $self->{'longdescs'};
}

sub milestoneurl () {
    my ($self) = @_;
    return $self->{'milestoneurl'} if exists $self->{'milestoneurl'};
    return '' if $self->{'error'};
    $self->{'milestoneurl'} = $::milestoneurl{$self->{product}};
    return $self->{'milestoneurl'};
}

sub qa_contact () {
    my ($self) = @_;
    return $self->{'qa_contact'} if exists $self->{'qa_contact'};
    return undef if $self->{'error'};

    if (Param('useqacontact') && $self->{'qa_contact_id'}) {
        $self->{'qa_contact'} = new Bugzilla::User($self->{'qa_contact_id'});
    } else {
        # XXX - This is somewhat inconsistent with the assignee/reporter 
        # methods, which will return an empty User if they get a 0. 
        # However, we're keeping it this way now, for backwards-compatibility.
        $self->{'qa_contact'} = undef;
    }
    return $self->{'qa_contact'};
}

sub reporter () {
    my ($self) = @_;
    return $self->{'reporter'} if exists $self->{'reporter'};
    $self->{'reporter_id'} = 0 if $self->{'error'};
    $self->{'reporter'} = new Bugzilla::User($self->{'reporter_id'});
    return $self->{'reporter'};
}


sub show_attachment_flags () {
    my ($self) = @_;
    return $self->{'show_attachment_flags'} 
        if exists $self->{'show_attachment_flags'};
    return 0 if $self->{'error'};

    # The number of types of flags that can be set on attachments to this bug
    # and the number of flags on those attachments.  One of these counts must be
    # greater than zero in order for the "flags" column to appear in the table
    # of attachments.
    my $num_attachment_flag_types = Bugzilla::FlagType::count(
        { 'target_type'  => 'attachment',
          'product_id'   => $self->{'product_id'},
          'component_id' => $self->{'component_id'} });
    my $num_attachment_flags = Bugzilla::Flag::count(
        { 'target_type'  => 'attachment',
          'bug_id'       => $self->bug_id,
          'is_active'    => 1 });

    $self->{'show_attachment_flags'} =
        ($num_attachment_flag_types || $num_attachment_flags);

    return $self->{'show_attachment_flags'};
}


sub use_keywords {
    return @::legal_keywords;
}

sub use_votes {
    my ($self) = @_;
    return 0 if $self->{'error'};

    return Param('usevotes')
      && $::prodmaxvotes{$self->{product}} > 0;
}

sub groups {
    my $self = shift;
    return $self->{'groups'} if exists $self->{'groups'};
    return [] if $self->{'error'};

    my $dbh = Bugzilla->dbh;
    my @groups;

    # Some of this stuff needs to go into Bugzilla::User

    # For every group, we need to know if there is ANY bug_group_map
    # record putting the current bug in that group and if there is ANY
    # user_group_map record putting the user in that group.
    # The LEFT JOINs are checking for record existence.
    #
    my $sth = $dbh->prepare(
             "SELECT DISTINCT groups.id, name, description," .
             " bug_group_map.group_id IS NOT NULL," .
             " user_group_map.group_id IS NOT NULL," .
             " isactive, membercontrol, othercontrol" .
             " FROM groups" . 
             " LEFT JOIN bug_group_map" .
             " ON bug_group_map.group_id = groups.id" .
             " AND bug_id = ?" .
             " LEFT JOIN user_group_map" .
             " ON user_group_map.group_id = groups.id" .
             " AND user_id = ?" .
             " AND isbless = 0" .
             " LEFT JOIN group_control_map" .
             " ON group_control_map.group_id = groups.id" .
             " AND group_control_map.product_id = ? " .
             " WHERE isbuggroup = 1" .
             " ORDER BY description");
    $sth->execute($self->{'bug_id'}, Bugzilla->user->id,
                  $self->{'product_id'});

    while (my ($groupid, $name, $description, $ison, $ingroup, $isactive,
            $membercontrol, $othercontrol) = $sth->fetchrow_array()) {

        $membercontrol ||= 0;

        # For product groups, we only want to use the group if either
        # (1) The bit is set and not required, or
        # (2) The group is Shown or Default for members and
        #     the user is a member of the group.
        if ($ison ||
            ($isactive && $ingroup
                       && (($membercontrol == CONTROLMAPDEFAULT)
                           || ($membercontrol == CONTROLMAPSHOWN))
            ))
        {
            my $ismandatory = $isactive
              && ($membercontrol == CONTROLMAPMANDATORY);

            push (@groups, { "bit" => $groupid,
                             "name" => $name,
                             "ison" => $ison,
                             "ingroup" => $ingroup,
                             "mandatory" => $ismandatory,
                             "description" => $description });
        }
    }

    $self->{'groups'} = \@groups;

    return $self->{'groups'};
}

sub user {
    my $self = shift;
    return $self->{'user'} if exists $self->{'user'};
    return {} if $self->{'error'};

    my $user = Bugzilla->user;
    my $canmove = Param('move-enabled') && $user->is_mover;

    # In the below, if the person hasn't logged in, then we treat them
    # as if they can do anything.  That's because we don't know why they
    # haven't logged in; it may just be because they don't use cookies.
    # Display everything as if they have all the permissions in the
    # world; their permissions will get checked when they log in and
    # actually try to make the change.
    my $unknown_privileges = !$user->id
                             || $user->in_group("editbugs");
    my $canedit = $unknown_privileges
                  || $user->id == $self->{assigned_to_id}
                  || (Param('useqacontact')
                      && $self->{'qa_contact_id'}
                      && $user->id == $self->{qa_contact_id});
    my $canconfirm = $unknown_privileges
                     || $user->in_group("canconfirm");
    my $isreporter = $user->id
                     && $user->id == $self->{reporter_id};

    $self->{'user'} = {canmove    => $canmove,
                       canconfirm => $canconfirm,
                       canedit    => $canedit,
                       isreporter => $isreporter};
    return $self->{'user'};
}

sub choices {
    my $self = shift;
    return $self->{'choices'} if exists $self->{'choices'};
    return {} if $self->{'error'};

    &::GetVersionTable();

    $self->{'choices'} = {};

    # Fiddle the product list.
    my $seen_curr_prod;
    my @prodlist;

    foreach my $product (@::enterable_products) {
        if ($product eq $self->{'product'}) {
            # if it's the product the bug is already in, it's ALWAYS in
            # the popup, period, whether the user can see it or not, and
            # regardless of the disallownew setting.
            $seen_curr_prod = 1;
            push(@prodlist, $product);
            next;
        }

        if (!&::CanEnterProduct($product)) {
            # If we're using bug groups to restrict entry on products, and
            # this product has an entry group, and the user is not in that
            # group, we don't want to include that product in this list.
            next;
        }

        push(@prodlist, $product);
    }

    # The current product is part of the popup, even if new bugs are no longer
    # allowed for that product
    if (!$seen_curr_prod) {
        push (@prodlist, $self->{'product'});
        @prodlist = sort @prodlist;
    }

    # Hack - this array contains "". See bug 106589.
    my @res = grep ($_, @::settable_resolution);

    $self->{'choices'} =
      {
       'product' => \@prodlist,
       'rep_platform' => \@::legal_platform,
       'priority' => \@::legal_priority,
       'bug_severity' => \@::legal_severity,
       'op_sys' => \@::legal_opsys,
       'bug_status' => \@::legal_bug_status,
       'resolution' => \@res,
       'component' => $::components{$self->{product}},
       'version' => $::versions{$self->{product}},
       'target_milestone' => $::target_milestone{$self->{product}},
      };

    return $self->{'choices'};
}

# Convenience Function. If you need speed, use this. If you need
# other Bug fields in addition to this, just create a new Bug with
# the alias.
# Queries the database for the bug with a given alias, and returns
# the ID of the bug if it exists or the undefined value if it doesn't.
sub bug_alias_to_id ($) {
    my ($alias) = @_;
    return undef unless Param("usebugaliases");
    my $dbh = Bugzilla->dbh;
    trick_taint($alias);
    return $dbh->selectrow_array(
        "SELECT bug_id FROM bugs WHERE alias = ?", undef, $alias);
}

#####################################################################
# Subroutines
#####################################################################

sub AppendComment ($$$;$$$) {
    my ($bugid, $whoid, $comment, $isprivate, $timestamp, $work_time) = @_;
    $work_time ||= 0;
    my $dbh = Bugzilla->dbh;

    ValidateTime($work_time, "work_time") if $work_time;
    trick_taint($work_time);

    # Use the date/time we were given if possible (allowing calling code
    # to synchronize the comment's timestamp with those of other records).
    $timestamp =  "NOW()" unless $timestamp;

    $comment =~ s/\r\n/\n/g;     # Handle Windows-style line endings.
    $comment =~ s/\r/\n/g;       # Handle Mac-style line endings.

    if ($comment =~ /^\s*$/) {  # Nothin' but whitespace
        return;
    }

    # Comments are always safe, because we always display their raw contents,
    # and we use them in a placeholder below.
    trick_taint($comment); 
    my $privacyval = $isprivate ? 1 : 0 ;
    $dbh->do(q{INSERT INTO longdescs
                      (bug_id, who, bug_when, thetext, isprivate, work_time)
               VALUES (?,?,?,?,?,?)}, undef,
             ($bugid, $whoid, $timestamp, $comment, $privacyval, $work_time));
    $dbh->do("UPDATE bugs SET delta_ts = ? WHERE bug_id = ?",
             undef, $timestamp, $bugid);
}

# This method is private and is not to be used outside of the Bug class.
sub EmitDependList {
    my ($myfield, $targetfield, $bug_id) = (@_);
    my $dbh = Bugzilla->dbh;
    my $list_ref =
        $dbh->selectcol_arrayref(
          "SELECT dependencies.$targetfield
             FROM dependencies, bugs
            WHERE dependencies.$myfield = ?
              AND bugs.bug_id = dependencies.$targetfield
         ORDER BY dependencies.$targetfield",
         undef, ($bug_id));
    return $list_ref;
}

sub ValidateTime {
    my ($time, $field) = @_;

    # regexp verifies one or more digits, optionally followed by a period and
    # zero or more digits, OR we have a period followed by one or more digits
    # (allow negatives, though, so people can back out errors in time reporting)
    if ($time !~ /^-?(?:\d+(?:\.\d*)?|\.\d+)$/) {
        ThrowUserError("number_not_numeric",
                       {field => "$field", num => "$time"});
    }

    # Only the "work_time" field is allowed to contain a negative value.
    if ( ($time < 0) && ($field ne "work_time") ) {
        ThrowUserError("number_too_small",
                       {field => "$field", num => "$time", min_num => "0"});
    }

    if ($time > 99999.99) {
        ThrowUserError("number_too_large",
                       {field => "$field", num => "$time", max_num => "99999.99"});
    }
}

sub GetComments {
    my ($id, $comment_sort_order) = (@_);
    $comment_sort_order = $comment_sort_order ||
        Bugzilla->user->settings->{'comment_sort_order'}->{'value'};

    my $sort_order = ($comment_sort_order eq "oldest_to_newest") ? 'asc' : 'desc';
    my $dbh = Bugzilla->dbh;
    my @comments;
    my $sth = $dbh->prepare(
            "SELECT  profiles.realname AS name, profiles.login_name AS email,
            " . $dbh->sql_date_format('longdescs.bug_when', '%Y.%m.%d %H:%i') . "
               AS time, longdescs.thetext AS body, longdescs.work_time,
                     isprivate, already_wrapped,
            " . $dbh->sql_date_format('longdescs.bug_when', '%Y%m%d%H%i%s') . "
               AS bug_when
             FROM    longdescs, profiles
            WHERE    profiles.userid = longdescs.who
              AND    longdescs.bug_id = ?
            ORDER BY longdescs.bug_when $sort_order");
    $sth->execute($id);

    while (my $comment_ref = $sth->fetchrow_hashref()) {
        my %comment = %$comment_ref;

        # Can't use "when" as a field name in MySQL
        $comment{'when'} = $comment{'bug_when'};
        delete($comment{'bug_when'});

        $comment{'email'} .= Param('emailsuffix');
        $comment{'name'} = $comment{'name'} || $comment{'email'};

        push (@comments, \%comment);
    }
   
    if ($comment_sort_order eq "newest_to_oldest_desc_first") {
        unshift(@comments, pop @comments);
    }

    return \@comments;
}

# CountOpenDependencies counts the number of open dependent bugs for a
# list of bugs and returns a list of bug_id's and their dependency count
# It takes one parameter:
#  - A list of bug numbers whose dependencies are to be checked
sub CountOpenDependencies {
    my (@bug_list) = @_;
    my @dependencies;
    my $dbh = Bugzilla->dbh;

    my $sth = $dbh->prepare(
          "SELECT blocked, COUNT(bug_status) " .
            "FROM bugs, dependencies " .
           "WHERE blocked IN (" . (join "," , @bug_list) . ") " .
             "AND bug_id = dependson " .
             "AND bug_status IN ('" . (join "','", &::OpenStates())  . "') " .
          $dbh->sql_group_by('blocked'));
    $sth->execute();

    while (my ($bug_id, $dependencies) = $sth->fetchrow_array()) {
        push(@dependencies, { bug_id       => $bug_id,
                              dependencies => $dependencies });
    }

    return @dependencies;
}

sub ValidateComment ($) {
    my ($comment) = @_;

    if (defined($comment) && length($comment) > MAX_COMMENT_LENGTH) {
        ThrowUserError("comment_too_long");
    }
}

# If a bug is moved to a product which allows less votes per bug
# compared to the previous product, extra votes need to be removed.
sub RemoveVotes {
    my ($id, $who, $reason) = (@_);
    my $dbh = Bugzilla->dbh;

    my $whopart = ($who) ? " AND votes.who = $who" : "";

    my $sth = $dbh->prepare("SELECT profiles.login_name, " .
                            "profiles.userid, votes.vote_count, " .
                            "products.votesperuser, products.maxvotesperbug " .
                            "FROM profiles " . 
                            "LEFT JOIN votes ON profiles.userid = votes.who " .
                            "LEFT JOIN bugs USING(bug_id) " .
                            "LEFT JOIN products ON products.id = bugs.product_id " .
                            "WHERE votes.bug_id = ? " . $whopart);
    $sth->execute($id);
    my @list;
    while (my ($name, $userid, $oldvotes, $votesperuser, $maxvotesperbug) = $sth->fetchrow_array()) {
        push(@list, [$name, $userid, $oldvotes, $votesperuser, $maxvotesperbug]);
    }
    if (scalar(@list)) {
        foreach my $ref (@list) {
            my ($name, $userid, $oldvotes, $votesperuser, $maxvotesperbug) = (@$ref);
            my $s;

            $maxvotesperbug = min($votesperuser, $maxvotesperbug);

            # If this product allows voting and the user's votes are in
            # the acceptable range, then don't do anything.
            next if $votesperuser && $oldvotes <= $maxvotesperbug;

            # If the user has more votes on this bug than this product
            # allows, then reduce the number of votes so it fits
            my $newvotes = $maxvotesperbug;

            my $removedvotes = $oldvotes - $newvotes;

            $s = ($oldvotes == 1) ? "" : "s";
            my $oldvotestext = "You had $oldvotes vote$s on this bug.";

            $s = ($removedvotes == 1) ? "" : "s";
            my $removedvotestext = "You had $removedvotes vote$s removed from this bug.";

            my $newvotestext;
            if ($newvotes) {
                $dbh->do("UPDATE votes SET vote_count = ? " .
                         "WHERE bug_id = ? AND who = ?",
                         undef, ($newvotes, $id, $userid));
                $s = $newvotes == 1 ? "" : "s";
                $newvotestext = "You still have $newvotes vote$s on this bug."
            } else {
                $dbh->do("DELETE FROM votes WHERE bug_id = ? AND who = ?",
                         undef, ($id, $userid));
                $newvotestext = "You have no more votes remaining on this bug.";
            }

            # Notice that we did not make sure that the user fit within the $votesperuser
            # range.  This is considered to be an acceptable alternative to losing votes
            # during product moves.  Then next time the user attempts to change their votes,
            # they will be forced to fit within the $votesperuser limit.

            # Now lets send the e-mail to alert the user to the fact that their votes have
            # been reduced or removed.
            my %substs;

            $substs{"to"} = $name . Param('emailsuffix');
            $substs{"bugid"} = $id;
            $substs{"reason"} = $reason;

            $substs{"votesremoved"} = $removedvotes;
            $substs{"votesold"} = $oldvotes;
            $substs{"votesnew"} = $newvotes;

            $substs{"votesremovedtext"} = $removedvotestext;
            $substs{"votesoldtext"} = $oldvotestext;
            $substs{"votesnewtext"} = $newvotestext;

            $substs{"count"} = $removedvotes . "\n    " . $newvotestext;

            my $msg = PerformSubsts(Param("voteremovedmail"), \%substs);
            Bugzilla::BugMail::MessageToMTA($msg);
        }
        my $votes = $dbh->selectrow_array("SELECT SUM(vote_count) " .
                                          "FROM votes WHERE bug_id = ?",
                                          undef, $id) || 0;
        $dbh->do("UPDATE bugs SET votes = ? WHERE bug_id = ?",
                 undef, ($votes, $id));
    }
}

# If a user votes for a bug, or the number of votes required to
# confirm a bug has been reduced, check if the bug is now confirmed.
sub CheckIfVotedConfirmed {
    my ($id, $who) = (@_);
    my $dbh = Bugzilla->dbh;

    my ($votes, $status, $everconfirmed, $votestoconfirm, $timestamp) =
        $dbh->selectrow_array("SELECT votes, bug_status, everconfirmed, " .
                              "       votestoconfirm, NOW() " .
                              "FROM bugs INNER JOIN products " .
                              "                  ON products.id = bugs.product_id " .
                              "WHERE bugs.bug_id = ?",
                              undef, $id);

    my $ret = 0;
    if ($votes >= $votestoconfirm && !$everconfirmed) {
        if ($status eq 'UNCONFIRMED') {
            my $fieldid = &::GetFieldID("bug_status");
            $dbh->do("UPDATE bugs SET bug_status = 'NEW', everconfirmed = 1, " .
                     "delta_ts = ? WHERE bug_id = ?",
                     undef, ($timestamp, $id));
            $dbh->do("INSERT INTO bugs_activity " .
                     "(bug_id, who, bug_when, fieldid, removed, added) " .
                     "VALUES (?, ?, ?, ?, ?, ?)",
                     undef, ($id, $who, $timestamp, $fieldid, 'UNCONFIRMED', 'NEW'));
        }
        else {
            $dbh->do("UPDATE bugs SET everconfirmed = 1, delta_ts = ? " .
                     "WHERE bug_id = ?", undef, ($timestamp, $id));
        }

        my $fieldid = &::GetFieldID("everconfirmed");
        $dbh->do("INSERT INTO bugs_activity " .
                 "(bug_id, who, bug_when, fieldid, removed, added) " .
                 "VALUES (?, ?, ?, ?, ?, ?)",
                 undef, ($id, $who, $timestamp, $fieldid, '0', '1'));

        AppendComment($id, $who,
                      "*** This bug has been confirmed by popular vote. ***",
                      0, $timestamp);

        $ret = 1;
    }
    return $ret;
}

#
# Field Validation
#

# ValidateBugAlias:
#   Check that the bug alias is valid and not used by another bug.  If 
#   curr_id is specified, verify the alias is not used for any other
#   bug id.  
sub ValidateBugAlias {
    my ($alias, $curr_id) = @_;
    my $dbh = Bugzilla->dbh;

    $alias = trim($alias || "");
    trick_taint($alias);

    if ($alias eq "") {
        ThrowUserError("alias_not_defined");
    }

    # Make sure the alias isn't too long.
    if (length($alias) > 20) {
        ThrowUserError("alias_too_long");
    }

    # Make sure the alias is unique.
    my $query = "SELECT bug_id FROM bugs WHERE alias = ?";
    if (detaint_natural($curr_id)) {
        $query .= " AND bug_id != $curr_id";
    }
    my $id = $dbh->selectrow_array($query, undef, $alias); 

    my $vars = {};
    $vars->{'alias'} = $alias;
    if ($id) {
        $vars->{'bug_link'} = &::GetBugLink($id, $id);
        ThrowUserError("alias_in_use", $vars);
    }

    # Make sure the alias isn't just a number.
    if ($alias =~ /^\d+$/) {
        ThrowUserError("alias_is_numeric", $vars);
    }

    # Make sure the alias has no commas or spaces.
    if ($alias =~ /[, ]/) {
        ThrowUserError("alias_has_comma_or_space", $vars);
    }

    $_[0] = $alias;
}

# Validate and return a hash of dependencies
sub ValidateDependencies($$$) {
    my $fields = {};
    $fields->{'dependson'} = shift;
    $fields->{'blocked'} = shift;
    my $id = shift || 0;

    unless (defined($fields->{'dependson'})
            || defined($fields->{'blocked'}))
    {
        return;
    }

    my $dbh = Bugzilla->dbh;
    my %deps;
    my %deptree;
    foreach my $pair (["blocked", "dependson"], ["dependson", "blocked"]) {
        my ($me, $target) = @{$pair};
        $deptree{$target} = [];
        $deps{$target} = [];
        next unless $fields->{$target};

        my %seen;
        foreach my $i (split('[\s,]+', $fields->{$target})) {
            if ($id == $i) {
                ThrowUserError("dependency_loop_single");
            }
            if (!exists $seen{$i}) {
                push(@{$deptree{$target}}, $i);
                $seen{$i} = 1;
            }
        }
        # populate $deps{$target} as first-level deps only.
        # and find remainder of dependency tree in $deptree{$target}
        @{$deps{$target}} = @{$deptree{$target}};
        my @stack = @{$deps{$target}};
        while (@stack) {
            my $i = shift @stack;
            my $dep_list =
                $dbh->selectcol_arrayref("SELECT $target
                                          FROM dependencies
                                          WHERE $me = ?", undef, $i);
            foreach my $t (@$dep_list) {
                # ignore any _current_ dependencies involving this bug,
                # as they will be overwritten with data from the form.
                if ($t != $id && !exists $seen{$t}) {
                    push(@{$deptree{$target}}, $t);
                    push @stack, $t;
                    $seen{$t} = 1;
                }
            }
        }
    }

    my @deps   = @{$deptree{'dependson'}};
    my @blocks = @{$deptree{'blocked'}};
    my @union = ();
    my @isect = ();
    my %union = ();
    my %isect = ();
    foreach my $b (@deps, @blocks) { $union{$b}++ && $isect{$b}++ }
    @union = keys %union;
    @isect = keys %isect;
    if (scalar(@isect) > 0) {
        my $both = "";
        foreach my $i (@isect) {
           $both .= &::GetBugLink($i, "#" . $i) . " ";
        }
        ThrowUserError("dependency_loop_multi", { both => $both });
    }
    return %deps;
}

sub AUTOLOAD {
  use vars qw($AUTOLOAD);
  my $attr = $AUTOLOAD;

  $attr =~ s/.*:://;
  return unless $attr=~ /[^A-Z]/;
  confess ("invalid bug attribute $attr") unless $ok_field{$attr};

  no strict 'refs';
  *$AUTOLOAD = sub {
      my $self = shift;
      if (defined $self->{$attr}) {
          return $self->{$attr};
      } else {
          return '';
      }
  };

  goto &$AUTOLOAD;
}

1;
