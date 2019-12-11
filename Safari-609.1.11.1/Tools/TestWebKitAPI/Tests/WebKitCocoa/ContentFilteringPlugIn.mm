/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#if ENABLE(CONTENT_FILTERING)

#import "ContentFiltering.h"
#import "MockContentFilterSettings.h"
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <mach-o/dyld.h>

using MockContentFilterSettings = WebCore::MockContentFilterSettings;
using Decision = MockContentFilterSettings::Decision;
using DecisionPoint = MockContentFilterSettings::DecisionPoint;

@interface MockContentFilterEnabler : NSObject <NSCopying, NSSecureCoding>
@end

@implementation MockContentFilterEnabler

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (instancetype)initWithCoder:(NSCoder *)decoder
{
    if (!(self = [super init]))
        return nil;

    auto& settings = MockContentFilterSettings::singleton();
    settings.setEnabled(true);
    settings.setDecision(static_cast<Decision>([decoder decodeIntForKey:@"Decision"]));
    settings.setDecisionPoint(static_cast<DecisionPoint>([decoder decodeIntForKey:@"DecisionPoint"]));
    settings.setBlockedString("blocked"_s);
    return self;
}

- (void)dealloc
{
    MockContentFilterSettings::singleton().setEnabled(false);
    [super dealloc];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
}

@end

@interface ContentFilteringPlugIn : NSObject <ContentFilteringProtocol, WKWebProcessPlugIn>
@end

@implementation ContentFilteringPlugIn {
    RetainPtr<MockContentFilterEnabler> _contentFilterEnabler;
    RetainPtr<WKWebProcessPlugInController> _plugInController;
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController initializeWithObject:(id)initializationObject
{
    ASSERT(!_plugInController);
    _plugInController = plugInController;
    [plugInController.parameters addObserver:self forKeyPath:NSStringFromClass([MockContentFilterEnabler class]) options:NSKeyValueObservingOptionInitial context:NULL];
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(ContentFilteringProtocol)];
    [[browserContextController _remoteObjectRegistry] registerExportedObject:self interface:interface];
}

- (void)dealloc
{
    [[_plugInController parameters] removeObserver:self forKeyPath:NSStringFromClass([MockContentFilterEnabler class])];
    [super dealloc];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    id contentFilterEnabler = [object valueForKeyPath:keyPath];
    ASSERT(!contentFilterEnabler || [contentFilterEnabler isKindOfClass:[MockContentFilterEnabler class]]);
    _contentFilterEnabler = contentFilterEnabler;
}

- (void)checkIfPlatformFrameworksAreLoaded:(void (^)(BOOL parentalControlsLoaded, BOOL networkExtensionLoaded))completionHandler
{
    bool parentalControlsLoaded = false;
#if HAVE(PARENTAL_CONTROLS)
    parentalControlsLoaded = NSVersionOfRunTimeLibrary("WebContentAnalysis") != -1;
#endif
    
    bool networkExtensionLoaded = false;
#if HAVE(NETWORK_EXTENSION)
    networkExtensionLoaded = NSVersionOfRunTimeLibrary("NetworkExtension") != -1;
#endif
    completionHandler(parentalControlsLoaded, networkExtensionLoaded);
}

@end

#endif // ENABLE(CONTENT_FILTERING)
