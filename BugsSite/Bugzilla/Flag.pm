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
#                 Jouni Heikniemi <jouni@heikniemi.net>
#                 Frédéric Buclin <LpSolit@gmail.com>

=head1 NAME

Bugzilla::Flag - A module to deal with Bugzilla flag values.

=head1 SYNOPSIS

Flag.pm provides an interface to flags as stored in Bugzilla.
See below for more information.

=head1 NOTES

=over

=item *

Prior to calling routines in this module, it's assumed that you have
already done a C<require CGI.pl>.  This will eventually change in a
future version when CGI.pl is removed.

=item *

Import relevant functions from that script and its companion globals.pl.

=item *

Use of private functions / variables outside this module may lead to
unexpected results after an upgrade.  Please avoid usi8ng private
functions in other files/modules.  Private functions are functions
whose names start with _ or a re specifically noted as being private.

=back

=cut

######################################################################
# Module Initialization
######################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

# This module implements bug and attachment flags.
package Bugzilla::Flag;

use Bugzilla::FlagType;
use Bugzilla::User;
use Bugzilla::Config;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Attachment;
use Bugzilla::BugMail;
use Bugzilla::Constants;

# Note that this line doesn't actually import these variables for some reason,
# so I have to use them as $::template and $::vars in the package code.
use vars qw($template $vars); 

######################################################################
# Global Variables
######################################################################

# basic sets of columns and tables for getting flags from the database

=begin private

=head1 PRIVATE VARIABLES/CONSTANTS

=over

=item C<@base_columns>

basic sets of columns and tables for getting flag types from th
database.  B<Used by get, match, sqlify_criteria and perlify_record>

=cut

my @base_columns = 
  ("is_active", "id", "type_id", "bug_id", "attach_id", "requestee_id", 
   "setter_id", "status");

=pod

=item C<@base_tables>

Which database(s) is the data coming from?

Note: when adding tables to @base_tables, make sure to include the separator 
(i.e. words like "LEFT OUTER JOIN") before the table name, since tables take
multiple separators based on the join type, and therefore it is not possible
to join them later using a single known separator.
B<Used by get, match, sqlify_criteria and perlify_record>

=back

=end private

=cut

my @base_tables = ("flags");

######################################################################
# Searching/Retrieving Flags
######################################################################

=head1 PUBLIC FUNCTIONS

=over

=item C<get($id)>

Retrieves and returns a flag from the database.

=back

=cut

# !!! Implement a cache for this function!
sub get {
    my ($id) = @_;

    my $select_clause = "SELECT " . join(", ", @base_columns);
    my $from_clause = "FROM " . join(" ", @base_tables);
    
    # Execute the query, retrieve the result, and write it into a record.
    &::PushGlobalSQLState();
    &::SendSQL("$select_clause $from_clause WHERE flags.id = $id");
    my $flag = perlify_record(&::FetchSQLData());
    &::PopGlobalSQLState();

    return $flag;
}

=pod

=over

=item C<match($criteria)>

Queries the database for flags matching the given criteria
(specified as a hash of field names and their matching values)
and returns an array of matching records.

=back

=cut

sub match {
    my ($criteria) = @_;

    my $select_clause = "SELECT " . join(", ", @base_columns);
    my $from_clause = "FROM " . join(" ", @base_tables);
    
    my @criteria = sqlify_criteria($criteria);
    
    my $where_clause = "WHERE " . join(" AND ", @criteria);
    
    # Execute the query, retrieve the results, and write them into records.
    &::PushGlobalSQLState();
    &::SendSQL("$select_clause $from_clause $where_clause");
    my @flags;
    while (&::MoreSQLData()) {
        my $flag = perlify_record(&::FetchSQLData());
        push(@flags, $flag);
    }
    &::PopGlobalSQLState();

    return \@flags;
}

=pod

=over

=item C<count($criteria)>

Queries the database for flags matching the given criteria 
(specified as a hash of field names and their matching values)
and returns an array of matching records.

=back

=cut

