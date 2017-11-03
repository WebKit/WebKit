/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
#import "ResourceHandleInternal.h"

#if USE(CFURLCONNECTION)

#if USE(APPLE_INTERNAL_SDK)
#import <CFNetwork/CFSocketStreamPriv.h>
#import <Foundation/NSURLRequestPrivate.h>
#else
#import <Foundation/NSURLRequest.h>
@interface NSURLRequest (Details)
+ (BOOL)allowsAnyHTTPSCertificateForHost:(NSString *)host;
+ (NSArray*)allowsSpecificHTTPSCertificateForHost:(NSString *)host;
@end
#endif

extern "C" const CFStringRef _kCFStreamSSLTrustedLeafCertificates;

namespace WebCore {

CFMutableDictionaryRef ResourceHandle::createSSLPropertiesFromNSURLRequest(const ResourceRequest& request)
{
    NSString *host = request.url().host();
    NSArray *certificateChain = [NSURLRequest allowsSpecificHTTPSCertificateForHost:host];
    BOOL allowsAnyCertificate = [NSURLRequest allowsAnyHTTPSCertificateForHost:host];
    if (!certificateChain && !allowsAnyCertificate)
        return nullptr;

    CFMutableDictionaryRef sslProperties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (certificateChain)
        CFDictionarySetValue(sslProperties, _kCFStreamSSLTrustedLeafCertificates, reinterpret_cast<CFTypeRef>(certificateChain));

    if (allowsAnyCertificate)
        CFDictionarySetValue(sslProperties, kCFStreamSSLValidatesCertificateChain, kCFBooleanFalse);
    return sslProperties;
}

}

#endif
