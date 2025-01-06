# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::GitHubAuth::Client::Error;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Error ();

use base qw(Exporter);
use fields qw(type error vars);

our @EXPORT = qw(ThrowUserError ThrowCodeError);
our $USE_EXCEPTION_OBJECTS = 0;

sub _new {
    my ($class, $type, $error, $vars) = @_;
    my $self = $class->fields::new();
    $self->{type}  = $type;
    $self->{error} = $error;
    $self->{vars}  = $vars // {};

    return $self;
}

sub type  { $_[0]->{type}  }
sub error { $_[0]->{error} }
sub vars  { $_[0]->{vars}  }

sub ThrowUserError {
    if ($USE_EXCEPTION_OBJECTS) {
        die __PACKAGE__->_new('user', @_);
    }
    else {
        Bugzilla::Error::ThrowUserError(@_);
    }
}

sub ThrowCodeError {
    if ($USE_EXCEPTION_OBJECTS) {
        die __PACKAGE__->_new('code', @_);
    }
    else {
        Bugzilla::Error::ThrowCodeError(@_);
    }
}

1;