sub count {
    my ($criteria) = @_;

    my @criteria = sqlify_criteria($criteria);
    
    my $where_clause = "WHERE " . join(" AND ", @criteria);
    
    # Execute the query, retrieve the result, and write it into a record.
    &::PushGlobalSQLState();
    &::SendSQL("SELECT COUNT(id) FROM flags $where_clause");
    my $count = &::FetchOneColumn();
    &::PopGlobalSQLState();

    return $count;
}

######################################################################
# Creating and Modifying
######################################################################

=pod

=over

=item C<validate($cgi, $bug_id, $attach_id)>

Validates fields containing flag modifications.

If the attachment is new, it has no ID yet and $attach_id is set
to -1 to force its check anyway.

=back

=cut

sub validate {
    my ($cgi, $bug_id, $attach_id) = @_;

    my $user = Bugzilla->user;
    my $dbh = Bugzilla->dbh;

    # Get a list of flags to validate.  Uses the "map" function
    # to extract flag IDs from form field names by matching fields
    # whose name looks like "flag-nnn", where "nnn" is the ID,
    # and returning just the ID portion of matching field names.
    my @ids = map(/^flag-(\d+)$/ ? $1 : (), $cgi->param());

    return unless scalar(@ids);
    
    # No flag reference should exist when changing several bugs at once.
    ThrowCodeError("flags_not_available", { type => 'b' }) unless $bug_id;

    # No reference to existing flags should exist when creating a new
    # attachment.
    if ($attach_id && ($attach_id < 0)) {
        ThrowCodeError("flags_not_available", { type => 'a' });
    }

    # Make sure all flags belong to the bug/attachment they pretend to be.
    my $field = ($attach_id) ? "attach_id" : "bug_id";
    my $field_id = $attach_id || $bug_id;
    my $not = ($attach_id) ? "" : "NOT";

    my $invalid_data =
        $dbh->selectrow_array("SELECT 1 FROM flags
                               WHERE id IN (" . join(',', @ids) . ")
                               AND ($field != ? OR attach_id IS $not NULL) " .
                               $dbh->sql_limit(1),
                               undef, $field_id);

    if ($invalid_data) {
        ThrowCodeError("invalid_flag_association",
                       { bug_id    => $bug_id,
                         attach_id => $attach_id });
    }

    foreach my $id (@ids) {
        my $status = $cgi->param("flag-$id");
        
        # Make sure the flag exists.
        my $flag = get($id);
        $flag || ThrowCodeError("flag_nonexistent", { id => $id });

        # Note that the deletedness of the flag (is_active or not) is not 
        # checked here; we do want to allow changes to deleted flags in
        # certain cases. Flag::modify() will revive the modified flags.
        # See bug 223878 for details.

        # Make sure the user chose a valid status.
        grep($status eq $_, qw(X + - ?))
          || ThrowCodeError("flag_status_invalid", 
                            { id => $id, status => $status });
                
        # Make sure the user didn't request the flag unless it's requestable.
        # If the flag was requested before it became unrequestable, leave it as is.
        if ($status eq '?' && $flag->{status} ne '?' && 
            !$flag->{type}->{is_requestable}) {
            ThrowCodeError("flag_status_invalid", 
                           { id => $id, status => $status });
        }

        # Make sure the user didn't specify a requestee unless the flag
        # is specifically requestable. If the requestee was set before
        # the flag became specifically unrequestable, leave it as is.
        my $old_requestee =
            $flag->{'requestee'} ? $flag->{'requestee'}->login : '';
        my $new_requestee = trim($cgi->param("requestee-$id") || '');

        if ($status eq '?'
            && !$flag->{type}->{is_requesteeble}
            && $new_requestee
            && ($new_requestee ne $old_requestee))
        {
            ThrowCodeError("flag_requestee_disabled",
                           { name => $flag->{type}->{name} });
        }

        # Make sure the requestee is authorized to access the bug.
        # (and attachment, if this installation is using the "insider group"
        # feature and the attachment is marked private).
        if ($status eq '?'
            && $flag->{type}->{is_requesteeble}
            && $new_requestee
            && ($old_requestee ne $new_requestee))
        {
            # We know the requestee exists because we ran
            # Bugzilla::User::match_field before getting here.
            my $requestee = Bugzilla::User->new_from_login($new_requestee);

            # Throw an error if the user can't see the bug.
            # Note that if permissions on this bug are changed,
            # can_see_bug() will refer to old settings.
            if (!$requestee->can_see_bug($bug_id)) {
                ThrowUserError("flag_requestee_unauthorized",
                               { flag_type => $flag->{'type'},
                                 requestee => $requestee,
                                 bug_id => $bug_id,
                                 attach_id =>
                                   $flag->{target}->{attachment}->{id} });
            }

            # Throw an error if the target is a private attachment and
            # the requestee isn't in the group of insiders who can see it.
            if ($flag->{target}->{attachment}->{exists}
                && $cgi->param('isprivate')
                && Param("insidergroup")
                && !$requestee->in_group(Param("insidergroup")))
            {
                ThrowUserError("flag_requestee_unauthorized_attachment",
                               { flag_type => $flag->{'type'},
                                 requestee => $requestee,
                                 bug_id    => $bug_id,
                                 attach_id =>
                                  $flag->{target}->{attachment}->{id} });
            }
        }

        # Make sure the user is authorized to modify flags, see bug 180879
        # - The flag is unchanged
        next if ($status eq $flag->{status});

        # - User in the $request_gid group can clear pending requests and set flags
        #   and can rerequest set flags.
        next if (($status eq 'X' || $status eq '?')
                 && (!$flag->{type}->{request_gid}
                     || $user->in_group(&::GroupIdToName($flag->{type}->{request_gid}))));

        # - User in the $grant_gid group can set/clear flags,
        #   including "+" and "-"
        next if (!$flag->{type}->{grant_gid}
                 || $user->in_group(&::GroupIdToName($flag->{type}->{grant_gid})));

        # - Any other flag modification is denied
        ThrowUserError("flag_update_denied",
                        { name       => $flag->{type}->{name},
                          status     => $status,
                          old_status => $flag->{status} });
    }
}

