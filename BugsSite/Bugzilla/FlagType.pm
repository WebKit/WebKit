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

=head1 NAME

Bugzilla::FlagType - A module to deal with Bugzilla flag types.

=head1 SYNOPSIS

FlagType.pm provides an interface to flag types as stored in Bugzilla.
See below for more information.

=head1 NOTES

=over

=item *

Prior to calling routines in this module, it's assumed that you have
already done a C<require CGI.pl>.  This will eventually change in a
future version when CGI.pl is removed.

=item *

Use of private functions/variables outside this module may lead to
unexpected results after an upgrade.  Please avoid using private
functions in other files/modules.  Private functions are functions
whose names start with _ or are specifically noted as being private.

=back

=cut

######################################################################
# Module Initialization
######################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

# This module implements flag types for the flag tracker.
package Bugzilla::FlagType;

# Use Bugzilla's User module which contains utilities for handling users.
use Bugzilla::User;

use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Config;

######################################################################
# Global Variables
######################################################################

=begin private

=head1 PRIVATE VARIABLES/CONSTANTS

=over

=item C<@base_columns>

basic sets of columns and tables for getting flag types from the
database.  B<Used by get, match, sqlify_criteria and perlify_record>

=back

=cut

my @base_columns = 
  ("1", "flagtypes.id", "flagtypes.name", "flagtypes.description", 
   "flagtypes.cc_list", "flagtypes.target_type", "flagtypes.sortkey", 
   "flagtypes.is_active", "flagtypes.is_requestable", 
   "flagtypes.is_requesteeble", "flagtypes.is_multiplicable", 
   "flagtypes.grant_group_id", "flagtypes.request_group_id");

=pod

=over

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

my @base_tables = ("flagtypes");

######################################################################
# Public Functions
######################################################################

=head1 PUBLIC FUNCTIONS/METHODS

=over

=item C<get($id)>

Returns a hash of information about a flag type.

=back

=cut

sub get {
    my ($id) = @_;

    my $select_clause = "SELECT " . join(", ", @base_columns);
    my $from_clause = "FROM " . join(" ", @base_tables);
    
    &::PushGlobalSQLState();
    &::SendSQL("$select_clause $from_clause WHERE flagtypes.id = $id");
    my @data = &::FetchSQLData();
    my $type = perlify_record(@data);
    &::PopGlobalSQLState();

    return $type;
}

=pod

=over

=item C<get_inclusions($id)>

Someone please document this

=back

=cut

sub get_inclusions {
    my ($id) = @_;
    return get_clusions($id, "in");
}

=pod

=over

=item C<get_exclusions($id)>

Someone please document this

=back

=cut

sub get_exclusions {
    my ($id) = @_;
    return get_clusions($id, "ex");
}

=pod

=over

=item C<get_clusions($id, $type)>

Return a hash of product/component IDs and names
associated with the flagtype:
$clusions{'product_name:component_name'} = "product_ID:component_ID"

=back

=cut

sub get_clusions {
    my ($id, $type) = @_;
    my $dbh = Bugzilla->dbh;

    my $list =
        $dbh->selectall_arrayref("SELECT products.id, products.name, " .
                                 "       components.id, components.name " . 
                                 "FROM flagtypes, flag${type}clusions " . 
                                 "LEFT OUTER JOIN products " .
                                 "  ON flag${type}clusions.product_id = products.id " . 
                                 "LEFT OUTER JOIN components " .
                                 "  ON flag${type}clusions.component_id = components.id " . 
                                 "WHERE flagtypes.id = ? " .
                                 " AND flag${type}clusions.type_id = flagtypes.id",
                                 undef, $id);
    my %clusions;
    foreach my $data (@$list) {
        my ($product_id, $product_name, $component_id, $component_name) = @$data;
        $product_id ||= 0;
        $product_name ||= "__Any__";
        $component_id ||= 0;
        $component_name ||= "__Any__";
        $clusions{"$product_name:$component_name"} = "$product_id:$component_id";
    }
    return \%clusions;
}

=pod

=over

=item C<match($criteria, $include_count)>

Queries the database for flag types matching the given criteria
and returns the set of matching types.

=back

=cut

