/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "CoreIPCPKPaymentMerchantSession.h"

#if USE(PASSKIT) && HAVE(WK_SECURE_CODING_PKPAYMENTMERCHANTSESSION)

#import "ArgumentCodersCocoa.h"
#import "Logging.h"
#import "WKKeyedCoder.h"
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <pal/cocoa/PassKitSoftLink.h>


namespace WebKit {

CoreIPCPKPaymentMerchantSession::CoreIPCPKPaymentMerchantSession(PKPaymentMerchantSession *session)
{
    if (!session)
        return;

    RetainPtr archiver = adoptNS([WKKeyedCoder new]);
    [session encodeWithCoder:archiver.get()];
    RetainPtr dictionary = [archiver accumulatedDictionary];

    CoreIPCPKPaymentMerchantSessionData data;

    if (RetainPtr merchantIdentifier = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"merchantIdentifier"]))
        data.merchantIdentifier = WTF::move(merchantIdentifier);

    if (RetainPtr merchantSessionIdentifier = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"merchantSessionIdentifier"]))
        data.merchantSessionIdentifier = WTF::move(merchantSessionIdentifier);

    if (RetainPtr nonce = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"nonce"]))
        data.nonce = WTF::move(nonce);

    if (RetainPtr epochTimestamp = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"epochTimestamp"]))
        data.epochTimestamp = WTF::move(epochTimestamp);

    if (RetainPtr expiresAt = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"expiresAt"]))
        data.expiresAt = WTF::move(expiresAt);

    if (RetainPtr domainName = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"domainName"]))
        data.domainName = WTF::move(domainName);

    if (RetainPtr displayName = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"displayName"]))
        data.displayName = WTF::move(displayName);

    if (RetainPtr signature = dynamic_objc_cast<NSData>([dictionary.get() objectForKey:@"signature"]))
        data.signature = WTF::move(signature);

    if (RetainPtr retryNonce = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"retryNonce"]))
        data.retryNonce = WTF::move(retryNonce);

    if (RetainPtr initiativeContext = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"initiativeContext"]))
        data.initiativeContext = WTF::move(initiativeContext);

    if (RetainPtr initiative = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"initiative"]))
        data.initiative = WTF::move(initiative);

    if (RetainPtr ampEnrollmentPinning = dynamic_objc_cast<NSData>([dictionary.get() objectForKey:@"ampEnrollmentPinning"]))
        data.ampEnrollmentPinning = WTF::move(ampEnrollmentPinning);

    if (RetainPtr operationalAnalyticsIdentifier = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"operationalAnalyticsIdentifier"]))
        data.operationalAnalyticsIdentifier = WTF::move(operationalAnalyticsIdentifier);

    if (RetainPtr signedFields = dynamic_objc_cast<NSArray>([dictionary.get() objectForKey:@"signedFields"])) {
        Vector<RetainPtr<NSString>> result;
        result.reserveInitialCapacity([signedFields.get() count]);
        for (id item in signedFields.get()) {
            if ([item isKindOfClass:NSString.class])
                result.append(item);
        }
        data.signedFields = WTF::move(result);
    }

#if ENABLE(APPLE_PAY_DELEGATED_REQUEST)
    if (RetainPtr isDelegatedSessionNumber = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"isDelegatedSession"]))
        data.isDelegatedSession = [isDelegatedSessionNumber boolValue];

    if (RetainPtr delegateDisplayName = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"delegateDisplayName"]))
        data.delegateDisplayName = WTF::move(delegateDisplayName);
#endif

    m_data = WTF::move(data);
}

CoreIPCPKPaymentMerchantSession::CoreIPCPKPaymentMerchantSession(std::optional<CoreIPCPKPaymentMerchantSessionData>&& data)
    : m_data { WTF::move(data) }
{
}

RetainPtr<id> CoreIPCPKPaymentMerchantSession::toID() const
{
    if (!m_data)
        return { };

    RetainPtr dictionary = [NSMutableDictionary dictionaryWithCapacity:16];

    if (m_data->merchantIdentifier)
        [dictionary setObject:m_data->merchantIdentifier.get() forKey:@"merchantIdentifier"];
    if (m_data->merchantSessionIdentifier)
        [dictionary setObject:m_data->merchantSessionIdentifier.get() forKey:@"merchantSessionIdentifier"];
    if (m_data->nonce)
        [dictionary setObject:m_data->nonce.get() forKey:@"nonce"];
    if (m_data->epochTimestamp)
        [dictionary setObject:m_data->epochTimestamp.get() forKey:@"epochTimestamp"];
    if (m_data->expiresAt)
        [dictionary setObject:m_data->expiresAt.get() forKey:@"expiresAt"];
    if (m_data->domainName)
        [dictionary setObject:m_data->domainName.get() forKey:@"domainName"];
    if (m_data->displayName)
        [dictionary setObject:m_data->displayName.get() forKey:@"displayName"];
    if (m_data->signature)
        [dictionary setObject:m_data->signature.get() forKey:@"signature"];
    if (m_data->retryNonce)
        [dictionary setObject:m_data->retryNonce.get() forKey:@"retryNonce"];
    if (m_data->initiativeContext)
        [dictionary setObject:m_data->initiativeContext.get() forKey:@"initiativeContext"];
    if (m_data->initiative)
        [dictionary setObject:m_data->initiative.get() forKey:@"initiative"];
    if (m_data->ampEnrollmentPinning)
        [dictionary setObject:m_data->ampEnrollmentPinning.get() forKey:@"ampEnrollmentPinning"];
    if (m_data->operationalAnalyticsIdentifier)
        [dictionary setObject:m_data->operationalAnalyticsIdentifier.get() forKey:@"operationalAnalyticsIdentifier"];

    if (m_data->signedFields) {
        RetainPtr arr = [NSMutableArray arrayWithCapacity:m_data->signedFields->size()];
        for (auto& element : *m_data->signedFields)
            [arr addObject:element.get()];
        [dictionary setObject:arr.get() forKey:@"signedFields"];
    }

#if ENABLE(APPLE_PAY_DELEGATED_REQUEST)
    if (m_data->isDelegatedSession)
        [dictionary setObject:@(*m_data->isDelegatedSession) forKey:@"isDelegatedSession"];
    if (m_data->delegateDisplayName)
        [dictionary setObject:m_data->delegateDisplayName.get() forKey:@"delegateDisplayName"];
#endif

    RetainPtr unarchiver = adoptNS([[WKKeyedCoder alloc] initWithDictionary:dictionary.get()]);
    RetainPtr session = adoptNS([[PAL::getPKPaymentMerchantSessionClassSingleton() alloc] initWithCoder:unarchiver.get()]);
    if (!session)
        RELEASE_LOG_ERROR(IPC, "CoreIPCPKPaymentMerchantSession was not able to reconstruct a PKPaymentMerchantSession object");
    return session;
}

} // namespace WebKit

#endif
