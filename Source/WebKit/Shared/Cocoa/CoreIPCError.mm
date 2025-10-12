/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "CoreIPCError.h"

#import <wtf/TZoneMallocInlines.h>
#import <wtf/URL.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CoreIPCError);

bool CoreIPCError::hasValidUserInfo(const RetainPtr<CFDictionaryRef>& userInfo)
{
    NSDictionary * info = bridge_cast(userInfo.get());

    if (RetainPtr<id> object = [info objectForKey:@"NSErrorClientCertificateChainKey"]) {
        if (![object isKindOfClass:[NSArray class]])
            return false;
        for (id certificate in object.get()) {
            if ((CFGetTypeID((__bridge CFTypeRef)certificate) != SecCertificateGetTypeID()))
                return false;
        }
    }

    if (RetainPtr<id> peerCertificateChain = [info objectForKey:@"NSErrorPeerCertificateChainKey"]) {
        for (id object in peerCertificateChain.get()) {
            if (CFGetTypeID((__bridge CFTypeRef)object) != SecCertificateGetTypeID())
                return false;
        }
    }

    if (RetainPtr peerTrust = (__bridge SecTrustRef)[info objectForKey:NSURLErrorFailingURLPeerTrustErrorKey]) {
        if (CFGetTypeID((__bridge CFTypeRef)peerTrust.get()) != SecTrustGetTypeID())
            return false;
    }

    if (RetainPtr<id> underlyingError = [info objectForKey:NSUnderlyingErrorKey]) {
        if (![underlyingError isKindOfClass:[NSError class]])
            return false;
    }

    return true;
}

RetainPtr<id> CoreIPCError::toID() const
{
    if (m_underlyingError) {
        auto underlyingNSError = m_underlyingError->toID();
        if (!underlyingNSError)
            return nil;

        RetainPtr<CFMutableDictionaryRef> mutableUserInfo;
        if (!m_userInfo)
            mutableUserInfo = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL));
        else
            mutableUserInfo = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(m_userInfo.get()) + 1, m_userInfo.get()));
        CFDictionarySetValue(mutableUserInfo.get(), (__bridge CFStringRef)NSUnderlyingErrorKey, (__bridge CFTypeRef)underlyingNSError.get());
        return adoptNS([[NSError alloc] initWithDomain:m_domain.createNSString().get() code:m_code userInfo:(__bridge NSDictionary *)mutableUserInfo.get()]);
    }
    return adoptNS([[NSError alloc] initWithDomain:m_domain.createNSString().get() code:m_code userInfo:(__bridge NSDictionary *)m_userInfo.get()]);
}

bool CoreIPCError::isSafeToEncodeUserInfo(id value)
{
    if ([value isKindOfClass:NSString.class] || [value isKindOfClass:NSURL.class] || [value isKindOfClass:NSNumber.class])
        return true;

    if (auto array = dynamic_objc_cast<NSArray>(value)) {
        for (id object in array) {
            if (!isSafeToEncodeUserInfo(object))
                return false;
        }
        return true;
    }

    if (auto dictionary = dynamic_objc_cast<NSDictionary>(value)) {
        for (id innerValue in dictionary.objectEnumerator) {
            if (!isSafeToEncodeUserInfo(innerValue))
                return false;
        }
        return true;
    }

    return false;
}

