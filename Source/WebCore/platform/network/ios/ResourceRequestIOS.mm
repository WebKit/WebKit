/*
 * Copyright (C) 2014 Apple, Inc.  All rights reserved.
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

#if PLATFORM(IOS)

@interface NSURLRequest (WebNSURLRequestDetails)
- (CFURLRequestRef)_CFURLRequest;
- (id)_initWithCFURLRequest:(CFURLRequestRef)request;
@end

namespace WebCore {

bool ResourceRequest::useQuickLookResourceCachingQuirks()
{
    return false;
}

#if USE(CFNETWORK)

ResourceRequest::ResourceRequest(NSURLRequest *nsRequest)
    : ResourceRequestBase()
    , m_mainResourceRequest(false)
    , m_cfRequest([nsRequest _CFURLRequest])
    , m_nsRequest(nsRequest)
{
}

void ResourceRequest::updateNSURLRequest()
{
    if (m_cfRequest)
        m_nsRequest = adoptNS([[NSMutableURLRequest alloc] _initWithCFURLRequest:m_cfRequest.get()]);
}

void ResourceRequest::clearOrUpdateNSURLRequest()
{
    // There is client code that extends NSURLRequest and expects to get back, in the delegate
    // callbacks, an object of the same type that they passed into WebKit. To keep then running, we
    // create an object of the same type and return that. See <rdar://9843582>.
    // Also, developers really really want an NSMutableURLRequest so try to create an
    // NSMutableURLRequest instead of NSURLRequest.
    static Class nsURLRequestClass = [NSURLRequest class];
    static Class nsMutableURLRequestClass = [NSMutableURLRequest class];
    Class requestClass = [m_nsRequest.get() class];

    if (!m_cfRequest)
        return;

    if (requestClass && requestClass != nsURLRequestClass && requestClass != nsMutableURLRequestClass)
        m_nsRequest = adoptNS([[requestClass alloc] _initWithCFURLRequest:m_cfRequest.get()]);
    else
        m_nsRequest = nullptr;
}

#endif // USE(CFNETWORK)

} // namespace WebCore

#endif // PLATFORM(IOS)
