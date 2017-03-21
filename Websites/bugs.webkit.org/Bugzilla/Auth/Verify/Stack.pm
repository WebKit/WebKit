# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Auth::Verify::Stack;

use 5.10.1;
use strict;
use warnings;

use base qw(Bugzilla::Auth::Verify);
use fields qw(
    _stack
    successful
);

use Bugzilla::Hook;

use Hash::Util qw(lock_keys);
use List::MoreUtils qw(any);

sub new {
    my $class = shift;
    my $list = shift;
    my $self = $class->SUPER::new(@_);
    my %methods = map { $_ => "Bugzilla/Auth/Verify/$_.pm" } split(',', $list);
    lock_keys(%methods);
    Bugzilla::Hook::process('auth_verify_methods', { modules => \%methods });

    $self->{_stack} = [];
    foreach my $verify_method (split(',', $list)) {
        my $module = $methods{$verify_method};
        require $module;
        $module =~ s|/|::|g;
        $module =~ s/.pm$//;
        push(@{$self->{_stack}}, $module->new(@_));
    }
    return $self;
}

sub can_change_password {
    my ($self) = @_;
    # We return true if any method can change passwords.
    foreach my $object (@{$self->{_stack}}) {
        return 1 if $object->can_change_password;
    }
    return 0;
}

sub check_credentials {
    my $self = shift;
    my $result;
    foreach my $object (@{$self->{_stack}}) {
        $result = $object->check_credentials(@_);
        $self->{successful} = $object;
        last if !$result->{failure};
        # So that if none of them succeed, it's undef.
        $self->{successful} = undef;
    }
    # Returns the result at the bottom of the stack if they all fail.
    return $result;
}

sub create_or_update_user {
    my $self = shift;
    my $result;
    foreach my $object (@{$self->{_stack}}) {
        $result = $object->create_or_update_user(@_);
        last if !$result->{failure};
    }
    # Returns the result at the bottom of the stack if they all fail.
    return $result;
}

sub user_can_create_account {
    my ($self) = @_;
    # We return true if any method allows the user to create an account.
    foreach my $object (@{$self->{_stack}}) {
        return 1 if $object->user_can_create_account;
    }
    return 0;
}

sub extern_id_used {
    my ($self) = @_;
    return any { $_->extern_id_used } @{ $self->{_stack} };
}

1;
