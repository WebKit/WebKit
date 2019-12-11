/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#import "BundleFormDelegateProtocol.h"
#import "PlatformUtilities.h"
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInFormDelegatePrivate.h>
#import <WebKit/WKWebProcessPlugInFrame.h>
#import <WebKit/WKWebProcessPlugInScriptWorld.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

@interface BundleFormDelegatePlugIn : NSObject <WKWebProcessPlugIn, WKWebProcessPlugInFormDelegatePrivate>
@end

@implementation BundleFormDelegatePlugIn {
    RetainPtr<WKWebProcessPlugInBrowserContextController> _browserContextController;
    RetainPtr<WKWebProcessPlugInController> _plugInController;
    RetainPtr<id <BundleFormDelegateProtocol>> _remoteObject;
    BOOL _shouldUseCustomClass;
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    ASSERT(!_browserContextController);
    ASSERT(!_plugInController);
    _browserContextController = browserContextController;
    _plugInController = plugInController;

    if (id shouldAddCustomClass = [plugInController.parameters valueForKey:@"FormDelegateShouldAddCustomClass"]) {
        ASSERT([shouldAddCustomClass isKindOfClass:[NSNumber class]]);
        _shouldUseCustomClass = [(NSNumber *)shouldAddCustomClass boolValue];
    } else
        _shouldUseCustomClass = NO;

    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(BundleFormDelegateProtocol)];
    _remoteObject = [browserContextController._remoteObjectRegistry remoteObjectProxyWithInterface:interface];

    [_browserContextController _setFormDelegate:self];
}

- (NSObject <NSSecureCoding> *)_webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller willBeginInputSessionForElement:(WKWebProcessPlugInNodeHandle *)node inFrame:(WKWebProcessPlugInFrame *)frame userIsInteracting:(BOOL)userIsInteracting
{
    if (!_shouldUseCustomClass)
        return nil;

    NSUnitConverterLinear *kilometersToMeters = [[NSUnitConverterLinear alloc] initWithCoefficient:1000.0 constant:0.0];

    return [kilometersToMeters autorelease];
}

- (void)_webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller didFocusTextField:(WKWebProcessPlugInNodeHandle *)textField inFrame:(WKWebProcessPlugInFrame *)frame
{
    [_remoteObject didGetFocus];
}

@end
