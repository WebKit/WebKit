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
# The Initial Developer of the Original Code is Frédéric Buclin.
# Portions created by Frédéric Buclin are Copyright (c) 2008 Frédéric Buclin.
# All rights reserved.
#
# Contributor(s): Frédéric Buclin <LpSolit@gmail.com>

package Bugzilla::User::Setting::Timezone;

use strict;

use DateTime::TimeZone;

use base qw(Bugzilla::User::Setting);

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
