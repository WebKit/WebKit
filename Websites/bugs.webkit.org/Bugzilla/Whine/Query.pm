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
# The Initial Developer of the Original Code is Eric Black.
# Portions created by the Initial Developer are Copyright (C) 2009 
# Eric Black. All Rights Reserved.
#
# Contributor(s): Eric Black <black.eric@gmail.com>

package Bugzilla::Whine::Query;

use strict;

use base qw(Bugzilla::Object);

use Bugzilla::Constants;
use Bugzilla::Search::Saved;

#############
# Constants #
#############

use constant DB_TABLE => 'whine_queries';

use constant DB_COLUMNS => qw(
    id
    eventid
    query_name
    sortkey
    onemailperbug
    title
);

use constant NAME_FIELD => 'id';
use constant LIST_ORDER => 'sortkey';

####################
# Simple Accessors #
####################
sub eventid           { return $_[0]->{'eventid'};       }
sub sortkey           { return $_[0]->{'sortkey'};       }
sub one_email_per_bug { return $_[0]->{'onemailperbug'}; }
sub title             { return $_[0]->{'title'};         }
sub name              { return $_[0]->{'query_name'};    }


1;

__END__

=head1 NAME

Bugzilla::Whine::Query - A query object used by L<Bugzilla::Whine>.

=head1 SYNOPSIS

 use Bugzilla::Whine::Query;

 my $query = new Bugzilla::Whine::Query($id);

 my $event_id          = $query->eventid;
 my $id                = $query->id;
 my $query_name        = $query->name;
 my $sortkey           = $query->sortkey;
 my $one_email_per_bug = $query->one_email_per_bug;
 my $title             = $query->title;

=head1 DESCRIPTION

This module exists to represent a query for a L<Bugzilla::Whine::Event>.
Each event, which are groups of schedules and queries based on how the 
user configured the event, may have zero or more queries associated
with it. Additionally, the queries are selected from the user's saved
searches, or L<Bugzilla::Search::Saved> object with a matching C<name>
attribute for the user. 

This is an implementation of L<Bugzilla::Object>, and so has all the
same methods available as L<Bugzilla::Object>, in addition to what is
documented below.

=head1 METHODS

=head2 Constructors

=over

=item C<new>

Does not accept a bare C<name> argument. Instead, accepts only an id.

See also: L<Bugzilla::Object/new>.

=back


=head2 Accessors

These return data about the object, without modifying the object.

=over

=item C<event_id>

The L<Bugzilla::Whine::Event> object id for this object.

=item C<name>

The L<Bugzilla::Search::Saved> query object name for this object.

=item C<sortkey>

The relational sorting key as compared with other L<Bugzilla::Whine::Query>
objects.

=item C<one_email_per_bug>

Returns a numeric 1(C<true>) or 0(C<false>) to represent whether this
L<Bugzilla::Whine::Query> object is supposed to be mailed as a list of
bugs or one email per bug.

=item C<title>

The title of this object as it appears in the user forms and emails.

=back
