# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Whine;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Object);

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
