/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "CoreIPCNSURLCredential.h"

#import <pal/spi/cf/CFNetworkSPI.h>

@interface NSURLCredential(WKSecureCoding)
- (NSDictionary *)_webKitPropertyListData;
- (instancetype)_initWithWebKitPropertyListData:(NSDictionary *)plist;
@end

#define SET_MDATA(NAME, CLASS, WRAPPER)     \
    id NAME = dict[@#NAME];                 \
    if ([NAME isKindOfClass:CLASS.class]) { \
        auto var = WRAPPER(NAME);           \
        m_data.NAME = WTFMove(var);         \
    }

namespace WebKit {

#if HAVE(WK_SECURE_CODING_NSURLCREDENTIAL)
CoreIPCNSURLCredential::CoreIPCNSURLCredential(NSURLCredential *credential)
{
    auto dict = [credential _webKitPropertyListData];

    NSNumber *persistence = dict[@"persistence"];
    if ([persistence isKindOfClass:NSNumber.class]) {
        switch ([persistence unsignedCharValue]) {
        case kCFURLCredentialPersistenceNone:
            m_data.persistence = CoreIPCNSURLCredentialPersistence::None;
            break;
        case kCFURLCredentialPersistenceForSession:
            m_data.persistence = CoreIPCNSURLCredentialPersistence::Session;
            break;
        case kCFURLCredentialPersistencePermanent:
            m_data.persistence = CoreIPCNSURLCredentialPersistence::Permanent;
            break;
        case kCFURLCredentialPersistenceSynchronizable:
            m_data.persistence = CoreIPCNSURLCredentialPersistence::Synchronizable;
            break;
        default:
            ASSERT_NOT_REACHED();
            m_data.persistence = CoreIPCNSURLCredentialPersistence::None;
            break;
        }
    }
    SET_MDATA(user, NSString, CoreIPCString);
    SET_MDATA(password, NSString, CoreIPCString);

    NSDictionary *attributes = dict[@"attributes"];
    if ([attributes isKindOfClass:NSDictionary.class]) {
        Vector<CoreIPCNSURLCredentialData::Attributes> vector;
        vector.reserveCapacity(attributes.count);
        for (id key in attributes) {
            if (![key isKindOfClass:NSString.class]) {
                ASSERT_NOT_REACHED();
                break;
            }
            id value = [attributes objectForKey:key];
            if (![value isKindOfClass:NSString.class] || ![value isKindOfClass:NSNumber.class] || ![value isKindOfClass:NSDate.class]) {
                ASSERT_NOT_REACHED();
                break;
            }
            std::optional<std::variant<CoreIPCNumber, CoreIPCString, CoreIPCDate>> option;
            if ([value isKindOfClass:NSString.class])
                option = CoreIPCString(value);
            if ([value isKindOfClass:NSNumber.class])
                option = CoreIPCNumber(value);
            if ([value isKindOfClass:NSDate.class])
                option = CoreIPCDate(value);
            if (option) {
                auto k = CoreIPCString(key);
                vector.append(std::make_pair(WTFMove(k), WTFMove(*option)));
            }
        }
        m_data.attributes = WTFMove(vector);
    }

    SET_MDATA(identifier, NSString, CoreIPCString);

    NSNumber *useKeychain = dict[@"useKeychain"];
    if ([useKeychain isKindOfClass:NSNumber.class])
        m_data.useKeychain = [useKeychain boolValue];

    SecTrustRef secTrust = static_cast<SecTrustRef>(dict[@"trust"]);
    if (secTrust && CFGetTypeID(secTrust) == SecTrustGetTypeID())
        m_data.trust = CoreIPCSecTrust(secTrust);

    SET_MDATA(service, NSString, CoreIPCString);

    NSDictionary *flags = dict[@"flags"];
    if ([flags isKindOfClass:NSDictionary.class]) {
        Vector<WebKit::CoreIPCNSURLCredentialData::Flags> vector;
        vector.reserveCapacity(flags.count);
        for (NSString *key in attributes) {
            if (![key isKindOfClass:NSString.class]) {
                ASSERT_NOT_REACHED();
                break;
            }
            NSString *value = [flags objectForKey:key];
            if (![value isKindOfClass:NSString.class]) {
                ASSERT_NOT_REACHED();
                break;
            }
            auto k = CoreIPCString(key);
            auto v = CoreIPCString(value);
            vector.append(std::make_pair(WTFMove(k), WTFMove(v)));
        }
        m_data.flags = WTFMove(vector);
    }

    SET_MDATA(uuid, NSString, CoreIPCString);
    SET_MDATA(appleID, NSString, CoreIPCString);
    SET_MDATA(realm, NSString, CoreIPCString);
    SET_MDATA(token, NSString, CoreIPCString);

    NSNumber *type = dict[@"type"];
    if ([type isKindOfClass:NSNumber.class]) {
        switch ([type intValue]) {
        case kURLCredentialInternetPassword:
            m_data.type = CoreIPCNSURLCredentialType::Password;
            break;
        case kURLCredentialServerTrust:
            m_data.type = CoreIPCNSURLCredentialType::ServerTrust;
            break;
        case kURLCredentialKerberosTicket:
            m_data.type = CoreIPCNSURLCredentialType::KerberosTicket;
            break;
        case kURLCredentialXMobileMeAuthToken:
            m_data.type = CoreIPCNSURLCredentialType::XMobileMeAuthToken;
            break;
        case kURLCredentialOAuth2:
            m_data.type = CoreIPCNSURLCredentialType::OAuth2;
            break;
        default:
            ASSERT_NOT_REACHED();
            m_data.type = CoreIPCNSURLCredentialType::Password;
            break;
        }
    }
}

CoreIPCNSURLCredential::CoreIPCNSURLCredential(CoreIPCNSURLCredentialData&& data)
    : m_data(WTFMove(data)) { }

RetainPtr<id> CoreIPCNSURLCredential::toID() const
{
    auto dict = adoptNS([[NSMutableDictionary alloc] initWithCapacity:7]);

    RetainPtr<NSNumber> persistence;
    switch (m_data.persistence) {
    case CoreIPCNSURLCredentialPersistence::None:
        persistence = @(kCFURLCredentialPersistenceNone);
        break;
    case CoreIPCNSURLCredentialPersistence::Session:
        persistence = @(kCFURLCredentialPersistenceForSession);
        break;
    case CoreIPCNSURLCredentialPersistence::Permanent:
        persistence = @(kCFURLCredentialPersistencePermanent);
        break;
    case CoreIPCNSURLCredentialPersistence::Synchronizable:
        persistence = @(kCFURLCredentialPersistenceSynchronizable);
        break;
    default:
        ASSERT_NOT_REACHED();
        persistence = @(kCFURLCredentialPersistenceNone);
        break;
    }
    [dict setObject:persistence.get() forKey:@"persistence"];

    switch (m_data.type) {
    case CoreIPCNSURLCredentialType::Password:
        [dict setObject:@(kURLCredentialInternetPassword) forKey:@"type"];
        if (m_data.user)
            [dict setObject:(*m_data.user).toID().get() forKey:@"user"];
        if (m_data.password)
            [dict setObject:(*m_data.password).toID().get() forKey:@"password"];
        if (m_data.attributes) {
            RetainPtr attributes = adoptNS([[NSMutableDictionary alloc] initWithCapacity:(*m_data.attributes).size()]);
            for (auto& attributePair : *m_data.attributes) {
                RetainPtr<id> value;
                WTF::switchOn(attributePair.second,
                    [&] (const CoreIPCNumber& n) {
                        value = n.toID();
                    },
                    [&] (const CoreIPCString& s) {
                        value = s.toID();
                    },
                    [&] (const CoreIPCDate& d) {
                        value = d.toID();
                    }
                );
                [attributes setObject:attributes.get() forKey:attributePair.first.toID().get()];
            }
            [dict setObject:attributes.get() forKey:@"attributes"];
        }
        if (m_data.identifier)
            [dict setObject:(*m_data.identifier).toID().get() forKey:@"identifier"];
        if (m_data.useKeychain)
            [dict setObject:[NSNumber numberWithBool:(*m_data.useKeychain)] forKey:@"useKeychain"];
        break;
    case CoreIPCNSURLCredentialType::ServerTrust: {
        RetainPtr<SecTrustRef> trust = m_data.trust.createSecTrust();
        if (trust) {
            [dict setObject:@(kURLCredentialServerTrust) forKey:@"type"];
            [dict setObject:(id)trust.get() forKey:@"trust"];
        }
        break;
    }
    case CoreIPCNSURLCredentialType::KerberosTicket:
        [dict setObject:@(kURLCredentialKerberosTicket) forKey:@"type"];
        if (m_data.user)
            [dict setObject:(*m_data.user).toID().get() forKey:@"user"];
        if (m_data.service)
            [dict setObject:(*m_data.service).toID().get() forKey:@"service"];
        if (m_data.uuid)
            [dict setObject:(*m_data.uuid).toID().get() forKey:@"uuid"];
        if (m_data.flags) {
            auto flags = adoptNS([[NSMutableDictionary alloc] initWithCapacity:(*m_data.flags).size()]);
            for (auto& flagPair : *m_data.flags)
                [flags setObject: flagPair.second.toID().get() forKey:flagPair.first.toID().get()];
            [dict setObject:flags.get() forKey:@"flags"];
        }
        break;
    case CoreIPCNSURLCredentialType::XMobileMeAuthToken:
        [dict setObject:@(kURLCredentialXMobileMeAuthToken) forKey:@"type"];
        if (m_data.appleID)
            [dict setObject:(*m_data.appleID).toID().get() forKey:@"appleid"];
        if (m_data.password)
            [dict setObject:(*m_data.password).toID().get() forKey:@"password"];
        if (m_data.realm)
            [dict setObject:(*m_data.realm).toID().get() forKey:@"realm"];
        break;
    case CoreIPCNSURLCredentialType::OAuth2:
        [dict setObject:@(kURLCredentialOAuth2) forKey:@"type"];
        if (m_data.token)
            [dict setObject:(*m_data.token).toID().get() forKey:@"token"];
        if (m_data.realm)
            [dict setObject:(*m_data.realm).toID().get() forKey:@"realm"];
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return adoptNS([[NSURLCredential alloc] _initWithWebKitPropertyListData:dict.get()]);
}
#endif

#if !HAVE(WK_SECURE_CODING_NSURLCREDENTIAL) && !HAVE(DICTIONARY_SERIALIZABLE_NSURLCREDENTIAL)

CoreIPCNSURLCredential::CoreIPCNSURLCredential(NSURLCredential *credential)
    : m_serializedBytes([NSKeyedArchiver archivedDataWithRootObject:credential requiringSecureCoding:YES error:nil]) { }

RetainPtr<id> CoreIPCNSURLCredential::toID() const
{
    return [NSKeyedUnarchiver unarchivedObjectOfClass:NSURLCredential.class fromData:m_serializedBytes.get() error:nil];
}

#endif

} // namespace WebKit

#undef SET_MDATA
