# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::BugUserLastVisit;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Object);

#####################################################################
# Overriden Constants that are used as methods
#####################################################################

use constant DB_TABLE       => 'bug_user_last_visit';
use constant DB_COLUMNS     => qw( id user_id bug_id last_visit_ts );
use constant UPDATE_COLUMNS => qw( last_visit_ts );
use constant VALIDATORS     => {};
use constant LIST_ORDER     => 'id';
use constant NAME_FIELD     => 'id';

# turn off auditing and exclude these objects from memcached
use constant { AUDIT_CREATES => 0,
               AUDIT_UPDATES => 0,
               AUDIT_REMOVES => 0,
               USE_MEMCACHED => 0 };

#####################################################################
# Provide accessors for our columns
#####################################################################

sub id            { return $_[0]->{id}            }
sub bug_id        { return $_[0]->{bug_id}        }
sub user_id       { return $_[0]->{user_id}       }
sub last_visit_ts { return $_[0]->{last_visit_ts} }

sub user {
    my $self = shift;

    $self->{user} //= Bugzilla::User->new({ id => $self->user_id, cache => 1 });
    return $self->{user};
}

1;
__END__

=head1 NAME

Bugzilla::BugUserLastVisit - Model for BugUserLastVisit bug search data

=head1 SYNOPSIS

  use Bugzilla::BugUserLastVisit;

  my $lv = Bugzilla::BugUserLastVisit->new($id);

  # Class Functions
  $user = Bugzilla::BugUserLastVisit->create({
      bug_id        => $bug_id,
      user_id       => $user_id,
      last_visit_ts => $last_visit_ts
  });

=head1 DESCRIPTION

This package handles Bugzilla BugUserLastVisit.

C<Bugzilla::BugUserLastVisit> is an implementation of L<Bugzilla::Object>, and
thus provides all the methods of L<Bugzilla::Object> in addition to the methods
listed below.

=head1 METHODS

=head2 Accessor Methods

=over

=item C<id>

=item C<bug_id>

=item C<user_id>

=item C<last_visit_ts>

=item C<user>

=back
