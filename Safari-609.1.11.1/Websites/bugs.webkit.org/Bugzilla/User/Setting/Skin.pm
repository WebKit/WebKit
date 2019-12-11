# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


package Bugzilla::User::Setting::Skin;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::User::Setting);

use Bugzilla::Constants;
use File::Spec::Functions;
use File::Basename;

use constant BUILTIN_SKIN_NAMES => ['standard'];

sub legal_values {
    my ($self) = @_;

    return $self->{'legal_values'} if defined $self->{'legal_values'};

    my $dirbase = bz_locations()->{'skinsdir'} . '/contrib';
    # Avoid modification of the list BUILTIN_SKIN_NAMES points to by copying the
    # list over instead of simply writing $legal_values = BUILTIN_SKIN_NAMES.
    my @legal_values = @{(BUILTIN_SKIN_NAMES)};

    foreach my $direntry (glob(catdir($dirbase, '*'))) {
        if (-d $direntry) {
            next if basename($direntry) =~ /^cvs$/i;
            # Stylesheet set found
            push(@legal_values, basename($direntry));
        }
    }

    return $self->{'legal_values'} = \@legal_values;
}

1;

__END__

=head1 NAME

Bugzilla::User::Setting::Skin - Object for a user preference setting for skins

=head1 DESCRIPTION

Skin.pm extends Bugzilla::User::Setting and implements a class specialized for
skins settings.

=head1 METHODS

=over

=item C<legal_values()>

Description: Returns all legal skins
Params:      none
Returns:     A reference to an array containing the names of all legal skins

=back
