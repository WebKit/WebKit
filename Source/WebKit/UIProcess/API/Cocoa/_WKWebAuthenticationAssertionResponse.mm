/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "_WKWebAuthenticationAssertionResponseInternal.h"

#import "WKNSData.h"
#import <WebCore/WebCoreObjCExtras.h>

#if ENABLE(WEB_AUTHN)


#endif // ENABLE(WEB_AUTHN)

@implementation _WKWebAuthenticationAssertionResponse

#if ENABLE(WEB_AUTHN)

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKWebAuthenticationAssertionResponse.class, self))
        return;

    SUPPRESS_UNRETAINED_ARG _response->~WebAuthenticationAssertionResponse();

    [super dealloc];
}

- (NSString *)name
{
    return _response->name().createNSString().autorelease();
}

- (NSString *)displayName
{
    return _response->displayName().createNSString().autorelease();
}

- (NSData *)userHandle
{
    return wrapper(protect(*_response)->userHandle()).autorelease();
}

- (BOOL)synchronizable
{
    return _response->synchronizable();
}

- (NSString *)group
{
    return _response->group().createNSString().autorelease();
}

- (NSData *)credentialID
{
    return wrapper(protect(*_response)->credentialID()).autorelease();
}

- (NSString *)accessGroup
{
    return _response->accessGroup().createNSString().autorelease();
}

#endif // ENABLE(WEB_AUTHN)

- (void)setLAContext:(LAContext *)context
{
#if ENABLE(WEB_AUTHN)
    protect(*_response)->setLAContext(context);
#endif
}

#if ENABLE(WEB_AUTHN)
#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_response;
}
#endif

@end
