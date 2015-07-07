/*
 * Copyright (C) 2006, 2007, 2008 Apple, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "ResourceRequest.h"

#if PLATFORM(MAC)

#import "RuntimeApplicationChecks.h"

#import <Foundation/Foundation.h>

@interface NSURLRequest (WebNSURLRequestDetails)
- (CFURLRequestRef)_CFURLRequest;
- (id)_initWithCFURLRequest:(CFURLRequestRef)request;
@end

namespace WebCore {

static bool initQuickLookResourceCachingQuirks()
{
    if (applicationIsSafari())
        return false;
    
    NSArray* frameworks = [NSBundle allFrameworks];
    
    if (!frameworks)
        return false;
    
    for (unsigned int i = 0; i < [frameworks count]; i++) {
        NSBundle* bundle = [frameworks objectAtIndex: i];
        const char* bundleID = [[bundle bundleIdentifier] UTF8String];
        if (bundleID && !strcasecmp(bundleID, "com.apple.QuickLookUIFramework"))
            return true;
    }
    return false;
}

bool ResourceRequest::useQuickLookResourceCachingQuirks()
{
    static bool flag = initQuickLookResourceCachingQuirks();
    return flag;
}

#if USE(CFNETWORK)

ResourceRequest::ResourceRequest(NSURLRequest *nsRequest)
    : ResourceRequestBase()
    , m_cfRequest([nsRequest _CFURLRequest])
    , m_nsRequest(nsRequest)
{
}

void ResourceRequest::updateNSURLRequest()
{
    if (m_cfRequest)
        m_nsRequest = adoptNS([[NSURLRequest alloc] _initWithCFURLRequest:m_cfRequest.get()]);
}

void ResourceRequest::applyWebArchiveHackForMail()
{
    // Hack because Mail checks for this property to detect data / archive loads
    _CFURLRequestSetProtocolProperty(cfURLRequest(DoNotUpdateHTTPBody), CFSTR("WebDataRequest"), CFSTR(""));
}

#endif

} // namespace WebCore

#endif
