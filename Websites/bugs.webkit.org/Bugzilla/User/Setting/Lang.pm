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
# The Initial Developer of the Original Code is Marc Schumann.
# Portions created by Marc Schumann are Copyright (c) 2007 Marc Schumann.
# All rights reserved.
#
# Contributor(s): Marc Schumann <wurblzap@gmail.com>

package Bugzilla::User::Setting::Lang;

use strict;

use base qw(Bugzilla::User::Setting);

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
