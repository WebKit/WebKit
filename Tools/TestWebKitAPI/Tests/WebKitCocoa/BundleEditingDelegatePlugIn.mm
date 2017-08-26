/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if WK_API_ENABLED

#import "BundleEditingDelegateProtocol.h"
#import "PlatformUtilities.h"
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInEditingDelegate.h>
#import <WebKit/WKWebProcessPlugInFrame.h>
#import <WebKit/WKWebProcessPlugInScriptWorld.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

@interface BundleEditingDelegatePlugIn : NSObject <WKWebProcessPlugIn, WKWebProcessPlugInEditingDelegate>
@end

@implementation BundleEditingDelegatePlugIn {
    RetainPtr<WKWebProcessPlugInBrowserContextController> _browserContextController;
    RetainPtr<WKWebProcessPlugInController> _plugInController;
    RetainPtr<id <BundleEditingDelegateProtocol>> _remoteObject;
    BOOL _editingDelegateShouldInsertText;
    BOOL _shouldOverridePerformTwoStepDrop;
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    ASSERT(!_browserContextController);
    ASSERT(!_plugInController);
    _browserContextController = browserContextController;
    _plugInController = plugInController;

    if (id shouldInsertText = [plugInController.parameters valueForKey:@"EditingDelegateShouldInsertText"]) {
        ASSERT([shouldInsertText isKindOfClass:[NSNumber class]]);
        _editingDelegateShouldInsertText = [(NSNumber *)shouldInsertText boolValue];
    } else
        _editingDelegateShouldInsertText = YES;

    _shouldOverridePerformTwoStepDrop = [[plugInController.parameters valueForKey:@"BundleOverridePerformTwoStepDrop"] boolValue];

    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(BundleEditingDelegateProtocol)];
    _remoteObject = [browserContextController._remoteObjectRegistry remoteObjectProxyWithInterface:interface];

    [_browserContextController _setEditingDelegate:self];
}

- (BOOL)_webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller shouldInsertText:(nonnull NSString *)text replacingRange:(nonnull WKWebProcessPlugInRangeHandle *)range givenAction:(WKEditorInsertAction)action
{
    JSValue *jsRange = [range.frame jsRangeForRangeHandle:range inWorld:[WKWebProcessPlugInScriptWorld normalWorld]];
    [_remoteObject shouldInsertText:text replacingRange:[jsRange toString] givenAction:action];
    return _editingDelegateShouldInsertText;
}

- (void)_webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller willWriteRangeToPasteboard:(WKWebProcessPlugInRangeHandle *)range
{
    JSValue *jsRange = [range.frame jsRangeForRangeHandle:range inWorld:[WKWebProcessPlugInScriptWorld normalWorld]];
    [_remoteObject willWriteToPasteboard:[jsRange toString]];
}

- (NSDictionary<NSString *, NSData *> *)_webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller pasteboardDataForRange:(WKWebProcessPlugInRangeHandle *)range
{
    return @{ @"org.webkit.data" : [NSData dataWithBytesNoCopy:(void*)"hello" length:5 freeWhenDone:NO] };
}

- (void)_webProcessPlugInBrowserContextControllerDidWriteToPasteboard:(WKWebProcessPlugInBrowserContextController *)controller
{
    [_remoteObject didWriteToPasteboard];
}

- (BOOL)_webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller performTwoStepDrop:(WKWebProcessPlugInNodeHandle *)fragment atDestination:(WKWebProcessPlugInRangeHandle *)destination isMove:(BOOL)isMove
{
    return _shouldOverridePerformTwoStepDrop;
}

@end

#endif // WK_API_ENABLED