sub snapshot {
    my ($bug_id, $attach_id) = @_;

    my $flags = match({ 'bug_id'    => $bug_id,
                        'attach_id' => $attach_id,
                        'is_active' => 1 });
    my @summaries;
    foreach my $flag (@$flags) {
        my $summary = $flag->{'type'}->{'name'} . $flag->{'status'};
        $summary .= "(" . $flag->{'requestee'}->login . ")" if $flag->{'requestee'};
        push(@summaries, $summary);
    }
    return @summaries;
}


=pod

=over

=item C<process($target, $timestamp, $cgi)>

Processes changes to flags.

The target is the bug or attachment this flag is about, the timestamp
is the date/time the bug was last touched (so that changes to the flag
can be stamped with the same date/time), the cgi is the CGI object
used to obtain the flag fields that the user submitted.

=back

=cut

sub process {
    my ($target, $timestamp, $cgi) = @_;

    my $dbh = Bugzilla->dbh;
    my $bug_id = $target->{'bug'}->{'id'};
    my $attach_id = $target->{'attachment'}->{'id'};

    # Use the date/time we were given if possible (allowing calling code
    # to synchronize the comment's timestamp with those of other records).
    $timestamp = ($timestamp ? &::SqlQuote($timestamp) : "NOW()");
    
    # Take a snapshot of flags before any changes.
    my @old_summaries = snapshot($bug_id, $attach_id);
    
    # Cancel pending requests if we are obsoleting an attachment.
    if ($attach_id && $cgi->param('isobsolete')) {
        CancelRequests($bug_id, $attach_id);
    }

    # Create new flags and update existing flags.
    my $new_flags = FormToNewFlags($target, $cgi);
    foreach my $flag (@$new_flags) { create($flag, $timestamp) }
    modify($cgi, $timestamp);
    
    # In case the bug's product/component has changed, clear flags that are
    # no longer valid.
    my $flag_ids = $dbh->selectcol_arrayref(
        "SELECT flags.id 
           FROM flags
     INNER JOIN bugs
             ON flags.bug_id = bugs.bug_id
      LEFT JOIN flaginclusions AS i
             ON flags.type_id = i.type_id 
            AND (bugs.product_id = i.product_id OR i.product_id IS NULL)
            AND (bugs.component_id = i.component_id OR i.component_id IS NULL)
          WHERE bugs.bug_id = ?
            AND flags.is_active = 1
            AND i.type_id IS NULL",
        undef, $bug_id);

    foreach my $flag_id (@$flag_ids) { clear($flag_id) }

    $flag_ids = $dbh->selectcol_arrayref(
        "SELECT flags.id 
        FROM flags, bugs, flagexclusions e
        WHERE bugs.bug_id = ?
        AND flags.bug_id = bugs.bug_id
        AND flags.type_id = e.type_id
        AND flags.is_active = 1 
        AND (bugs.product_id = e.product_id OR e.product_id IS NULL)
        AND (bugs.component_id = e.component_id OR e.component_id IS NULL)",
        undef, $bug_id);

    foreach my $flag_id (@$flag_ids) { clear($flag_id) }

    # Take a snapshot of flags after changes.
    my @new_summaries = snapshot($bug_id, $attach_id);

    update_activity($bug_id, $attach_id, $timestamp, \@old_summaries, \@new_summaries);
}

