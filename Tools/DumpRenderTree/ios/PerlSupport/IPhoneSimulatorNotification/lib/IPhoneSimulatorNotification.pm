# Copyright (C) 2009 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

package IPhoneSimulatorNotification;

use 5.008000;
use strict;
use warnings;

require Exporter;

our @ISA = qw(DynaLoader Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration use IPhoneSimulatorNotification ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
    
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
    &applicationLaunchedApplicationPID
    &applicationLaunchedSessionUUID
    &hasReceivedApplicationLaunchedNotification
    &hasReceivedApplicationQuitNotification
    &hasReceivedReadyNotification
    &new
    &postEndSessionNotification
    &postStartSessionNotification
    &readyNotificationCallback
    &setHasReceivedReadyNotification
    &startObservingApplicationLaunchedNotification
    &startObservingApplicationQuitNotification
    &startObservingReadyNotification
    &stopObservingApplicationLaunchedNotification
    &stopObservingApplicationQuitNotification
    &stopObservingReadyNotification
);

our $VERSION = '1.01';

require XSLoader;
XSLoader::load('IPhoneSimulatorNotification', $VERSION);

# Preloaded methods go here.

use Foundation;

PerlObjCBridge::preloadSelectors('IPhoneSimulatorNotification');

sub new
{
    my $class = shift;

    my $self = {};
    $self->{hasReceivedApplicationLaunchedNotification} = {};
    $self->{hasReceivedApplicationQuitNotification} = {};
    $self->{hasReceivedReadyNotification} = 0;
    bless($self, $class);

    return $self;
}

sub DESTROY
{
    my $self = shift;

    $self->stopObservingReadyNotification();
}

sub applicationLaunchedNotificationCallback_
{
    my $self = shift;
    my $nsNotification = shift;
    my $userInfo = $nsNotification->userInfo();

    my $keyObject;
    my $enumerator = $userInfo->keyEnumerator();
    while ($keyObject = $enumerator->nextObject() and $$keyObject) {
        my $key = $keyObject->UTF8String();
        my $valueObject = $userInfo->objectForKey_($keyObject);
        if ($key eq "errorString") {
            die "iPhone Simulator returned error: " . $valueObject->UTF8String();
        } elsif ($key eq "applicationPID") {
            $self->{hasReceivedApplicationLaunchedNotification}->{applicationPID} = $valueObject->intValue();
        } elsif ($key eq "sessionUUID") {
            $self->{hasReceivedApplicationLaunchedNotification}->{sessionUUID} = $valueObject->UTF8String();
        }
    }
}

sub applicationQuitNotificationCallback_
{
    my $self = shift;
    my $nsNotification = shift;
    my $userInfo = $nsNotification->userInfo();

    my $keyObject;
    my $enumerator = $userInfo->keyEnumerator();
    while ($keyObject = $enumerator->nextObject() and $$keyObject) {
        my $key = $keyObject->UTF8String();
        my $valueObject = $userInfo->objectForKey_($keyObject);
        if ($key eq "errorString") {
            my $errorString = $valueObject->UTF8String();
            warn "iPhone Simulator returned error: " . $errorString
                unless $errorString eq "The simulated application quit.";
        } elsif ($key eq "sessionUUID") {
            $self->{hasReceivedApplicationQuitNotification}->{sessionUUID} = $valueObject->UTF8String();
        }
    }
}

sub readyNotificationCallback
{
    my $self = shift;
    $self->setHasReceivedReadyNotification(1);
}

sub applicationLaunchedApplicationPID
{
    my $self = shift;
    return $self->{hasReceivedApplicationLaunchedNotification}->{applicationPID};
}

sub applicationLaunchedSessionUUID
{
    my $self = shift;
    return $self->{hasReceivedApplicationLaunchedNotification}->{sessionUUID};
}

