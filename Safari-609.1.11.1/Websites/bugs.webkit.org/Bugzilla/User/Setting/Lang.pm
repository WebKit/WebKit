# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::User::Setting::Lang;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::User::Setting);

use Bugzilla::Constants;

sub legal_values {
    my ($self) = @_;

    return $self->{'legal_values'} if defined $self->{'legal_values'};

    return $self->{'legal_values'} = Bugzilla->languages;
}

1;

__END__

=head1 NAME

Bugzilla::User::Setting::Lang - Object for a user preference setting for preferred language

=head1 DESCRIPTION

Lang.pm extends Bugzilla::User::Setting and implements a class specialized for
setting the preferred language.

=head1 METHODS

=over

=item C<legal_values()>

Description: Returns all legal languages
Params:      none
Returns:     A reference to an array containing the names of all legal languages

=back
