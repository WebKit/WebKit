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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::Auth::Login::Stack;
use strict;
use base qw(Bugzilla::Auth::Login);
use fields qw(
    _stack
    successful
);
use Hash::Util qw(lock_keys);
use Bugzilla::Hook;
use Bugzilla::Constants;
use List::MoreUtils qw(any);

sub new {
    my $class = shift;
    my $self = $class->SUPER::new(@_);
    my $list = shift;
    my %methods = map { $_ => "Bugzilla/Auth/Login/$_.pm" } split(',', $list);
    lock_keys(%methods);
    Bugzilla::Hook::process('auth_login_methods', { modules => \%methods });

    $self->{_stack} = [];
    foreach my $login_method (split(',', $list)) {
        my $module = $methods{$login_method};
        require $module;
        $module =~ s|/|::|g;
        $module =~ s/.pm$//;
        push(@{$self->{_stack}}, $module->new(@_));
    }
    return $self;
}

sub get_login_info {
    my $self = shift;
    my $result;
    foreach my $object (@{$self->{_stack}}) {
        # See Bugzilla::WebService::Server::JSONRPC for where and why
        # auth_no_automatic_login is used.
        if (Bugzilla->request_cache->{auth_no_automatic_login}) {
            next if $object->is_automatic;
        }
        $result = $object->get_login_info(@_);
        $self->{successful} = $object;
        
        # We only carry on down the stack if this method denied all knowledge.
        last unless ($result->{failure}
                    && ($result->{failure} eq AUTH_NODATA 
                       || $result->{failure} eq AUTH_NO_SUCH_USER));
        
        # If none of the methods succeed, it's undef.
        $self->{successful} = undef;
    }
    return $result;
}

sub fail_nodata {
    my $self = shift;
    # We fail from the bottom of the stack.
    my @reverse_stack = reverse @{$self->{_stack}};
    foreach my $object (@reverse_stack) {
        # We pick the first object that actually has the method
        # implemented.
        if ($object->can('fail_nodata')) {
            $object->fail_nodata(@_);
        }
    }
}

sub can_login {
    my ($self) = @_;
    # We return true if any method can log in.
    foreach my $object (@{$self->{_stack}}) {
        return 1 if $object->can_login;
    }
    return 0;
}

sub user_can_create_account {
    my ($self) = @_;
    # We return true if any method allows users to create accounts.
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