sub match {
    my ($criteria, $include_count) = @_;

    my @tables = @base_tables;
    my @columns = @base_columns;
    my $dbh = Bugzilla->dbh;

    # Include a count of the number of flags per type if requested.
    if ($include_count) { 
        push(@columns, "COUNT(flags.id)");
        push(@tables, "LEFT OUTER JOIN flags ON flagtypes.id = flags.type_id");
    }
    
    # Generate the SQL WHERE criteria.
    my @criteria = sqlify_criteria($criteria, \@tables);
    
    # Build the query, grouping the types if we are counting flags.
    # DISTINCT is used in order to count flag types only once when
    # they appear several times in the flaginclusions table.
    my $select_clause = "SELECT DISTINCT " . join(", ", @columns);
    my $from_clause = "FROM " . join(" ", @tables);
    my $where_clause = "WHERE " . join(" AND ", @criteria);
    
    my $query = "$select_clause $from_clause $where_clause";
    $query .= " " . $dbh->sql_group_by('flagtypes.id',
              join(', ', @base_columns[2..$#base_columns]))
                    if $include_count;
    $query .= " ORDER BY flagtypes.sortkey, flagtypes.name";
    
    # Execute the query and retrieve the results.
    &::PushGlobalSQLState();
    &::SendSQL($query);
    my @types;
    while (&::MoreSQLData()) {
        my @data = &::FetchSQLData();
        my $type = perlify_record(@data);
        push(@types, $type);
    }
    &::PopGlobalSQLState();

    return \@types;
}

=pod

=over

=item C<count($criteria)>

Returns the total number of flag types matching the given criteria.

=back

=cut

sub count {
    my ($criteria) = @_;

    # Generate query components.
    my @tables = @base_tables;
    my @criteria = sqlify_criteria($criteria, \@tables);
    
    # Build the query.
    my $select_clause = "SELECT COUNT(flagtypes.id)";
    my $from_clause = "FROM " . join(" ", @tables);
    my $where_clause = "WHERE " . join(" AND ", @criteria);
    my $query = "$select_clause $from_clause $where_clause";
        
    # Execute the query and get the results.
    &::PushGlobalSQLState();
    &::SendSQL($query);
    my $count = &::FetchOneColumn();
    &::PopGlobalSQLState();

    return $count;
}

=pod

=over

=item C<validate($cgi, $bug_id, $attach_id)>

Get a list of flag types to validate.  Uses the "map" function
to extract flag type IDs from form field names by matching columns
whose name looks like "flag_type-nnn", where "nnn" is the ID,
and returning just the ID portion of matching field names.

If the attachment is new, it has no ID yet and $attach_id is set
to -1 to force its check anyway.

=back

=cut

sub validate {
    my ($cgi, $bug_id, $attach_id) = @_;

    my $user = Bugzilla->user;
    my $dbh = Bugzilla->dbh;

    my @ids = map(/^flag_type-(\d+)$/ ? $1 : (), $cgi->param());
  
    return unless scalar(@ids);
    
    # No flag reference should exist when changing several bugs at once.
    ThrowCodeError("flags_not_available", { type => 'b' }) unless $bug_id;

    # We don't check that these flag types are valid for
    # this bug/attachment. This check will be done later when
    # processing new flags, see Flag::FormToNewFlags().

    # All flag types have to be active
    my $inactive_flagtypes =
        $dbh->selectrow_array("SELECT 1 FROM flagtypes
                               WHERE id IN (" . join(',', @ids) . ")
                               AND is_active = 0 " .
                               $dbh->sql_limit(1));

    ThrowCodeError("flag_type_inactive") if $inactive_flagtypes;

    foreach my $id (@ids) {
        my $status = $cgi->param("flag_type-$id");
        
        # Don't bother validating types the user didn't touch.
        next if $status eq "X";

        # Make sure the flag type exists.
        my $flag_type = get($id);
        $flag_type 
          || ThrowCodeError("flag_type_nonexistent", { id => $id });

        # Make sure the value of the field is a valid status.
        grep($status eq $_, qw(X + - ?))
          || ThrowCodeError("flag_status_invalid", 
                            { id => $id , status => $status });

        # Make sure the user didn't request the flag unless it's requestable.
        if ($status eq '?' && !$flag_type->{is_requestable}) {
            ThrowCodeError("flag_status_invalid", 
                           { id => $id , status => $status });
        }
        
        # Make sure the user didn't specify a requestee unless the flag
        # is specifically requestable.
        my $new_requestee = trim($cgi->param("requestee_type-$id") || '');

        if ($status eq '?'
            && !$flag_type->{is_requesteeble}
            && $new_requestee)
        {
            ThrowCodeError("flag_requestee_disabled",
                           { name => $flag_type->{name} });
        }

        # Make sure the requestee is authorized to access the bug
        # (and attachment, if this installation is using the "insider group"
        # feature and the attachment is marked private).
        if ($status eq '?'
            && $flag_type->{is_requesteeble}
            && $new_requestee)
        {
            # We know the requestee exists because we ran
            # Bugzilla::User::match_field before getting here.
            my $requestee = Bugzilla::User->new_from_login($new_requestee);

            # Throw an error if the user can't see the bug.
            if (!$requestee->can_see_bug($bug_id)) {
                ThrowUserError("flag_requestee_unauthorized",
                               { flag_type => $flag_type,
                                 requestee => $requestee,
                                 bug_id    => $bug_id,
                                 attach_id => $attach_id });
            }
            
            # Throw an error if the target is a private attachment and
            # the requestee isn't in the group of insiders who can see it.
            if ($attach_id
                && Param("insidergroup")
                && $cgi->param('isprivate')
                && !$requestee->in_group(Param("insidergroup")))
            {
                ThrowUserError("flag_requestee_unauthorized_attachment",
                               { flag_type => $flag_type,
                                 requestee => $requestee,
                                 bug_id    => $bug_id,
                                 attach_id => $attach_id });
            }
        }

        # Make sure the user is authorized to modify flags, see bug 180879
        # - User in the $grant_gid group can set flags, including "+" and "-"
        next if (!$flag_type->{grant_gid}
                 || $user->in_group(&::GroupIdToName($flag_type->{grant_gid})));

        # - User in the $request_gid group can request flags
        next if ($status eq '?'
                 && (!$flag_type->{request_gid}
                     || $user->in_group(&::GroupIdToName($flag_type->{request_gid}))));

        # - Any other flag modification is denied
        ThrowUserError("flag_update_denied",
                        { name       => $flag_type->{name},
                          status     => $status,
                          old_status => "X" });
    }
}

=pod

=over

=item C<normalize(@ids)>

Given a list of flag types, checks its flags to make sure they should
still exist after a change to the inclusions/exclusions lists.

=back

=cut

sub normalize {
    # A list of IDs of flag types to normalize.
    my (@ids) = @_;
    
    my $ids = join(", ", @ids);
    
    # Check for flags whose product/component is no longer included.
    &::SendSQL("
        SELECT flags.id 
        FROM (flags INNER JOIN bugs ON flags.bug_id = bugs.bug_id)
          LEFT OUTER JOIN flaginclusions AS i
            ON (flags.type_id = i.type_id
            AND (bugs.product_id = i.product_id OR i.product_id IS NULL)
            AND (bugs.component_id = i.component_id OR i.component_id IS NULL))
        WHERE flags.type_id IN ($ids)
        AND flags.is_active = 1
        AND i.type_id IS NULL
    ");
    Bugzilla::Flag::clear(&::FetchOneColumn()) while &::MoreSQLData();
    
    &::SendSQL("
        SELECT flags.id 
        FROM flags, bugs, flagexclusions AS e
        WHERE flags.type_id IN ($ids)
        AND flags.bug_id = bugs.bug_id
        AND flags.type_id = e.type_id 
        AND flags.is_active = 1
        AND (bugs.product_id = e.product_id OR e.product_id IS NULL)
        AND (bugs.component_id = e.component_id OR e.component_id IS NULL)
    ");
    Bugzilla::Flag::clear(&::FetchOneColumn()) while &::MoreSQLData();
}

######################################################################
# Private Functions
######################################################################

=begin private

=head1 PRIVATE FUNCTIONS

=over

=item C<sqlify_criteria($criteria, $tables)>

Converts a hash of criteria into a list of SQL criteria.
$criteria is a reference to the criteria (field => value), 
$tables is a reference to an array of tables being accessed 
by the query.

=back

=cut

sub sqlify_criteria {
    my ($criteria, $tables) = @_;

    # the generated list of SQL criteria; "1=1" is a clever way of making sure
    # there's something in the list so calling code doesn't have to check list
    # size before building a WHERE clause out of it
    my @criteria = ("1=1");

    if ($criteria->{name}) {
        push(@criteria, "flagtypes.name = " . &::SqlQuote($criteria->{name}));
    }
    if ($criteria->{target_type}) {
        # The target type is stored in the database as a one-character string
        # ("a" for attachment and "b" for bug), but this function takes complete
        # names ("attachment" and "bug") for clarity, so we must convert them.
        my $target_type = &::SqlQuote(substr($criteria->{target_type}, 0, 1));
        push(@criteria, "flagtypes.target_type = $target_type");
    }
    if (exists($criteria->{is_active})) {
        my $is_active = $criteria->{is_active} ? "1" : "0";
        push(@criteria, "flagtypes.is_active = $is_active");
    }
    if ($criteria->{product_id} && $criteria->{'component_id'}) {
        my $product_id = $criteria->{product_id};
        my $component_id = $criteria->{component_id};
        
        # Add inclusions to the query, which simply involves joining the table
        # by flag type ID and target product/component.
        push(@$tables, "INNER JOIN flaginclusions ON " .
                       "flagtypes.id = flaginclusions.type_id");
        push(@criteria, "(flaginclusions.product_id = $product_id " . 
                        " OR flaginclusions.product_id IS NULL)");
        push(@criteria, "(flaginclusions.component_id = $component_id " . 
                        " OR flaginclusions.component_id IS NULL)");
        
        # Add exclusions to the query, which is more complicated.  First of all,
        # we do a LEFT JOIN so we don't miss flag types with no exclusions.
        # Then, as with inclusions, we join on flag type ID and target product/
        # component.  However, since we want flag types that *aren't* on the
        # exclusions list, we add a WHERE criteria to use only records with
        # NULL exclusion type, i.e. without any exclusions.
        my $join_clause = "flagtypes.id = flagexclusions.type_id " . 
                          "AND (flagexclusions.product_id = $product_id " . 
                          "OR flagexclusions.product_id IS NULL) " . 
                          "AND (flagexclusions.component_id = $component_id " .
                          "OR flagexclusions.component_id IS NULL)";
        push(@$tables, "LEFT JOIN flagexclusions ON ($join_clause)");
        push(@criteria, "flagexclusions.type_id IS NULL");
    }
    if ($criteria->{group}) {
        my $gid = $criteria->{group};
        detaint_natural($gid);
        push(@criteria, "(flagtypes.grant_group_id = $gid " .
                        " OR flagtypes.request_group_id = $gid)");
    }
    
    return @criteria;
}

=pod

=over

=item C<perlify_record()>

Converts data retrieved from the database into a Perl record.  Depends on the
formatting as described in @base_columns.

=back

=cut

sub perlify_record {
    my $type = {};
    
    $type->{'exists'} = $_[0];
    $type->{'id'} = $_[1];
    $type->{'name'} = $_[2];
    $type->{'description'} = $_[3];
    $type->{'cc_list'} = $_[4];
    $type->{'target_type'} = $_[5] eq "b" ? "bug" : "attachment";
    $type->{'sortkey'} = $_[6];
    $type->{'is_active'} = $_[7];
    $type->{'is_requestable'} = $_[8];
    $type->{'is_requesteeble'} = $_[9];
    $type->{'is_multiplicable'} = $_[10];
    $type->{'grant_gid'} = $_[11];
    $type->{'request_gid'} = $_[12];
    $type->{'flag_count'} = $_[13];
        
    return $type;
}

1;

=end private

=head1 SEE ALSO

=over

=item B<Bugzilla::Flags>

=back

=head1 CONTRIBUTORS

=over

=item Myk Melez <myk@mozilla.org>

=item Kevin Benton <kevin.benton@amd.com>

=back

=cut
