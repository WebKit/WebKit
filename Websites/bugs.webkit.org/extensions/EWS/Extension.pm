# Copyright (C) 2018 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

package Bugzilla::Extension::EWS;

use strict;
use warnings;

use parent qw(Bugzilla::Extension);

use Bugzilla::Constants;
use Bugzilla::Group;
use Bugzilla::User;

our $VERSION = "1.0.0";

sub isReviewFlag($);

sub new
{
    my ($class, @args) = @_;
    my $self = $class->SUPER::new(@args);
    $self->{shouldCCFeeder} = 0;
    $self->{reviewFlagSeen} = 0;
    return $self;
}

sub object_before_set
{
    my ($self, $args) = @_;

    return if $self->{reviewFlagSeen};
    return if !isReviewFlag($args->{object});

    my $willChangeValue = defined($args->{field}) && $args->{field} eq "status";
    return if !$willChangeValue;

    my $reviewRequested = $args->{value} eq "?";
    $self->{shouldCCFeeder} = 1 if $reviewRequested;
    $self->{reviewFlagSeen} = 1;
}

sub bug_start_of_update
{
    my ($self, $args) = @_;

    return if !$self->{shouldCCFeeder};

    my $feeder = new Bugzilla::User({name => Bugzilla->params->{"ews_feeder_login"}});
    return if !$feeder || $feeder->can_see_bug($args->{bug}->id());

    $args->{bug}->add_cc($feeder);
}

sub config_add_panels
{
    my ($self, $args) = @_;

    my $modules = $args->{panel_modules};
    $modules->{EWS} = "Bugzilla::Extension::EWS::ParamsPanelUI";
}

###
# Helper functions
##

sub isReviewFlag($)
{
    my ($mayBeFlag) = @_;
    return $mayBeFlag->isa("Bugzilla::Flag") && $mayBeFlag->name() eq "review";
}

__PACKAGE__->NAME;