sub hasReceivedApplicationLaunchedNotification
{
    my $self = shift;
    return scalar(keys(%{$self->{hasReceivedApplicationLaunchedNotification}})) > 0;
}

sub hasReceivedApplicationQuitNotification
{
    my $self = shift;
    return scalar(keys(%{$self->{hasReceivedApplicationQuitNotification}})) > 0;
}

sub hasReceivedReadyNotification
{
    my $self = shift;
    return $self->{hasReceivedReadyNotification};
}

sub postEndSessionNotification
{
    my $self = shift;
    my $dict = shift;

    my $userInfo = NSMutableDictionary->dictionaryWithCapacity_(2);
    for my $property (qw(dontQuit sessionUUID)) {
        if (exists $dict->{$property}) {
            my $key = NSString->stringWithCString_($property);
            my $value = $dict->{$property};
            $userInfo->setObject_forKey_($value, $key);
        }
    }
    $userInfo->setObject_forKey_(NSNumber->numberWithInt_(1), NSString->stringWithCString_("version"));

    my $center = NSDistributedNotificationCenter->defaultCenter();
    $center->postNotificationName_object_userInfo_("com.apple.iphonesimulator.endSession", undef, $userInfo);
}

sub postStartSessionNotification
{
    my $self = shift;
    my $dict = shift;

    my $userInfo = NSMutableDictionary->dictionaryWithCapacity_(4);
    for my $property (qw(applicationArguments applicationEnvironment applicationIdentifier applicationPath deviceFamily deviceInfo productType sessionOwner sessionUUID sdkRoot version waitForDebugger)) {
        if (exists $dict->{$property}) {
            my $key = NSString->stringWithCString_($property);
            my $value = $dict->{$property};
            $userInfo->setObject_forKey_($value, $key);
        }
    }
    $userInfo->setObject_forKey_(NSNumber->numberWithInt_(1), NSString->stringWithCString_("version"));

    my $center = NSDistributedNotificationCenter->defaultCenter();
    $center->postNotificationName_object_userInfo_("com.apple.iphonesimulator.startSession", undef, $userInfo);
}

sub setHasReceivedReadyNotification
{
    my $self = shift;
    $self->{hasReceivedReadyNotification} = shift;
}

sub startObservingApplicationLaunchedNotification
{
    my $self = shift;
    my $center = NSDistributedNotificationCenter->defaultCenter();
    $center->addObserver_selector_name_object_($self, 'applicationLaunchedNotificationCallback:', "com.apple.iphonesimulator.applicationLaunched", undef); 
}

sub startObservingApplicationQuitNotification
{
    my $self = shift;
    my $center = NSDistributedNotificationCenter->defaultCenter();
    $center->addObserver_selector_name_object_($self, 'applicationQuitNotificationCallback:', "com.apple.iphonesimulator.applicationQuit", undef); 
}

sub startObservingReadyNotification
{
    my $self = shift;
    my $center = NSDistributedNotificationCenter->defaultCenter();
    $center->addObserver_selector_name_object_($self, 'readyNotificationCallback', "com.apple.iphonesimulator.ready", undef); 
}

sub stopObservingApplicationLaunchedNotification
{
    my $self = shift;
    my $center = NSDistributedNotificationCenter->defaultCenter();
    $center->removeObserver_name_object_($self, "com.apple.iphonesimulator.applicationLaunched", undef); 
}

sub stopObservingApplicationQuitNotification
{
    my $self = shift;
    my $center = NSDistributedNotificationCenter->defaultCenter();
    $center->removeObserver_name_object_($self, "com.apple.iphonesimulator.applicationQuit", undef); 
}

sub stopObservingReadyNotification
{
    my $self = shift;
    my $center = NSDistributedNotificationCenter->defaultCenter();
    $center->removeObserver_name_object_($self, "com.apple.iphonesimulator.ready", undef); 
}

1;
__END__

=head1 NAME

IPhoneSimulatorNotification - Perl extension for receiving distributed notifications from the iPhone Simulator

