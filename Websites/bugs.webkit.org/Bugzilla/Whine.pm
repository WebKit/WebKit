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
# Portions created by the Initial Developer are Copyright (C) 2010 
# Eric Black. All Rights Reserved.
#
# Contributor(s): Eric Black <black.eric@gmail.com>

use strict;

package Bugzilla::Whine;

use base qw(Bugzilla::Object);

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Util;
use Bugzilla::Whine::Schedule;
use Bugzilla::Whine::Query;

#############
# Constants #
#############

use constant DB_TABLE => 'whine_events';

use constant DB_COLUMNS => qw(
    id
    owner_userid
    subject
    body
    mailifnobugs
);

use constant LIST_ORDER => 'id';

####################
# Simple Accessors #
####################
sub subject         { return $_[0]->{'subject'};      }
sub body            { return $_[0]->{'body'};         }
sub mail_if_no_bugs { return $_[0]->{'mailifnobugs'}; }

sub user {
    my ($self) = @_;
    return $self->{user} if defined $self->{user};
    $self->{user} = new Bugzilla::User($self->{'owner_userid'});
    return $self->{user};
}

1;

__END__

=head1 NAME

Bugzilla::Whine - A Whine event

=head1 SYNOPSIS

 use Bugzilla::Whine;

 my $event = new Bugzilla::Whine($event_id);

 my $subject      = $event->subject;
 my $body         = $event->body;
 my $mailifnobugs = $event->mail_if_no_bugs;
 my $user         = $event->user;

=head1 DESCRIPTION

This module exists to represent a whine event that has been
saved to the database.

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

=item C<subject>

Returns the subject of the whine event.

=item C<body>

Returns the body of the whine event.

=item C<mail_if_no_bugs>

Returns a numeric 1(C<true>) or 0(C<false>) to represent whether this
whine event object is supposed to be mailed even if there are no bugs
returned by the query.

=item C<user>

Returns the L<Bugzilla::User> object for the owner of the L<Bugzilla::Whine>
event.

=back