sub update_activity {
    my ($bug_id, $attach_id, $timestamp, $old_summaries, $new_summaries) = @_;
    my $dbh = Bugzilla->dbh;

    $attach_id ||= 'NULL';
    $old_summaries = join(", ", @$old_summaries);
    $new_summaries = join(", ", @$new_summaries);
    my ($removed, $added) = diff_strings($old_summaries, $new_summaries);
    if ($removed ne $added) {
        my $sql_removed = &::SqlQuote($removed);
        my $sql_added = &::SqlQuote($added);
        my $field_id = &::GetFieldID('flagtypes.name');
        $dbh->do("INSERT INTO bugs_activity
                  (bug_id, attach_id, who, bug_when, fieldid, removed, added)
                  VALUES ($bug_id, $attach_id, $::userid, $timestamp,
                  $field_id, $sql_removed, $sql_added)");

        $dbh->do("UPDATE bugs SET delta_ts = $timestamp WHERE bug_id = ?",
                 undef, $bug_id);
    }
}

=pod

=over

=item C<create($flag, $timestamp)>

Creates a flag record in the database.

=back

=cut

sub create {
    my ($flag, $timestamp) = @_;

    # Determine the ID for the flag record by retrieving the last ID used
    # and incrementing it.
    &::SendSQL("SELECT MAX(id) FROM flags");
    $flag->{'id'} = (&::FetchOneColumn() || 0) + 1;
    
    # Insert a record for the flag into the flags table.
    my $attach_id = $flag->{'target'}->{'attachment'}->{'id'} || "NULL";
    my $requestee_id = $flag->{'requestee'} ? $flag->{'requestee'}->id : "NULL";
    &::SendSQL("INSERT INTO flags (id, type_id, 
                                      bug_id, attach_id, 
                                      requestee_id, setter_id, status, 
                                      creation_date, modification_date)
                VALUES ($flag->{'id'}, 
                        $flag->{'type'}->{'id'}, 
                        $flag->{'target'}->{'bug'}->{'id'}, 
                        $attach_id,
                        $requestee_id,
                        " . $flag->{'setter'}->id . ",
                        '$flag->{'status'}', 
                        $timestamp,
                        $timestamp)");
    
    # Send an email notifying the relevant parties about the flag creation.
    if ($flag->{'requestee'} 
          && $flag->{'requestee'}->wants_mail([EVT_FLAG_REQUESTED]))
    {
        $flag->{'addressee'} = $flag->{'requestee'};
    }

    notify($flag, "request/email.txt.tmpl");
}

=pod

=over

=item C<migrate($old_attach_id, $new_attach_id, $timestamp)>

Moves a flag from one attachment to another.  Useful for migrating
a flag from an obsolete attachment to the attachment that obsoleted it.

=back

=cut

sub migrate {
    my ($old_attach_id, $new_attach_id, $timestamp) = @_;

    # Use the date/time we were given if possible (allowing calling code
    # to synchronize the comment's timestamp with those of other records).
    $timestamp = ($timestamp ? &::SqlQuote($timestamp) : "NOW()");

    # Update the record in the flags table to point to the new attachment.
    &::SendSQL("UPDATE flags " . 
               "SET    attach_id = $new_attach_id , " . 
               "       modification_date = $timestamp " . 
               "WHERE  attach_id = $old_attach_id");
}

=pod

=over

=item C<modify($cgi, $timestamp)>

Modifies flags in the database when a user changes them.
Note that modified flags are always set active (is_active = 1) -
this will revive deleted flags that get changed through 
attachment.cgi midairs. See bug 223878 for details.

=back

=cut

sub modify {
    my ($cgi, $timestamp) = @_;

    # Use the date/time we were given if possible (allowing calling code
    # to synchronize the comment's timestamp with those of other records).
    $timestamp = ($timestamp ? &::SqlQuote($timestamp) : "NOW()");
    
    # Extract a list of flags from the form data.
    my @ids = map(/^flag-(\d+)$/ ? $1 : (), $cgi->param());
    
    # Loop over flags and update their record in the database if necessary.
    # Two kinds of changes can happen to a flag: it can be set to a different
    # state, and someone else can be asked to set it.  We take care of both
    # those changes.
    my @flags;
    foreach my $id (@ids) {
        my $flag = get($id);

        my $status = $cgi->param("flag-$id");
        my $requestee_email = trim($cgi->param("requestee-$id") || '');

        
        # Ignore flags the user didn't change. There are two components here:
        # either the status changes (trivial) or the requestee changes.
        # Change of either field will cause full update of the flag.

        my $status_changed = ($status ne $flag->{'status'});
        
        # Requestee is considered changed, if all of the following apply:
        # 1. Flag status is '?' (requested)
        # 2. Flag can have a requestee
        # 3. The requestee specified on the form is different from the 
        #    requestee specified in the db.
        
        my $old_requestee = 
          $flag->{'requestee'} ? $flag->{'requestee'}->login : '';

        my $requestee_changed = 
          ($status eq "?" && 
           $flag->{'type'}->{'is_requesteeble'} &&
           $old_requestee ne $requestee_email);
           
        next unless ($status_changed || $requestee_changed);

        
        # Since the status is validated, we know it's safe, but it's still
        # tainted, so we have to detaint it before using it in a query.
        &::trick_taint($status);
        
        if ($status eq '+' || $status eq '-') {
            &::SendSQL("UPDATE flags 
                        SET    setter_id = $::userid , 
                               requestee_id = NULL , 
                               status = '$status' , 
                               modification_date = $timestamp ,
                               is_active = 1
                        WHERE  id = $flag->{'id'}");

            my $requester;
            if ($flag->{'status'} eq '?') {
                $requester = $flag->{'setter'};
            }

            # Now update the flag object with its new values.
            $flag->{'setter'} = Bugzilla->user;
            $flag->{'requestee'} = undef;
            $flag->{'status'} = $status;

            # Send an email notifying the relevant parties about the fulfillment,
            # including the requester.
            if ($requester && $requester->wants_mail([EVT_REQUESTED_FLAG])) {
                $flag->{'addressee'} = $requester;
            }

            notify($flag, "request/email.txt.tmpl");
        }
        elsif ($status eq '?') {
            # Get the requestee, if any.
            my $requestee_id = "NULL";
            if ($requestee_email) {
                $requestee_id = login_to_id($requestee_email);
                $flag->{'requestee'} = new Bugzilla::User($requestee_id);
            }
            else {
                # If the status didn't change but we only removed the
                # requestee, we have to clear the requestee field.
                $flag->{'requestee'} = undef;
            }

            # Update the database with the changes.
            &::SendSQL("UPDATE flags 
                        SET    setter_id = $::userid , 
                               requestee_id = $requestee_id , 
                               status = '$status' , 
                               modification_date = $timestamp ,
                               is_active = 1
                        WHERE  id = $flag->{'id'}");

            # Now update the flag object with its new values.
            $flag->{'setter'} = Bugzilla->user;
            $flag->{'status'} = $status;

            # Send an email notifying the relevant parties about the request.
            if ($flag->{'requestee'}
                  && $flag->{'requestee'}->wants_mail([EVT_FLAG_REQUESTED]))
            {
                $flag->{'addressee'} = $flag->{'requestee'};
            }

            notify($flag, "request/email.txt.tmpl");
        }
        # The user unset the flag; set is_active = 0
        elsif ($status eq 'X') {
            clear($flag->{'id'});
        }
        
        push(@flags, $flag);
    }
    
    return \@flags;
}

=pod

=over

=item C<clear($id)>

Deactivate a flag.

=back

=cut

sub clear {
    my ($id) = @_;
    
    my $flag = get($id);
    
    &::PushGlobalSQLState();
    &::SendSQL("UPDATE flags SET is_active = 0 WHERE id = $id");
    &::PopGlobalSQLState();

    # If we cancel a pending request, we have to notify the requester
    # (if he wants to).
    my $requester;
    if ($flag->{'status'} eq '?') {
        $requester = $flag->{'setter'};
    }

    # Now update the flag object to its new values. The last
    # requester/setter and requestee are kept untouched (for the
    # record). Else we could as well delete the flag completely.
    $flag->{'exists'} = 0;    
    $flag->{'status'} = "X";

    if ($requester && $requester->wants_mail([EVT_REQUESTED_FLAG])) {
        $flag->{'addressee'} = $requester;
    }

    notify($flag, "request/email.txt.tmpl");
}


######################################################################
# Utility Functions
######################################################################

=pod

=over

=item C<FormToNewFlags($target, $cgi)>

Checks whether or not there are new flags to create and returns an
array of flag objects. This array is then passed to Flag::create().

=back

=cut

sub FormToNewFlags {
    my ($target, $cgi) = @_;

    my $dbh = Bugzilla->dbh;
    
    # Extract a list of flag type IDs from field names.
    my @type_ids = map(/^flag_type-(\d+)$/ ? $1 : (), $cgi->param());
    @type_ids = grep($cgi->param("flag_type-$_") ne 'X', @type_ids);

    return () unless (scalar(@type_ids) && $target->{'exists'});

    # Get information about the setter to add to each flag.
    my $setter = new Bugzilla::User($::userid);

    # Get a list of active flag types available for this target.
    my $flag_types = Bugzilla::FlagType::match(
        { 'target_type'  => $target->{'type'},
          'product_id'   => $target->{'product_id'},
          'component_id' => $target->{'component_id'},
          'is_active'    => 1 });

    my @flags;
    foreach my $flag_type (@$flag_types) {
        my $type_id = $flag_type->{'id'};

        # We are only interested in flags the user tries to create.
        next unless scalar(grep { $_ == $type_id } @type_ids);

        # Get the number of active flags of this type already set for this target.
        my $has_flags = count(
            { 'type_id'     => $type_id,
              'target_type' => $target->{'type'},
              'bug_id'      => $target->{'bug'}->{'id'},
              'attach_id'   => $target->{'attachment'}->{'id'},
              'is_active'   => 1 });

        # Do not create a new flag of this type if this flag type is
        # not multiplicable and already has an active flag set.
        next if (!$flag_type->{'is_multiplicable'} && $has_flags);

        my $status = $cgi->param("flag_type-$type_id");
        trick_taint($status);
    
        # Create the flag record and populate it with data from the form.
        my $flag = { 
            type   => $flag_type ,
            target => $target , 
            setter => $setter , 
            status => $status 
        };

        if ($status eq "?") {
            my $requestee = $cgi->param("requestee_type-$type_id");
            if ($requestee) {
                my $requestee_id = login_to_id($requestee);
                $flag->{'requestee'} = new Bugzilla::User($requestee_id);
            }
        }
        
        # Add the flag to the array of flags.
        push(@flags, $flag);
    }

    # Return the list of flags.
    return \@flags;
}

=pod

=over

=item C<GetBug($id)>

Returns a hash of information about a target bug.

=back

=cut

# Ideally, we'd use Bug.pm, but it's way too heavyweight, and it can't be
# made lighter without totally rewriting it, so we'll use this function
# until that one gets rewritten.
sub GetBug {
    my ($id) = @_;

    my $dbh = Bugzilla->dbh;

    # Save the currently running query (if any) so we do not overwrite it.
    &::PushGlobalSQLState();

    &::SendSQL("SELECT    1, short_desc, product_id, component_id,
                          COUNT(bug_group_map.group_id)
                FROM      bugs LEFT JOIN bug_group_map
                            ON (bugs.bug_id = bug_group_map.bug_id)
                WHERE     bugs.bug_id = $id " .
                $dbh->sql_group_by('bugs.bug_id',
                                   'short_desc, product_id, component_id'));

    my $bug = { 'id' => $id };
    
    ($bug->{'exists'}, $bug->{'summary'}, $bug->{'product_id'}, 
     $bug->{'component_id'}, $bug->{'restricted'}) = &::FetchSQLData();

    # Restore the previously running query (if any).
    &::PopGlobalSQLState();

    return $bug;
}

=pod

=over

=item C<GetTarget($bug_id, $attach_id)>

Someone please document this function.

=back

=cut

sub GetTarget {
    my ($bug_id, $attach_id) = @_;
    
    # Create an object representing the target bug/attachment.
    my $target = { 'exists' => 0 };

    if ($attach_id) {
        $target->{'attachment'} = new Bugzilla::Attachment($attach_id);
        if ($bug_id) {
            # Make sure the bug and attachment IDs correspond to each other
            # (i.e. this is the bug to which this attachment is attached).
            $bug_id == $target->{'attachment'}->{'bug_id'}
              || return { 'exists' => 0 };
        }
        $target->{'bug'} = GetBug($target->{'attachment'}->{'bug_id'});
        $target->{'exists'} = $target->{'attachment'}->{'exists'};
        $target->{'type'} = "attachment";
    }
    elsif ($bug_id) {
        $target->{'bug'} = GetBug($bug_id);
        $target->{'exists'} = $target->{'bug'}->{'exists'};
        $target->{'type'} = "bug";
    }

    return $target;
}

=pod

=over

=item C<notify($flag, $template_file)>

Sends an email notification about a flag being created, fulfilled
or deleted.

=back

=cut

sub notify {
    my ($flag, $template_file) = @_;

    # There is nobody to notify.
    return unless ($flag->{'addressee'} || $flag->{'type'}->{'cc_list'});

    # If the target bug is restricted to one or more groups, then we need
    # to make sure we don't send email about it to unauthorized users
    # on the request type's CC: list, so we have to trawl the list for users
    # not in those groups or email addresses that don't have an account.
    if ($flag->{'target'}->{'bug'}->{'restricted'}
        || $flag->{'target'}->{'attachment'}->{'isprivate'})
    {
        my @new_cc_list;
        foreach my $cc (split(/[, ]+/, $flag->{'type'}->{'cc_list'})) {
            my $ccuser = Bugzilla::User->new_from_login($cc,
                                                        DERIVE_GROUPS_TABLES_ALREADY_LOCKED)
              || next;

            next if $flag->{'target'}->{'bug'}->{'restricted'}
              && !$ccuser->can_see_bug($flag->{'target'}->{'bug'}->{'id'});
            next if $flag->{'target'}->{'attachment'}->{'isprivate'}
              && Param("insidergroup")
              && !$ccuser->in_group(Param("insidergroup"));
            push(@new_cc_list, $cc);
        }
        $flag->{'type'}->{'cc_list'} = join(", ", @new_cc_list);
    }

    # If there is nobody left to notify, return.
    return unless ($flag->{'addressee'} || $flag->{'type'}->{'cc_list'});

    $::vars->{'flag'} = $flag;

    # Process and send notification for each recipient
    foreach my $to ($flag->{'addressee'} ? $flag->{'addressee'}->email : '',
                    split(/[, ]+/, $flag->{'type'}->{'cc_list'}))
    {
        next unless $to;
        my $message;
        $::vars->{'to'} = $to;
        my $rv = $::template->process($template_file, $::vars, \$message);
        if (!$rv) {
            Bugzilla->cgi->header();
            ThrowTemplateError($::template->error());
        }

        Bugzilla::BugMail::MessageToMTA($message);
    }
}

# Cancel all request flags from the attachment being obsoleted.
sub CancelRequests {
    my ($bug_id, $attach_id, $timestamp) = @_;
    my $dbh = Bugzilla->dbh;

    my $request_ids =
        $dbh->selectcol_arrayref("SELECT flags.id
                                  FROM flags
                                  LEFT JOIN attachments ON flags.attach_id = attachments.attach_id
                                  WHERE flags.attach_id = ?
                                  AND flags.status = '?'
                                  AND flags.is_active = 1
                                  AND attachments.isobsolete = 0",
                                  undef, $attach_id);

    return if (!scalar(@$request_ids));

    # Take a snapshot of flags before any changes.
    my @old_summaries = snapshot($bug_id, $attach_id) if ($timestamp);
    foreach my $flag (@$request_ids) { clear($flag) }

    # If $timestamp is undefined, do not update the activity table
    return unless ($timestamp);

    # Take a snapshot of flags after any changes.
    my @new_summaries = snapshot($bug_id, $attach_id);
    update_activity($bug_id, $attach_id, $timestamp, \@old_summaries, \@new_summaries);
}

######################################################################
# Private Functions
######################################################################

=begin private

=head1 PRIVATE FUNCTIONS

=over

=item C<sqlify_criteria($criteria)>

Converts a hash of criteria into a list of SQL criteria.

=back

=cut

sub sqlify_criteria {
    # a reference to a hash containing the criteria (field => value)
    my ($criteria) = @_;

    # the generated list of SQL criteria; "1=1" is a clever way of making sure
    # there's something in the list so calling code doesn't have to check list
    # size before building a WHERE clause out of it
    my @criteria = ("1=1");
    
    # If the caller specified only bug or attachment flags,
    # limit the query to those kinds of flags.
    if (defined($criteria->{'target_type'})) {
        if    ($criteria->{'target_type'} eq 'bug')        { push(@criteria, "attach_id IS NULL") }
        elsif ($criteria->{'target_type'} eq 'attachment') { push(@criteria, "attach_id IS NOT NULL") }
    }
    
    # Go through each criterion from the calling code and add it to the query.
    foreach my $field (keys %$criteria) {
        my $value = $criteria->{$field};
        next unless defined($value);
        if    ($field eq 'type_id')      { push(@criteria, "type_id      = $value") }
        elsif ($field eq 'bug_id')       { push(@criteria, "bug_id       = $value") }
        elsif ($field eq 'attach_id')    { push(@criteria, "attach_id    = $value") }
        elsif ($field eq 'requestee_id') { push(@criteria, "requestee_id = $value") }
        elsif ($field eq 'setter_id')    { push(@criteria, "setter_id    = $value") }
        elsif ($field eq 'status')       { push(@criteria, "status       = '$value'") }
        elsif ($field eq 'is_active')    { push(@criteria, "is_active    = $value") }
    }
    
    return @criteria;
}

=pod

=over

=item C<perlify_record($exists, $id, $type_id, $bug_id, $attach_id, $requestee_id, $setter_id, $status)>

Converts a row from the database into a Perl record.

=back

=end private

=cut

sub perlify_record {
    my ($exists, $id, $type_id, $bug_id, $attach_id, 
        $requestee_id, $setter_id, $status) = @_;
    
    return undef unless defined($exists);
    
    my $flag =
      {
        exists    => $exists , 
        id        => $id ,
        type      => Bugzilla::FlagType::get($type_id) ,
        target    => GetTarget($bug_id, $attach_id) , 
        requestee => $requestee_id ? new Bugzilla::User($requestee_id) : undef,
        setter    => new Bugzilla::User($setter_id) ,
        status    => $status , 
      };
    
    return $flag;
}

=head1 SEE ALSO

=over

=item B<Bugzilla::FlagType>

=back


=head1 CONTRIBUTORS

=over

=item Myk Melez <myk@mozilla.org>

=item Jouni Heikniemi <jouni@heikniemi.net>

=item Kevin Benton <kevin.benton@amd.com>

=back

=cut

1;
