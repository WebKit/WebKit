/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "WKHitTestResultInternal.h"

#import "APIJSHandle.h"
#import "WKJSHandleInternal.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/URL.h>

namespace WebKit {

NSURL *URLFromHitTestResultString(const String& urlString)
{
    // Avoid +[NSURL URLWithString:], which re-encodes the path when it contains characters
    // that WebCore's URL parser leaves literal (e.g. '[' and ']'), double-encoding the
    // existing %XX sequences. CFURLCreateAbsoluteURLWithBytes preserves them.
    if (urlString.isEmpty())
        return nil;
    return WTF::URL { urlString }.createNSURL().autorelease();
}

}

@implementation WKHitTestResult

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKHitTestResult.class, self))
        return;

    SUPPRESS_UNCOUNTED_ARG _hitTestResult->~HitTestResult();

    [super dealloc];
}

- (WKJSHandle *)domNodeHandle
{
    return wrapper(_hitTestResult->domNodeHandle());
}

- (NSURL *)absoluteLinkURL
{
    return WebKit::URLFromHitTestResultString(_hitTestResult->absoluteLinkURL());
}

- (NSString *)linkTitle
{
    return nsStringNilIfNull(_hitTestResult->linkTitle()).autorelease();
}

- (NSString *)linkLabel
{
    return nsStringNilIfNull(_hitTestResult->linkLabel()).autorelease();
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_hitTestResult;
}

@end
