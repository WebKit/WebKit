/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
#import "ResourceError.h"

#import "BlockExceptions.h"
#import "URL.h"
#import <CoreFoundation/CFError.h>
#import <Foundation/Foundation.h>

#if PLATFORM(IOS)
#import <CFNetwork/CFSocketStreamPriv.h>
#import <Foundation/NSURLError.h>
#endif

@interface NSError (WebExtras)
- (NSString *)_web_localizedDescription;
@end

#if PLATFORM(IOS)

// This workaround code exists here because we can't call translateToCFError in Foundation. Once we
// have that, we can remove this code. <rdar://problem/9837415> Need SPI for translateCFError
// The code is mostly identical to Foundation - I changed the class name and fixed minor compile errors.
// We need this because client code (Safari) wants an NSError with NSURLErrorDomain as its domain.
// The Foundation code below does that and sets up appropriate certificate keys in the NSError.

@interface WebCustomNSURLError : NSError

@end

@implementation WebCustomNSURLError

static NSDictionary* dictionaryThatCanCode(NSDictionary* src)
{
    // This function makes a copy of input dictionary, modifies it such that it "should" (as much as we can help it)
    // not contain any objects that do not conform to NSCoding protocol, and returns it autoreleased.

    NSMutableDictionary* dst = [src mutableCopy];

    // Kill the known problem entries.
    [dst removeObjectForKey:@"NSErrorPeerCertificateChainKey"]; // NSArray with SecCertificateRef objects
    [dst removeObjectForKey:@"NSErrorClientCertificateChainKey"]; // NSArray with SecCertificateRef objects
    [dst removeObjectForKey:NSURLErrorFailingURLPeerTrustErrorKey]; // SecTrustRef object
    [dst removeObjectForKey:NSUnderlyingErrorKey]; // (Immutable) CFError containing kCF equivalent of the above
    // We could reconstitute this but it's more trouble than it's worth

    // Non-comprehensive safety check:  Kill top-level dictionary entries that don't conform to NSCoding.
    // We may hit ones we just removed, but that's fine.
    // We don't handle arbitrary objects that clients have stuffed into the dictionary, since we may not know how to
    // get at its conents (e.g., a CFError object -- you'd have to know it had a userInfo dictionary and kill things
    // inside it).
    [src enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL*) {
        if (! [obj conformsToProtocol:@protocol(NSCoding)]) {
            [dst removeObjectForKey:key];
        }
        // FIXME: We could drill down into subdictionaries, but it seems more trouble than it's worth
    }];

    return [dst autorelease];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    NSDictionary* newUserInfo = dictionaryThatCanCode([self userInfo]);

    [[NSError errorWithDomain:[self domain] code:[self code] userInfo:newUserInfo] encodeWithCoder:coder];
}

@end


#if USE(CFNETWORK)
static NSError *NSErrorFromCFError(CFErrorRef cfError, NSURL *url)
{
    CFIndex errCode = CFErrorGetCode(cfError);
    if (CFEqual(CFErrorGetDomain(cfError), kCFErrorDomainCFNetwork) && errCode <= kCFURLErrorUnknown && errCode > -4000) {
        // This is an URL error and needs to be translated to the NSURLErrorDomain
        id keys[10], values[10];
        CFDictionaryRef userInfo = CFErrorCopyUserInfo(cfError);
        NSError *result;
        NSInteger count = 0;

        if (url) {
            keys[count] = NSURLErrorFailingURLErrorKey;
            values[count] = url;
            count ++;

            keys[count] = NSURLErrorFailingURLStringErrorKey;
            values[count] = [url absoluteString];
            count ++;
        }

        values[count] = (id)CFDictionaryGetValue(userInfo, kCFErrorLocalizedDescriptionKey);
        if (values[count]) {
            keys[count] = NSLocalizedDescriptionKey;
            count ++;
        }

        values[count] = (id)CFDictionaryGetValue(userInfo, kCFErrorLocalizedFailureReasonKey);
        if (values[count]) {
            keys[count] = NSLocalizedFailureReasonErrorKey;
            count ++;
        }

        values[count] = (id)CFDictionaryGetValue(userInfo, kCFErrorLocalizedRecoverySuggestionKey);
        if (values[count]) {
            keys[count] = NSLocalizedRecoverySuggestionErrorKey;
            count ++;
        }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        if (userInfo && (values[count] = (id)CFDictionaryGetValue(userInfo, kCFStreamPropertySSLPeerCertificates)) != nil) {
            // Need to translate the cert
            keys[count] = @"NSErrorPeerCertificateChainKey";
            count ++;

            values[count] = (id)CFDictionaryGetValue(userInfo, _kCFStreamPropertySSLClientCertificates);
            if (values[count]) {
                keys[count] = @"NSErrorClientCertificateChainKey";
                count ++;
            }

            values[count] = (id)CFDictionaryGetValue(userInfo, _kCFStreamPropertySSLClientCertificateState);
            if (values[count]) {
                keys[count] = @"NSErrorClientCertificateStateKey";
                count ++;
            }
        }
#pragma clang diagnostic pop

        if (userInfo && (values[count] = (id)CFDictionaryGetValue(userInfo, kCFStreamPropertySSLPeerTrust)) != nil) {
            keys[count] = NSURLErrorFailingURLPeerTrustErrorKey;
            count++;
        }

        keys[count] = NSUnderlyingErrorKey;
        values[count] = (id)cfError;
        count ++;

        result = [WebCustomNSURLError errorWithDomain:NSURLErrorDomain code:(errCode == kCFURLErrorUnknown ? (CFIndex)NSURLErrorUnknown : errCode) userInfo:[NSDictionary dictionaryWithObjects:values forKeys:keys count:count]];
        if (userInfo)
            CFRelease(userInfo);
        return result;
    }
    return (NSError *)cfError;
}
#endif // USE(CFNETWORK)

