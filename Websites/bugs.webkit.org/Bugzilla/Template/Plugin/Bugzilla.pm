# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Template::Plugin::Bugzilla;

use 5.10.1;
use strict;
use warnings;

use parent qw(Template::Plugin);

use Bugzilla;

sub new {
    my ($class, $context) = @_;

    return bless {}, $class;
}

sub AUTOLOAD {
    my $class = shift;
    our $AUTOLOAD;

    $AUTOLOAD =~ s/^.*:://;

    return if $AUTOLOAD eq 'DESTROY';

    return Bugzilla->$AUTOLOAD(@_);
}

1;

__END__

=head1 NAME

Bugzilla::Template::Plugin::Bugzilla

=head1 DESCRIPTION

Template Toolkit plugin to allow access to the persistent C<Bugzilla>
object.

=head1 SEE ALSO

L<Bugzilla>, L<Template::Plugin>