CoreIPCError::CoreIPCError(NSError *nsError)
    : m_domain([nsError domain])
    , m_code([nsError code])
{
    RetainPtr<NSDictionary> userInfo = [nsError userInfo];

    RetainPtr<CFMutableDictionaryRef> filteredUserInfo = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, [userInfo count], &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    [userInfo enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL*) {
        if ([key isEqualToString:@"NSErrorClientCertificateChainKey"]) {
            if (![value isKindOfClass:[NSArray class]])
                return;
        }
        if (isSafeToEncodeUserInfo(value))
            CFDictionarySetValue(filteredUserInfo.get(), (__bridge CFTypeRef)key, (__bridge CFTypeRef)value);
    }];

    if (RetainPtr<NSArray> clientIdentityAndCertificates = [userInfo objectForKey:@"NSErrorClientCertificateChainKey"]) {
        if ([clientIdentityAndCertificates isKindOfClass:[NSArray class]]) {
            // Turn SecIdentity members into SecCertificate to strip out private key information.
            RetainPtr<id> clientCertificates = [NSMutableArray arrayWithCapacity:clientIdentityAndCertificates.get().count];
            for (id object in clientIdentityAndCertificates.get()) {
                // Only SecIdentity or SecCertificate types are expected in clientIdentityAndCertificates
                if (CFGetTypeID((__bridge CFTypeRef)object) != SecIdentityGetTypeID() && CFGetTypeID((__bridge CFTypeRef)object) != SecCertificateGetTypeID())
                    continue;
                if (CFGetTypeID((__bridge CFTypeRef)object) != SecIdentityGetTypeID()) {
                    [clientCertificates addObject:object];
                    continue;
                }
                SecCertificateRef certificate = nil;
                OSStatus status = SecIdentityCopyCertificate((SecIdentityRef)object, &certificate);
                RetainPtr<SecCertificateRef> retainCertificate = adoptCF(certificate);
                // The SecIdentity member is the key information of this attribute. Without it, we should nil
                // the attribute.
                if (status != errSecSuccess) {
                    LOG_ERROR("Failed to encode nsError.userInfo[NSErrorClientCertificateChainKey]: %d", status);
                    clientCertificates = nil;
                    break;
                }
                [clientCertificates addObject:(__bridge id)certificate];
            }
            CFDictionarySetValue(filteredUserInfo.get(), CFSTR("NSErrorClientCertificateChainKey"), (__bridge CFTypeRef)clientCertificates.get());
        }
    }

    RetainPtr<id> peerCertificateChain = [userInfo objectForKey:@"NSErrorPeerCertificateChainKey"];
    if (!peerCertificateChain) {
        if (RetainPtr<id> candidatePeerTrust = [userInfo objectForKey:NSURLErrorFailingURLPeerTrustErrorKey]) {
            if (CFGetTypeID((__bridge CFTypeRef)candidatePeerTrust.get()) == SecTrustGetTypeID())
                peerCertificateChain = (__bridge NSArray *)adoptCF(SecTrustCopyCertificateChain((__bridge SecTrustRef)candidatePeerTrust.get())).get();
        }
    }

    if (peerCertificateChain && [peerCertificateChain isKindOfClass:[NSArray class]]) {
        bool hasExpectedTypes = true;
        for (id object in peerCertificateChain.get()) {
            if (CFGetTypeID((__bridge CFTypeRef)object) != SecCertificateGetTypeID()) {
                hasExpectedTypes = false;
                break;
            }
        }
        if (hasExpectedTypes)
            CFDictionarySetValue(filteredUserInfo.get(), CFSTR("NSErrorPeerCertificateChainKey"), (__bridge CFTypeRef)peerCertificateChain.get());
    }

    if (RetainPtr peerTrust = (__bridge SecTrustRef)[userInfo objectForKey:NSURLErrorFailingURLPeerTrustErrorKey]) {
        if (CFGetTypeID((__bridge CFTypeRef)peerTrust.get()) == SecTrustGetTypeID())
            CFDictionarySetValue(filteredUserInfo.get(), (__bridge CFStringRef)NSURLErrorFailingURLPeerTrustErrorKey, peerTrust.get());
    }

    m_userInfo = static_cast<CFDictionaryRef>(filteredUserInfo.get());

    if (RetainPtr<id> underlyingError = [userInfo objectForKey:NSUnderlyingErrorKey]) {
        if ([underlyingError isKindOfClass:[NSError class]])
            m_underlyingError = makeUnique<CoreIPCError>(underlyingError.get());
    }
}

} // namespace WebKit
