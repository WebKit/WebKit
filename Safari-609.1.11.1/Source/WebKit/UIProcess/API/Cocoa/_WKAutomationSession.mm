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
#import "_WKAutomationSessionInternal.h"

#import "AutomationSessionClient.h"
#import "WKAPICast.h"
#import "WKProcessPool.h"
#import "WebAutomationSession.h"
#import "_WKAutomationSessionConfiguration.h"
#import "_WKAutomationSessionDelegate.h"
#import <wtf/WeakObjCPtr.h>

@implementation _WKAutomationSession {
    RetainPtr<_WKAutomationSessionConfiguration> _configuration;
    WeakObjCPtr<id <_WKAutomationSessionDelegate>> _delegate;
}

- (instancetype)init
{
    return [self initWithConfiguration:[[[_WKAutomationSessionConfiguration alloc] init] autorelease]];
}

- (instancetype)initWithConfiguration:(_WKAutomationSessionConfiguration *)configuration
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebAutomationSession>(self);

    _configuration = adoptNS([configuration copy]);

    return self;
}

- (void)dealloc
{
    _session->setClient(nullptr);
    _session->~WebAutomationSession();

    [super dealloc];
}

- (id <_WKAutomationSessionDelegate>)delegate
{
    return _delegate.getAutoreleased();
}

- (void)setDelegate:(id <_WKAutomationSessionDelegate>)delegate
{
    _delegate = delegate;
    _session->setClient(delegate ? makeUnique<WebKit::AutomationSessionClient>(delegate) : nullptr);
}

- (NSString *)sessionIdentifier
{
    return _session->sessionIdentifier();
}

- (void)setSessionIdentifier:(NSString *)sessionIdentifier
{
    _session->setSessionIdentifier(sessionIdentifier);
}

- (_WKAutomationSessionConfiguration *)configuration
{
    return [[_configuration copy] autorelease];
}

- (BOOL)isPaired
{
    return _session->isPaired();
}

- (BOOL)isSimulatingUserInteraction
{
    return _session->isSimulatingUserInteraction();
}

- (void)terminate
{
    _session->terminate();
}

#if PLATFORM(MAC)
- (BOOL)wasEventSynthesizedForAutomation:(NSEvent *)event
{
    return _session->wasEventSynthesizedForAutomation(event);
}

- (void)markEventAsSynthesizedForAutomation:(NSEvent *)event
{
    _session->markEventAsSynthesizedForAutomation(event);
}
#endif

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_session;
}

@end
