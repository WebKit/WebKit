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

#if !USE(CFNETWORK)

#import "HTTPParsers.h"
#import "WebCoreURLResponse.h"
#import "WebCoreSystemInterface.h"
#import <Foundation/Foundation.h>
#import <wtf/StdLibExtras.h>
#import <limits>
#include <wtf/text/CString.h>

@interface NSURLResponse (FoundationSecretsWebCoreKnowsAbout)
- (NSTimeInterval)_calculatedExpiration;
@end


namespace WebCore {

static NSString* const commonHeaderFields[] = {
    @"Age", @"Cache-Control", @"Content-Type", @"Date", @"Etag", @"Expires", @"Last-Modified", @"Pragma"
};
static const int numCommonHeaderFields = sizeof(commonHeaderFields) / sizeof(AtomicString*);

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

void ResourceResponse::platformLazyInit(InitLevel initLevel)
{
    if (m_initLevel >= initLevel)
        return;

    if (m_isNull || !m_nsResponse)
        return;
    
    if (m_initLevel < CommonFieldsOnly && initLevel >= CommonFieldsOnly) {
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

        m_httpHeaderFields.clear();
        m_url = [m_nsResponse.get() URL];
        m_mimeType = [m_nsResponse.get() MIMEType];
        m_expectedContentLength = [m_nsResponse.get() expectedContentLength];
        m_textEncodingName = [m_nsResponse.get() textEncodingName];

        // Workaround for <rdar://problem/8757088>, can be removed once that is fixed.
        unsigned textEncodingNameLength = m_textEncodingName.length();
        if (textEncodingNameLength >= 2 && m_textEncodingName[0U] == '"' && m_textEncodingName[textEncodingNameLength - 1] == '"')
            m_textEncodingName = m_textEncodingName.substring(1, textEncodingNameLength - 2);

        if ([m_nsResponse.get() isKindOfClass:[NSHTTPURLResponse class]]) {
            NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)m_nsResponse.get();

            m_httpStatusCode = [httpResponse statusCode];

            NSDictionary *headers = [httpResponse allHeaderFields];
            
            for (int i = 0; i < numCommonHeaderFields; i++) {
                if (NSString* headerValue = [headers objectForKey:commonHeaderFields[i]])
                    m_httpHeaderFields.set([commonHeaderFields[i] UTF8String], headerValue);
            }
        } else
            m_httpStatusCode = 0;
        
        [pool drain];
    }

    if (m_initLevel < CommonAndUncommonFields && initLevel >= CommonAndUncommonFields) {
        if ([m_nsResponse.get() isKindOfClass:[NSHTTPURLResponse class]]) {
            NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

            NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)m_nsResponse.get();

            RetainPtr<NSString> httpStatusLine(AdoptNS, wkCopyNSURLResponseStatusLine(m_nsResponse.get()));
            if (httpStatusLine)
                m_httpStatusText = extractReasonPhraseFromHTTPStatusLine(httpStatusLine.get());
            else
                m_httpStatusText = "OK";

            NSDictionary *headers = [httpResponse allHeaderFields];
            NSEnumerator *e = [headers keyEnumerator];
            while (NSString *name = [e nextObject])
                m_httpHeaderFields.set(name, [headers objectForKey:name]);
            
            [pool drain];
        }
    }
     
    if (m_initLevel < AllFields && initLevel >= AllFields)
        m_suggestedFilename = [m_nsResponse.get() suggestedFilename];

    m_initLevel = initLevel;
}

    
bool ResourceResponse::platformCompare(const ResourceResponse& a, const ResourceResponse& b)
{
    return a.nsURLResponse() == b.nsURLResponse();
}

} // namespace WebCore

#endif // !USE(CFNETWORK)
