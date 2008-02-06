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
#                 Myk Melez <myk@mozilla.org>

############################################################################
# Module Initialization
############################################################################

use strict;

package Bugzilla::Attachment;

# This module requires that its caller have said "require CGI.pl" to import
# relevant functions from that script and its companion globals.pl.

# Use the Flag module to handle flags.
use Bugzilla::Flag;
use Bugzilla::Config qw(:locations);
use Bugzilla::User;

############################################################################
# Functions
############################################################################

sub new {
    # Returns a hash of information about the attachment with the given ID.

    my ($invocant, $id) = @_;
    return undef if !$id;
    my $self = { 'id' => $id };
    my $class = ref($invocant) || $invocant;
    bless($self, $class);
    
    &::PushGlobalSQLState();
    &::SendSQL("SELECT 1, description, bug_id, isprivate FROM attachments " . 
               "WHERE attach_id = $id");
    ($self->{'exists'},
     $self->{'summary'},
     $self->{'bug_id'},
     $self->{'isprivate'}) = &::FetchSQLData();
    &::PopGlobalSQLState();

    return $self;
}

sub query
{
  # Retrieves and returns an array of attachment records for a given bug. 
  # This data should be given to attachment/list.html.tmpl in an
  # "attachments" variable.
  my ($bugid) = @_;

  my $dbh = Bugzilla->dbh;

  # Retrieve a list of attachments for this bug and write them into an array
  # of hashes in which each hash represents a single attachment.
  my $list = $dbh->selectall_arrayref("SELECT attach_id, " .
                                      $dbh->sql_date_format('creation_ts', '%Y.%m.%d %H:%i') .
                                      ", mimetype, description, ispatch,
                                      isobsolete, isprivate, LENGTH(thedata)
                                      FROM attachments
                                      WHERE bug_id = ? ORDER BY attach_id",
                                      undef, $bugid);

  my @attachments = ();
  foreach my $row (@$list) {
    my %a;
    ($a{'attachid'}, $a{'date'}, $a{'contenttype'},
     $a{'description'}, $a{'ispatch'}, $a{'isobsolete'},
     $a{'isprivate'}, $a{'datasize'}) = @$row;

    # Retrieve a list of flags for this attachment.
    $a{'flags'} = Bugzilla::Flag::match({ 'attach_id' => $a{'attachid'},
                                          'is_active' => 1 });

    # A zero size indicates that the attachment is stored locally.
    if ($a{'datasize'} == 0) {
        my $attachid = $a{'attachid'};
        my $hash = ($attachid % 100) + 100;
        $hash =~ s/.*(\d\d)$/group.$1/;
        if (open(AH, "$attachdir/$hash/attachment.$attachid")) {
            $a{'datasize'} = (stat(AH))[7];
            close(AH);
        }
    }
    push @attachments, \%a;
  }

  return \@attachments;  
}

1;
