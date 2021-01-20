# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::User::Setting::Timezone;

use 5.10.1;
use strict;
use warnings;

use DateTime::TimeZone;

use parent qw(Bugzilla::User::Setting);

use Bugzilla::Constants;

sub legal_values {
    my ($self) = @_;

    return $self->{'legal_values'} if defined $self->{'legal_values'};

    my @timezones = DateTime::TimeZone->all_names;
    # Remove old formats, such as CST6CDT, EST, EST5EDT.
    @timezones = grep { $_ =~ m#.+/.+#} @timezones;
    # Append 'local' to the list, which will use the timezone
    # given by the server.
    push(@timezones, 'local');
    push(@timezones, 'UTC');

    return $self->{'legal_values'} = \@timezones;
}

1;

__END__

=head1 NAME

Bugzilla::User::Setting::Timezone - Object for a user preference setting for desired timezone

=head1 DESCRIPTION

Timezone.pm extends Bugzilla::User::Setting and implements a class specialized for
setting the desired timezone.

=head1 METHODS

=over

=item C<legal_values()>

Description: Returns all legal timezones

Params:      none

Returns:     A reference to an array containing the names of all legal timezones

=back