=head1 SYNOPSIS

    use IPhoneSimulatorNotification;

    my $iphoneSimulatorNotification = new IPhoneSimulatorNotification;
    $iphoneSimulatorNotification->startObservingReadyNotification();

    while (!$iphoneSimulatorNotification->hasReceivedReadyNotification()) {
        my $date = NSDate->dateWithTimeIntervalSinceNow(0.1);
        NSRunLoop->currentRunLoop->runUntilDate($date);
    }

    $iphoneSimulatorNotification->stopObservingReadyNotification();

=head1 DESCRIPTION

IPhoneSimulatorNotification is used to receive distributed notifications
from the iPhone Simulator.

=head2 Export

C<&applicationLaunchedApplicationPID>,
C<&applicationLaunchedSessionUUID>,
C<&hasReceivedApplicationLaunchedNotification>,
C<&hasReceivedApplicationQuitNotification>,
C<&hasReceivedReadyNotification>,
C<&new>,
C<&postEndSessionNotification>,
C<&postStartSessionNotification>,
C<&readyNotificationCallback>,
C<&setHasReceivedReadyNotification>,
C<&startObservingApplicationLaunchedNotification>,
C<&startObservingApplicationQuitNotification>,
C<&startObservingReadyNotification>,
C<&stopObservingApplicationLaunchedNotification>,
C<&stopObservingApplicationQuitNotification>,
and
C<&stopObservingReadyNotification>
are exported by default.

=head2 Methods

=over 4

=item applicationLaunchedApplicationPID()

Returns the PID of the launched application after the application launched
distributed notification has been received.

=item applicationLaunchedSessionUUID()

Returns the session UUID of the launched application after the application
launched distributed notification has been received.

=item hasReceivedApplicationLaunchedNotification()

Returns true if the "com.apple.iphonesimulator.applicationLaunched"
distributed notification has been received, else false.

=item hasReceivedApplicationQuitNotification()

Returns true if the "com.apple.iphonesimulator.applicationQuit" distributed
notification has been received, else false.

=item hasReceivedReadyNotification()

Returns 1 if the "com.apple.iphonesimulator.ready" distributed notification
has been received from the iPhone Simulator, else returns 0.

Note that if you want to reuse the IPhoneSimulatorNotification object, you
must reset the internal state using the C<&setHasReceivedReadyNotification>
method.

=item new()

Creates a new IPhoneSimulatorNotification object.

=item postEndSessionNotification()

Sends "com.apple.iphonesimulator.endSession" distributed notification for
the iPhone Simulator process.

=item postStartSessionNotification()

Sends "com.apple.iphonesimulator.startSession" distributed notification
for the iPhone Simulator process.

=item readyNotificationCallback()

Method used to receive the "com.apple.iphonesimulator.ready" distributed
notification.  Calls C<&setHasReceivedReadyNotification> to set the value
to 1.

=item setHasReceivedReadyNotification($)

Sets the internal state to the value provided.  Should be 0 or 1.

=item startObservingApplicationLaunchedNotification()

Starts observing the "com.apple.iphonesimulator.applicationLaunched"
distributed notification.

=item startObservingApplicationQuitNotification()

Starts observing the "com.apple.iphonesimulator.applicationQuit"
distributed notification.

=item startObservingReadyNotification()

Starts observing the "com.apple.iphonesimulator.ready" distributed
notification.

=item stopObservingApplicationLaunchedNotification()

Stops observing the "com.apple.iphonesimulator.applicationLaunched"
distributed notification.

=item stopObservingApplicationQuitNotification()

Stops observing the "com.apple.iphonesimulator.applicationQuit"
distributed notification.

=item stopObservingReadyNotification()

Stops observing the "com.apple.iphonesimulator.ready" distributed
notification.

=back

=head1 SEE ALSO

L<Foundation> and L<PerlObjCBridge>.

=cut
