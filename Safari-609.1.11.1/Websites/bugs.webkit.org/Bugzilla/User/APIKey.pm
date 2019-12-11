# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::User::APIKey;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Object);

use Bugzilla::User;
use Bugzilla::Util qw(generate_random_password trim);

#####################################################################
# Overriden Constants that are used as methods
#####################################################################

use constant DB_TABLE       => 'user_api_keys';
use constant DB_COLUMNS     => qw(
    id
    user_id
    api_key
    description
    revoked
    last_used
);

use constant UPDATE_COLUMNS => qw(description revoked last_used);
use constant VALIDATORS     => {
    api_key     => \&_check_api_key,
    description => \&_check_description,
    revoked     => \&Bugzilla::Object::check_boolean,
};
use constant LIST_ORDER     => 'id';
use constant NAME_FIELD     => 'api_key';

# turn off auditing and exclude these objects from memcached
use constant { AUDIT_CREATES => 0,
               AUDIT_UPDATES => 0,
               AUDIT_REMOVES => 0,
               USE_MEMCACHED => 0 };

# Accessors
sub id            { return $_[0]->{id}          }
sub user_id       { return $_[0]->{user_id}     }
sub api_key       { return $_[0]->{api_key}     }
sub description   { return $_[0]->{description} }
sub revoked       { return $_[0]->{revoked}     }
sub last_used     { return $_[0]->{last_used}   }

# Helpers
sub user {
    my $self = shift;
    $self->{user} //= Bugzilla::User->new({name => $self->user_id, cache => 1});
    return $self->{user};
}

sub update_last_used {
    my $self = shift;
    my $timestamp = shift
        || Bugzilla->dbh->selectrow_array('SELECT LOCALTIMESTAMP(0)');
    $self->set('last_used', $timestamp);
    $self->update;
}

# Setters
sub set_description { $_[0]->set('description', $_[1]); }
sub set_revoked     { $_[0]->set('revoked',     $_[1]); }

# Validators
sub _check_api_key     { return generate_random_password(40); }
sub _check_description { return trim($_[1]) || '';   }
1;

__END__

=head1 NAME

Bugzilla::User::APIKey - Model for an api key belonging to a user.

=head1 SYNOPSIS

  use Bugzilla::User::APIKey;

  my $api_key = Bugzilla::User::APIKey->new($id);
  my $api_key = Bugzilla::User::APIKey->new({ name => $api_key });

  # Class Functions
  $user_api_key = Bugzilla::User::APIKey->create({
      description => $description,
  });

=head1 DESCRIPTION

This package handles Bugzilla User::APIKey.

C<Bugzilla::User::APIKey> is an implementation of L<Bugzilla::Object>, and
thus provides all the methods of L<Bugzilla::Object> in addition to the methods
listed below.

=head1 METHODS

=head2 Accessor Methods

=over

=item C<id>

The internal id of the api key.

=item C<user>

The Bugzilla::User object that this api key belongs to.

=item C<user_id>

The user id that this api key belongs to.

=item C<api_key>

The API key, which is a random string.

=item C<description>

An optional string that lets the user describe what a key is used for.
For example: "Dashboard key", "Application X key".

=item C<revoked>

If true, this api key cannot be used.

=item C<last_used>

The date that this key was last used. undef if never used.

=item C<update_last_used>

Updates the last used value to the current timestamp. This is updated even
if the RPC call resulted in an error. It is not updated when the description
or the revoked flag is changed.

=item C<set_description>

Sets the new description

=item C<set_revoked>

Sets the revoked flag

=back
