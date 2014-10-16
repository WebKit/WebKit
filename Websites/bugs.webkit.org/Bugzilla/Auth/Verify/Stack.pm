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
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::Auth::Verify::Stack;
use strict;
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
