/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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
#import "ResourceResponse.h"

#if PLATFORM(COCOA)

#import "CFNetworkSPI.h"
#import "HTTPParsers.h"
#import "WebCoreURLResponse.h"
#import <Foundation/Foundation.h>
#import <limits>
#import <wtf/StdLibExtras.h>

namespace WebCore {

void ResourceResponse::initNSURLResponse() const
{
    if (!m_httpStatusCode || !m_url.protocolIsInHTTPFamily()) {
        // Work around a mistake in the NSURLResponse class - <rdar://problem/6875219>.
        // The init function takes an NSInteger, even though the accessor returns a long long.
        // For values that won't fit in an NSInteger, pass -1 instead.
        NSInteger expectedContentLength;
        if (m_expectedContentLength < 0 || m_expectedContentLength > std::numeric_limits<NSInteger>::max())
            expectedContentLength = -1;
        else
            expectedContentLength = static_cast<NSInteger>(m_expectedContentLength);

        NSString* encodingNSString = nsStringNilIfEmpty(m_textEncodingName);
        m_nsResponse = adoptNS([[NSURLResponse alloc] initWithURL:m_url MIMEType:m_mimeType expectedContentLength:expectedContentLength textEncodingName:encodingNSString]);
        return;
    }

    // FIXME: We lose the status text and the HTTP version here.
    NSMutableDictionary* headerDictionary = [NSMutableDictionary dictionary];
    for (auto& header : m_httpHeaderFields)
        [headerDictionary setObject:(NSString *)header.value forKey:(NSString *)header.key];

    m_nsResponse = adoptNS([[NSHTTPURLResponse alloc] initWithURL:m_url statusCode:m_httpStatusCode HTTPVersion:(NSString*)kCFHTTPVersion1_1 headerFields:headerDictionary]);

    // Mime type sniffing doesn't work with a synthesized response.
    [m_nsResponse.get() _setMIMEType:(NSString *)m_mimeType];
}

CertificateInfo ResourceResponse::platformCertificateInfo() const
{
    ASSERT(m_nsResponse);
    auto cfResponse = [m_nsResponse _CFURLResponse];
    if (!cfResponse)
        return { };

    CFDictionaryRef context = _CFURLResponseGetSSLCertificateContext(cfResponse);
    if (!context)
        return { };

    auto trustValue = CFDictionaryGetValue(context, kCFStreamPropertySSLPeerTrust);
    if (!trustValue)
        return { };
    ASSERT(CFGetTypeID(trustValue) == SecTrustGetTypeID());
    auto trust = (SecTrustRef)trustValue;

    SecTrustResultType trustResultType;
    OSStatus result = SecTrustGetTrustResult(trust, &trustResultType);
    if (result != errSecSuccess)
        return { };

    if (trustResultType == kSecTrustResultInvalid) {
        result = SecTrustEvaluate(trust, &trustResultType);
        if (result != errSecSuccess)
            return { };
    }

    CFIndex count = SecTrustGetCertificateCount(trust);
    auto certificateChain = CFArrayCreateMutable(0, count, &kCFTypeArrayCallBacks);
    for (CFIndex i = 0; i < count; i++)
        CFArrayAppendValue(certificateChain, SecTrustGetCertificateAtIndex(trust, i));

    return CertificateInfo(adoptCF(certificateChain));
}

#if USE(CFNETWORK)

NSURLResponse *ResourceResponse::nsURLResponse() const
{
    if (!m_nsResponse && !m_cfResponse && !m_isNull) {
        initNSURLResponse();
        m_cfResponse = [m_nsResponse.get() _CFURLResponse];
        return m_nsResponse.get();
    }

    if (!m_cfResponse)
        return nil;

    if (!m_nsResponse)
        m_nsResponse = [NSURLResponse _responseWithCFURLResponse:m_cfResponse.get()];

    return m_nsResponse.get();
}

ResourceResponse::ResourceResponse(NSURLResponse* nsResponse)
    : m_initLevel(Uninitialized)
    , m_platformResponseIsUpToDate(true)
    , m_cfResponse([nsResponse _CFURLResponse])
    , m_nsResponse(nsResponse)
{
    m_isNull = !nsResponse;
}

#else

static NSString* const commonHeaderFields[] = {
    @"Age", @"Cache-Control", @"Content-Type", @"Date", @"Etag", @"Expires", @"Last-Modified", @"Pragma"
};

NSURLResponse *ResourceResponse::nsURLResponse() const
{
    if (!m_nsResponse && !m_isNull)
        initNSURLResponse();
    return m_nsResponse.get();
}
    
static NSString *copyNSURLResponseStatusLine(NSURLResponse *response)
{
    CFURLResponseRef cfResponse = [response _CFURLResponse];
    if (!cfResponse)
        return nil;

    CFHTTPMessageRef cfHTTPMessage = CFURLResponseGetHTTPResponse(cfResponse);
    if (!cfHTTPMessage)
        return nil;

    return (NSString *)CFHTTPMessageCopyResponseStatusLine(cfHTTPMessage);
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
            m_textEncodingName = m_textEncodingName.string().substring(1, textEncodingNameLength - 2);

        if ([m_nsResponse.get() isKindOfClass:[NSHTTPURLResponse class]]) {
            NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)m_nsResponse.get();

            m_httpStatusCode = [httpResponse statusCode];
            
            if (initLevel < AllFields) {
                NSDictionary *headers = [httpResponse allHeaderFields];
                for (NSString *name : commonHeaderFields) {
                    if (NSString* headerValue = [headers objectForKey:name])
                        m_httpHeaderFields.set(name, headerValue);
                }
            }
        } else
            m_httpStatusCode = 0;
        
        [pool drain];
    }

    if (m_initLevel < AllFields && initLevel == AllFields) {
        if ([m_nsResponse.get() isKindOfClass:[NSHTTPURLResponse class]]) {
            NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

            NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)m_nsResponse.get();
            if (RetainPtr<NSString> httpStatusLine = adoptNS(copyNSURLResponseStatusLine(httpResponse)))
                m_httpStatusText = extractReasonPhraseFromHTTPStatusLine(httpStatusLine.get());
            else
                m_httpStatusText = AtomicString("OK", AtomicString::ConstructFromLiteral);

            NSDictionary *headers = [httpResponse allHeaderFields];
            for (NSString *name in headers)
                m_httpHeaderFields.set(name, [headers objectForKey:name]);
            
            [pool drain];
        }
    }

    m_initLevel = initLevel;
}

String ResourceResponse::platformSuggestedFilename() const
{
    return [nsURLResponse() suggestedFilename];
}

bool ResourceResponse::platformCompare(const ResourceResponse& a, const ResourceResponse& b)
{
    return a.nsURLResponse() == b.nsURLResponse();
}

#endif // USE(CFNETWORK)

} // namespace WebCore

#endif // PLATFORM(COCOA)
