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

#define INJECT_VALUE(key, value) { \
    if (value)\
        [mutableUserInfo setObject:(__bridge id)value.get() forKey:key];\
}

#define INJECT_STRING_VALUE(key, value) { \
    if (!value.isNull())\
        [mutableUserInfo setObject:value.createNSString().get() forKey:key];\
}

RetainPtr<id> CoreIPCError::toID() const
{
    RetainPtr<NSMutableDictionary> mutableUserInfo = adoptNS([[NSMutableDictionary alloc] init]);

    if (m_clientCertificateChain) {
        RetainPtr<NSMutableArray> chain = adoptNS([[NSMutableArray alloc] init]);
        for (RetainPtr item : *m_clientCertificateChain)
            [chain addObject:(__bridge id)item.get()];
        INJECT_VALUE(@"NSErrorClientCertificateChainKey", chain)
    }

    if (m_peerCertificateChain) {
        RetainPtr<NSMutableArray> chain = adoptNS([[NSMutableArray alloc] init]);
        for (RetainPtr item : *m_peerCertificateChain)
            [chain addObject:(__bridge id)item.get()];
        INJECT_VALUE(@"NSErrorPeerCertificateChainKey", chain)
    }

    INJECT_STRING_VALUE(NSLocalizedDescriptionKey, m_localizedDescription)
    INJECT_STRING_VALUE(NSLocalizedFailureReasonErrorKey, m_localizedFailureReasonError)
    INJECT_STRING_VALUE(NSLocalizedRecoverySuggestionErrorKey, m_localizedRecoverySuggestionError)
    INJECT_STRING_VALUE(NSLocalizedFailureErrorKey, m_localizedFailureError)

    if (m_localizedRecoveryOptionsError) {
        RetainPtr options = [NSMutableArray arrayWithCapacity:m_localizedRecoveryOptionsError->size()];
        for (const String& entry : *m_localizedRecoveryOptionsError)
            [options addObject:entry.createNSString().get()];
        INJECT_VALUE(NSLocalizedRecoveryOptionsErrorKey, options)
    }

    INJECT_STRING_VALUE(NSHelpAnchorErrorKey, m_helpAnchorError)
    INJECT_STRING_VALUE(NSDebugDescriptionErrorKey, m_debugDescriptionError)

    INJECT_VALUE(NSStringEncodingErrorKey, m_stringEncodingError)

    INJECT_VALUE(NSURLErrorFailingURLPeerTrustErrorKey, m_failingURLPeerTrustError)
    INJECT_VALUE(NSURLErrorKey, m_urlError)
    INJECT_VALUE(NSURLErrorFailingURLErrorKey, m_failingURLError)

    INJECT_STRING_VALUE(NSFilePathErrorKey, m_filePathError)

    INJECT_STRING_VALUE(@"networkTaskDescription", m_networkTaskDescription)
    INJECT_STRING_VALUE(@"networkTaskMetricsPrivacyStance", m_networkTaskMetricsPrivacyStance)

    INJECT_STRING_VALUE(@"NSDescription", m_description)

    if (m_underlyingError) {
        RetainPtr underlyingNSError = m_underlyingError->toID();
        if (!underlyingNSError)
            return nil;

        INJECT_VALUE(NSUnderlyingErrorKey, underlyingNSError)
    }

    return adoptNS([[NSError alloc] initWithDomain:m_domain.createNSString().get() code:m_code userInfo:(__bridge NSDictionary *)mutableUserInfo.get()]);
}


#define EXTRACT_TYPED_VALUE(key, type, target) { \
    RetainPtr<id> extractedValue = [userInfo objectForKey:key];\
    if ([extractedValue isKindOfClass:[type class]])\
        target = extractedValue.get();\
}

#define EXTRACT_STRING_VALUE(key, target) { \
    RetainPtr<id> extractedValue = [userInfo objectForKey:key];\
    if ([extractedValue isKindOfClass:[NSString class]])\
        target = String(extractedValue.get());\
}