#endif // PLATFORM(IOS)

namespace WebCore {

static RetainPtr<NSError> createNSErrorFromResourceErrorBase(const ResourceErrorBase& resourceError)
{
    RetainPtr<NSMutableDictionary> userInfo = adoptNS([[NSMutableDictionary alloc] init]);

    if (!resourceError.localizedDescription().isEmpty())
        [userInfo.get() setValue:resourceError.localizedDescription() forKey:NSLocalizedDescriptionKey];

    if (!resourceError.failingURL().isEmpty()) {
        RetainPtr<NSURL> cocoaURL = adoptNS([[NSURL alloc] initWithString:resourceError.failingURL()]);
        [userInfo.get() setValue:resourceError.failingURL() forKey:@"NSErrorFailingURLStringKey"];
        [userInfo.get() setValue:cocoaURL.get() forKey:@"NSErrorFailingURLKey"];
    }

    return adoptNS([[NSError alloc] initWithDomain:resourceError.domain() code:resourceError.errorCode() userInfo:userInfo.get()]);
}

#if USE(CFNETWORK)

ResourceError::ResourceError(NSError *error)
    : m_dataIsUpToDate(false)
    , m_platformError(reinterpret_cast<CFErrorRef>(error))
{
    m_isNull = !error;
    if (!m_isNull)
        m_isTimeout = [error code] == NSURLErrorTimedOut;
}

NSError *ResourceError::nsError() const
{
    if (m_isNull) {
        ASSERT(!m_platformError);
        return nil;
    }

    if (m_platformNSError)
        return m_platformNSError.get();

    if (m_platformError) {
        CFErrorRef error = m_platformError.get();
        RetainPtr<CFDictionaryRef> userInfo = adoptCF(CFErrorCopyUserInfo(error));
#if PLATFORM(IOS)
        m_platformNSError = NSErrorFromCFError(error, (NSURL *)[(NSDictionary *)userInfo.get() objectForKey:(id) kCFURLErrorFailingURLErrorKey]);
        // If NSErrorFromCFError created a new NSError for us, return that.
        if (m_platformNSError.get() != (NSError *)error)
            return m_platformNSError.get();

        // Otherwise fall through to create a new NSError from the CFError.
#endif
        m_platformNSError = adoptNS([[NSError alloc] initWithDomain:(NSString *)CFErrorGetDomain(error) code:CFErrorGetCode(error) userInfo:(NSDictionary *)userInfo.get()]);
        return m_platformNSError.get();
    }

    m_platformNSError = createNSErrorFromResourceErrorBase(*this);
    return m_platformNSError.get();
}

ResourceError::operator NSError *() const
{
    return nsError();
}

#else

ResourceError::ResourceError(NSError *nsError)
    : m_dataIsUpToDate(false)
    , m_platformError(nsError)
{
    m_isNull = !nsError;
    if (!m_isNull)
        m_isTimeout = [m_platformError.get() code] == NSURLErrorTimedOut;
}

ResourceError::ResourceError(CFErrorRef cfError)
    : m_dataIsUpToDate(false)
    , m_platformError((NSError *)cfError)
{
    m_isNull = !cfError;
    if (!m_isNull)
        m_isTimeout = [m_platformError.get() code] == NSURLErrorTimedOut;
}

void ResourceError::platformLazyInit()
{
    if (m_dataIsUpToDate)
        return;

    m_domain = [m_platformError.get() domain];
    m_errorCode = [m_platformError.get() code];

    NSString* failingURLString = [[m_platformError.get() userInfo] valueForKey:@"NSErrorFailingURLStringKey"];
    if (!failingURLString)
        failingURLString = [[[m_platformError.get() userInfo] valueForKey:@"NSErrorFailingURLKey"] absoluteString];
    m_failingURL = failingURLString; 
    // Workaround for <rdar://problem/6554067>
    m_localizedDescription = m_failingURL;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    m_localizedDescription = [m_platformError.get() _web_localizedDescription];
    END_BLOCK_OBJC_EXCEPTIONS;

    m_dataIsUpToDate = true;
}

bool ResourceError::platformCompare(const ResourceError& a, const ResourceError& b)
{
    return a.nsError() == b.nsError();
}

NSError *ResourceError::nsError() const
{
    if (m_isNull) {
        ASSERT(!m_platformError);
        return nil;
    }

    if (!m_platformError)
        m_platformError = createNSErrorFromResourceErrorBase(*this);;

    return m_platformError.get();
}

ResourceError::operator NSError *() const
{
    return nsError();
}

CFErrorRef ResourceError::cfError() const
{
    return (CFErrorRef)nsError();
}

ResourceError::operator CFErrorRef() const
{
    return cfError();
}

#endif // USE(CFNETWORK)

} // namespace WebCore
