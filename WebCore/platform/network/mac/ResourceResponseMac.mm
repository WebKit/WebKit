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
#import "ResourceResponse.h"

#import "WebCoreURLResponse.h"
#import <Foundation/Foundation.h>
#import <wtf/StdLibExtras.h>
#import <limits>

@interface NSURLResponse (FoundationSecretsWebCoreKnowsAbout)
- (NSTimeInterval)_calculatedExpiration;
@end

#ifdef BUILDING_ON_TIGER
typedef int NSInteger;
#endif

namespace WebCore {

NSURLResponse *ResourceResponse::nsURLResponse() const
{
    if (!m_nsResponse && !m_isNull) {
        // Work around a mistake in the NSURLResponse class.
        // The init function takes an NSInteger, even though the accessor returns a long long.
        // For values that won't fit in an NSInteger, pass -1 instead.
        NSInteger expectedContentLength;
        if (m_expectedContentLength < 0 || m_expectedContentLength > std::numeric_limits<NSInteger>::max())
            expectedContentLength = -1;
        else
            expectedContentLength = static_cast<NSInteger>(m_expectedContentLength);
        const_cast<ResourceResponse*>(this)->m_nsResponse.adoptNS([[NSURLResponse alloc] initWithURL:m_url MIMEType:m_mimeType expectedContentLength:expectedContentLength textEncodingName:m_textEncodingName]);
    }
    return m_nsResponse.get();
}

void ResourceResponse::platformLazyInit()
{
    if (m_isUpToDate)
        return;
    m_isUpToDate = true;

    if (m_isNull) {
        ASSERT(!m_nsResponse);
        return;
    }
    
    m_url = [m_nsResponse.get() URL];
    m_mimeType = [m_nsResponse.get() _webcore_MIMEType];
    m_expectedContentLength = [m_nsResponse.get() expectedContentLength];
    m_textEncodingName = [m_nsResponse.get() textEncodingName];
    m_suggestedFilename = [m_nsResponse.get() suggestedFilename];
    
    const time_t maxTime = std::numeric_limits<time_t>::max();
    
    NSTimeInterval expiration = [m_nsResponse.get() _calculatedExpiration];
    expiration += kCFAbsoluteTimeIntervalSince1970;
    m_expirationDate = expiration > maxTime ? maxTime : static_cast<time_t>(expiration);
    
    if ([m_nsResponse.get() isKindOfClass:[NSHTTPURLResponse class]]) {
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)m_nsResponse.get();
        
        m_httpStatusCode = [httpResponse statusCode];
        
        // FIXME: it would be nice to have a way to get the real status text eventually.
        m_httpStatusText = "OK";
        
        NSDictionary *headers = [httpResponse allHeaderFields];
        NSEnumerator *e = [headers keyEnumerator];
        while (NSString *name = [e nextObject])
            m_httpHeaderFields.set(name, [headers objectForKey:name]);
    } else {
        m_httpStatusCode = 0;

#ifndef BUILDING_ON_TIGER
        // FIXME: This is a work around for <rdar://problem/5230154> (-[NSURLConnection initWithRequest:delegate:] 
        // is returning incorrect MIME type for local .xhtml files) which is only required in Leopard.
        if (m_url.isLocalFile() && m_mimeType == "text/html") {
            const String& path = m_url.path();
            DEFINE_STATIC_LOCAL(const String, xhtmlExt, (".xhtml"));
            if (path.endsWith(xhtmlExt, false))
                m_mimeType = "application/xhtml+xml";
        }
#endif
    }
}

bool ResourceResponse::platformCompare(const ResourceResponse& a, const ResourceResponse& b)
{
    return a.nsURLResponse() == b.nsURLResponse();
}

}