CoreIPCError::CoreIPCError(NSError *nsError)
    : m_domain([nsError domain])
    , m_code([nsError code])
{
    RetainPtr<NSDictionary> userInfo = [nsError userInfo];

    if (RetainPtr<NSArray> clientIdentityAndCertificates = [userInfo objectForKey:@"NSErrorClientCertificateChainKey"]) {
        if ([clientIdentityAndCertificates isKindOfClass:[NSArray class]]) {
            m_clientCertificateChain = Vector<RetainPtr<SecCertificateRef>> { };
            // Turn SecIdentity members into SecCertificate to strip out private key information.
            for (id object in clientIdentityAndCertificates.get()) {
                // Only SecIdentity or SecCertificate types are expected in clientIdentityAndCertificates
                if (CFGetTypeID((__bridge CFTypeRef)object) != SecIdentityGetTypeID() && CFGetTypeID((__bridge CFTypeRef)object) != SecCertificateGetTypeID())
                    continue;
                if (CFGetTypeID((__bridge CFTypeRef)object) != SecIdentityGetTypeID()) {
                    m_clientCertificateChain->append((__bridge SecCertificateRef)object);
                    continue;
                }
                SecCertificateRef certificate = nil;
                OSStatus status = SecIdentityCopyCertificate((SecIdentityRef)object, &certificate);
                // The SecIdentity member is the key information of this attribute. Without it, we should nil
                // the attribute.
                if (status != errSecSuccess) {
                    LOG_ERROR("Failed to encode nsError.userInfo[NSErrorClientCertificateChainKey]: %ld", static_cast<long>(status));
                    m_clientCertificateChain = std::nullopt;
                    break;
                }
                m_clientCertificateChain->append(adoptCF(certificate));
            }
        }
    }

    RetainPtr<NSArray> peerCertificateChain = dynamic_objc_cast<NSArray>([userInfo objectForKey:@"NSErrorPeerCertificateChainKey"]);
    if (!peerCertificateChain) {
        if (RetainPtr<id> candidatePeerTrust = [userInfo objectForKey:NSURLErrorFailingURLPeerTrustErrorKey]) {
            if (CFGetTypeID((__bridge CFTypeRef)candidatePeerTrust.get()) == SecTrustGetTypeID())
                peerCertificateChain = bridge_cast(adoptCF(SecTrustCopyCertificateChain((__bridge SecTrustRef)candidatePeerTrust.get())));
        }
    }

    if (peerCertificateChain && [peerCertificateChain isKindOfClass:[NSArray class]]) {
        m_peerCertificateChain = Vector<RetainPtr<SecCertificateRef>> { };
        for (id object in peerCertificateChain.get()) {
            if (CFGetTypeID((__bridge CFTypeRef)object) != SecCertificateGetTypeID()) {
                m_peerCertificateChain = std::nullopt;
                break;
            }
            m_peerCertificateChain->append((__bridge SecCertificateRef)object);
        }
    }

    if (RetainPtr<id> underlyingError = [userInfo objectForKey:NSUnderlyingErrorKey]) {
        if (RetainPtr error = dynamic_objc_cast<NSError>(underlyingError))
            m_underlyingError = makeUnique<CoreIPCError>(error.get());
    }

    EXTRACT_STRING_VALUE(NSLocalizedDescriptionKey, m_localizedDescription)
    EXTRACT_STRING_VALUE(NSLocalizedFailureReasonErrorKey, m_localizedFailureReasonError)
    EXTRACT_STRING_VALUE(NSLocalizedRecoverySuggestionErrorKey, m_localizedRecoverySuggestionError)
    EXTRACT_STRING_VALUE(NSLocalizedFailureErrorKey, m_localizedFailureError)

    if (RetainPtr<id> extractedValue = [userInfo objectForKey:NSLocalizedRecoveryOptionsErrorKey]) {
        if (RetainPtr array = dynamic_objc_cast<NSArray>(extractedValue)) {
            m_localizedRecoveryOptionsError = Vector<String> { };
            for (id object in array.get()) {
                if (RetainPtr string = dynamic_objc_cast<NSString>(object))
                    m_localizedRecoveryOptionsError->append(string.get());
            }
        }
    }

    EXTRACT_STRING_VALUE(NSHelpAnchorErrorKey, m_helpAnchorError)
    EXTRACT_STRING_VALUE(NSDebugDescriptionErrorKey, m_debugDescriptionError)

    EXTRACT_TYPED_VALUE(NSStringEncodingErrorKey, NSNumber, m_stringEncodingError)

    if (RetainPtr peerTrust = (__bridge SecTrustRef)[userInfo objectForKey:NSURLErrorFailingURLPeerTrustErrorKey]) {
        if (CFGetTypeID((__bridge CFTypeRef)peerTrust.get()) == SecTrustGetTypeID())
            m_failingURLPeerTrustError = peerTrust;
    }

    EXTRACT_TYPED_VALUE(NSURLErrorKey, NSURL, m_urlError)
    EXTRACT_TYPED_VALUE(NSURLErrorFailingURLErrorKey, NSURL, m_failingURLError)

    EXTRACT_STRING_VALUE(NSFilePathErrorKey, m_filePathError)

    EXTRACT_STRING_VALUE(@"networkTaskDescription", m_networkTaskDescription)
    EXTRACT_STRING_VALUE(@"networkTaskMetricsPrivacyStance", m_networkTaskMetricsPrivacyStance)

    EXTRACT_STRING_VALUE(@"NSDescription", m_description)
}

} // namespace WebKit
