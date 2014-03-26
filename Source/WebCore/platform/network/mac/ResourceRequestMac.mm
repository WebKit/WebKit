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

#import "FormDataStreamMac.h"
#import "ResourceRequestCFNet.h"
#import "RuntimeApplicationChecks.h"
#import "WebCoreSystemInterface.h"
#import <wtf/text/CString.h>

#import <Foundation/Foundation.h>


@interface NSURLRequest (WebNSURLRequestDetails)
- (NSArray *)contentDispositionEncodingFallbackArray;
+ (void)setDefaultTimeoutInterval:(NSTimeInterval)seconds;
- (CFURLRequestRef)_CFURLRequest;
- (id)_initWithCFURLRequest:(CFURLRequestRef)request;
@end

@interface NSMutableURLRequest (WebMutableNSURLRequestDetails)
- (void)setContentDispositionEncodingFallbackArray:(NSArray *)theEncodingFallbackArray;
@end

namespace WebCore {

NSURLRequest *ResourceRequest::nsURLRequest(HTTPBodyUpdatePolicy bodyPolicy) const
{
    updatePlatformRequest(bodyPolicy);

    return [[m_nsRequest.get() retain] autorelease];
}

#if USE(CFNETWORK)

ResourceRequest::ResourceRequest(NSURLRequest *nsRequest)
    : ResourceRequestBase()
#if PLATFORM(IOS)
    , m_mainResourceRequest(false)
#endif
    , m_cfRequest([nsRequest _CFURLRequest])
    , m_nsRequest(nsRequest)
{
}

void ResourceRequest::updateNSURLRequest()
{
#if PLATFORM(IOS)
    // There is client code that extends NSURLRequest and expects to get back, in the delegate
    // callbacks, an object of the same type that they passed into WebKit. To keep then running, we
    // create an object of the same type and return that. See <rdar://9843582>.
    // Also, developers really really want an NSMutableURLRequest so try to create an
    // NSMutableURLRequest instead of NSURLRequest.
    static Class nsURLRequestClass = [NSURLRequest class];
    static Class nsMutableURLRequestClass = [NSMutableURLRequest class];
    Class requestClass = [m_nsRequest.get() class];

    if (!requestClass || requestClass == nsURLRequestClass)
        requestClass = nsMutableURLRequestClass;

    if (m_cfRequest)
        m_nsRequest = adoptNS([[requestClass alloc] _initWithCFURLRequest:m_cfRequest.get()]);
#else
    if (m_cfRequest)
        m_nsRequest = adoptNS([[NSURLRequest alloc] _initWithCFURLRequest:m_cfRequest.get()]);
#endif
}

#else

CFURLRequestRef ResourceRequest::cfURLRequest(HTTPBodyUpdatePolicy bodyPolicy) const
{
    return [nsURLRequest(bodyPolicy) _CFURLRequest];
}

void ResourceRequest::doUpdateResourceRequest()
{
    m_url = [m_nsRequest.get() URL];
    m_cachePolicy = (ResourceRequestCachePolicy)[m_nsRequest.get() cachePolicy];
    m_timeoutInterval = [m_nsRequest.get() timeoutInterval];
    m_firstPartyForCookies = [m_nsRequest.get() mainDocumentURL];
    
    if (NSString* method = [m_nsRequest.get() HTTPMethod])
        m_httpMethod = method;
    m_allowCookies = [m_nsRequest.get() HTTPShouldHandleCookies];

    m_priority = toResourceLoadPriority(wkGetHTTPRequestPriority([m_nsRequest.get() _CFURLRequest]));

    NSDictionary *headers = [m_nsRequest.get() allHTTPHeaderFields];
    NSEnumerator *e = [headers keyEnumerator];
    NSString *name;
    m_httpHeaderFields.clear();
    while ((name = [e nextObject]))
        m_httpHeaderFields.set(name, [headers objectForKey:name]);

    m_responseContentDispositionEncodingFallbackArray.clear();
    NSArray *encodingFallbacks = [m_nsRequest.get() contentDispositionEncodingFallbackArray];
    NSUInteger count = [encodingFallbacks count];
    for (NSUInteger i = 0; i < count; ++i) {
        CFStringEncoding encoding = CFStringConvertNSStringEncodingToEncoding([(NSNumber *)[encodingFallbacks objectAtIndex:i] unsignedLongValue]);
        if (encoding != kCFStringEncodingInvalidId)
            m_responseContentDispositionEncodingFallbackArray.append(CFStringConvertEncodingToIANACharSetName(encoding));
    }

#if ENABLE(CACHE_PARTITIONING)
    if (m_nsRequest) {
        NSString* cachePartition = [NSURLProtocol propertyForKey:(NSString *)wkCachePartitionKey() inRequest:m_nsRequest.get()];
        if (cachePartition)
            m_cachePartition = cachePartition;
    }
#endif
}

void ResourceRequest::doUpdateResourceHTTPBody()
{
    if (NSData* bodyData = [m_nsRequest.get() HTTPBody])
        m_httpBody = FormData::create([bodyData bytes], [bodyData length]);
    else if (NSInputStream* bodyStream = [m_nsRequest.get() HTTPBodyStream]) {
        FormData* formData = httpBodyFromStream(bodyStream);
        // There is no FormData object if a client provided a custom data stream.
        // We shouldn't be looking at http body after client callbacks.
        ASSERT(formData);
        if (formData)
            m_httpBody = formData;
    }
}

void ResourceRequest::doUpdatePlatformRequest()
{
    if (isNull()) {
        m_nsRequest = nil;
        return;
    }
    
    NSMutableURLRequest *nsRequest = [m_nsRequest.get() mutableCopy];

    if (nsRequest)
        [nsRequest setURL:url()];
    else
        nsRequest = [[NSMutableURLRequest alloc] initWithURL:url()];

    if (ResourceRequest::httpPipeliningEnabled())
        wkHTTPRequestEnablePipelining([nsRequest _CFURLRequest]);

    wkSetHTTPRequestPriority([nsRequest _CFURLRequest], toPlatformRequestPriority(m_priority));

    [nsRequest setCachePolicy:(NSURLRequestCachePolicy)cachePolicy()];
    wkCFURLRequestAllowAllPostCaching([nsRequest _CFURLRequest]);

    double timeoutInterval = ResourceRequestBase::timeoutInterval();
    if (timeoutInterval)
        [nsRequest setTimeoutInterval:timeoutInterval];
    // Otherwise, respect NSURLRequest default timeout.

    [nsRequest setMainDocumentURL:firstPartyForCookies()];
    if (!httpMethod().isEmpty())
        [nsRequest setHTTPMethod:httpMethod()];
    [nsRequest setHTTPShouldHandleCookies:allowCookies()];

    // Cannot just use setAllHTTPHeaderFields here, because it does not remove headers.
    NSArray *oldHeaderFieldNames = [[nsRequest allHTTPHeaderFields] allKeys];
    for (unsigned i = [oldHeaderFieldNames count]; i != 0; --i)
        [nsRequest setValue:nil forHTTPHeaderField:[oldHeaderFieldNames objectAtIndex:i - 1]];
    for (const auto& header : httpHeaderFields())
        [nsRequest setValue:header.value forHTTPHeaderField:header.key];

    NSMutableArray *encodingFallbacks = [NSMutableArray array];
    unsigned count = m_responseContentDispositionEncodingFallbackArray.size();
    for (unsigned i = 0; i != count; ++i) {
        RetainPtr<CFStringRef> encodingName = m_responseContentDispositionEncodingFallbackArray[i].createCFString();
        unsigned long nsEncoding = CFStringConvertEncodingToNSStringEncoding(CFStringConvertIANACharSetNameToEncoding(encodingName.get()));

        if (nsEncoding != kCFStringEncodingInvalidId)
            [encodingFallbacks addObject:[NSNumber numberWithUnsignedLong:nsEncoding]];
    }
    [nsRequest setContentDispositionEncodingFallbackArray:encodingFallbacks];

#if ENABLE(CACHE_PARTITIONING)
    String partition = cachePartition();
    if (!partition.isNull() && !partition.isEmpty()) {
        NSString *partitionValue = [NSString stringWithUTF8String:partition.utf8().data()];
        [NSURLProtocol setProperty:partitionValue forKey:(NSString *)wkCachePartitionKey() inRequest:nsRequest];
    }
#endif

    m_nsRequest = adoptNS(nsRequest);
}

void ResourceRequest::doUpdatePlatformHTTPBody()
{
    if (isNull()) {
        ASSERT(!m_nsRequest);
        return;
    }

    NSMutableURLRequest *nsRequest = [m_nsRequest.get() mutableCopy];

    if (nsRequest)
        [nsRequest setURL:url()];
    else
        nsRequest = [[NSMutableURLRequest alloc] initWithURL:url()];

    RefPtr<FormData> formData = httpBody();
    if (formData && !formData->isEmpty())
        WebCore::setHTTPBody(nsRequest, formData);

    if (NSInputStream *bodyStream = [nsRequest HTTPBodyStream]) {
        // For streams, provide a Content-Length to avoid using chunked encoding, and to get accurate total length in callbacks.
        NSString *lengthString = [bodyStream propertyForKey:(NSString *)formDataStreamLengthPropertyName()];
        if (lengthString) {
            [nsRequest setValue:lengthString forHTTPHeaderField:@"Content-Length"];
            // Since resource request is already marked updated, we need to keep it up to date too.
            ASSERT(m_resourceRequestUpdated);
            m_httpHeaderFields.set("Content-Length", lengthString);
        }
    }

    m_nsRequest = adoptNS(nsRequest);
}

void ResourceRequest::updateFromDelegatePreservingOldProperties(const ResourceRequest& delegateProvidedRequest)
{
    RefPtr<FormData> oldHTTPBody = httpBody();
#if ENABLE(INSPECTOR)
    bool isHiddenFromInspector = hiddenFromInspector();
#endif

    *this = delegateProvidedRequest;

    setHTTPBody(oldHTTPBody.release());
#if ENABLE(INSPECTOR)
    setHiddenFromInspector(isHiddenFromInspector);
#endif
}

#if !PLATFORM(IOS)
void ResourceRequest::applyWebArchiveHackForMail()
{
    // Hack because Mail checks for this property to detect data / archive loads
    [NSURLProtocol setProperty:@"" forKey:@"WebDataRequest" inRequest:(NSMutableURLRequest *)nsURLRequest(DoNotUpdateHTTPBody)];
}
#endif

void ResourceRequest::setStorageSession(CFURLStorageSessionRef storageSession)
{
    updatePlatformRequest();
    m_nsRequest = adoptNS(wkCopyRequestWithStorageSession(storageSession, m_nsRequest.get()));
}
    
#endif // USE(CFNETWORK)

#if !PLATFORM(IOS)
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
#endif // !PLATFORM(IOS)

bool ResourceRequest::useQuickLookResourceCachingQuirks()
{
#if !PLATFORM(IOS)
    static bool flag = initQuickLookResourceCachingQuirks();
    return flag;
#else
    return false;
#endif // !PLATFORM(IOS)
}

} // namespace WebCore

