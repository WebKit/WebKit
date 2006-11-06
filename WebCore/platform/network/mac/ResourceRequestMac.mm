// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "ResourceRequestMac.h"

#import "FormDataStreamMac.h"
#import "ResourceRequest.h"

#import <Foundation/Foundation.h>

namespace WebCore {
    
    void getResourceRequest(ResourceRequest& request, NSURLRequest* nsRequest)
    {
        request = ResourceRequest([nsRequest URL]);

        request.setCachePolicy((ResourceRequestCachePolicy)[nsRequest cachePolicy]);
        request.setTimeoutInterval([nsRequest timeoutInterval]);
        request.setMainDocumentURL(KURL([nsRequest mainDocumentURL]));
        if (NSString* method = [nsRequest HTTPMethod])
            request.setHTTPMethod(method);
        request.setAllowHTTPCookies([nsRequest HTTPShouldHandleCookies]);

        NSDictionary *headers = [nsRequest allHTTPHeaderFields];
        NSEnumerator *e = [headers keyEnumerator];
        NSString *name;
        while ((name = [e nextObject]))
            request.setHTTPHeaderField(name, [headers objectForKey:name]);

        if (NSData* bodyData = [nsRequest HTTPBody])
            request.setHTTPBody(FormData([bodyData bytes], [bodyData length]));
        else if (NSInputStream* bodyStream = [nsRequest HTTPBodyStream])
            if (const FormData* formData = httpBodyFromStream(bodyStream))
                request.setHTTPBody(*formData);
        // FIXME: what to do about arbitrary body streams?
    }

    NSURLRequest* nsURLRequest(const ResourceRequest& request)
    {
        NSMutableURLRequest* nsRequest = [[NSMutableURLRequest alloc] initWithURL:request.url().getNSURL()];

        [nsRequest setCachePolicy:(NSURLRequestCachePolicy)request.cachePolicy()];
        [nsRequest setTimeoutInterval:request.timeoutInterval()];
        [nsRequest setMainDocumentURL:request.mainDocumentURL().getNSURL()];
        if (!request.httpMethod().isEmpty())
            [nsRequest setHTTPMethod:request.httpMethod()];
        [nsRequest setHTTPShouldHandleCookies:request.allowHTTPCookies()];

        HTTPHeaderMap::const_iterator end = request.httpHeaderFields().end();
        for (HTTPHeaderMap::const_iterator it = request.httpHeaderFields().begin(); it != end; ++it)
            [nsRequest setValue:it->second forHTTPHeaderField:it->first];

        if (!request.httpBody().isEmpty())
            setHTTPBody(nsRequest, request.httpBody());

        return [nsRequest autorelease];
    }

}
