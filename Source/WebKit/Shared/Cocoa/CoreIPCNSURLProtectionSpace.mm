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
#import "CoreIPCNSURLProtectionSpace.h"

#import "ArgumentCoders.h"
#import <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA) && HAVE(WK_SECURE_CODING_NSURLPROTECTIONSPACE)

@interface NSURLProtectionSpace (WKSecureCoding)
- (NSDictionary *)_webKitPropertyListData;
- (instancetype)_initWithWebKitPropertyListData:(NSDictionary *)plist;
@end

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CoreIPCNSURLProtectionSpace);

#define SET_OBJECT(NAME, CLASS, WRAPPER)    \
    id NAME = dict[@#NAME];                 \
    if ([NAME isKindOfClass:CLASS.class]) { \
        auto var = WRAPPER(NAME);           \
        m_data.NAME = WTFMove(var);         \
    }

CoreIPCNSURLProtectionSpace::CoreIPCNSURLProtectionSpace(NSURLProtectionSpace *ps)
{
    auto dict = [ps _webKitPropertyListData];

    SET_OBJECT(host, NSString, CoreIPCString);

    id port = dict[@"port"];
    if ([port isKindOfClass:NSNumber.class])
        m_data.port = [port unsignedShortValue];

    id type = dict[@"type"];
    if ([type isKindOfClass:NSNumber.class]) {
        auto val = [type unsignedCharValue];
        if (isValidEnum<WebCore::ProtectionSpaceBaseServerType>(val))
            m_data.type = static_cast<WebCore::ProtectionSpaceBaseServerType>(val);
    }

    SET_OBJECT(realm, NSString, CoreIPCString);

    id scheme = dict[@"scheme"];
    if ([scheme isKindOfClass:NSNumber.class]) {
        auto val = [scheme unsignedCharValue];
        if (isValidEnum<WebCore::ProtectionSpaceBaseAuthenticationScheme>(val))
            m_data.scheme = static_cast<WebCore::ProtectionSpaceBaseAuthenticationScheme>(val);
    }

    id trust = dict[@"trust"];
    if (trust && CFGetTypeID((CFTypeRef)trust) == SecTrustGetTypeID())
        m_data.trust = { CoreIPCSecTrust((SecTrustRef)trust) };

    NSArray *distnames = dict[@"distnames"];
    if ([distnames isKindOfClass:NSArray.class]) {
        bool allElementsValid = true;
        Vector<WebKit::CoreIPCData> data;
        data.reserveInitialCapacity(distnames.count);
        for (NSData *d in distnames) {
            if (![d isKindOfClass:NSData.class]) {
                allElementsValid = false;
                break;
            }
            data.append(d);
        }
        if (allElementsValid)
            m_data.distnames = WTFMove(data);
    }
}

CoreIPCNSURLProtectionSpace::CoreIPCNSURLProtectionSpace(CoreIPCNSURLProtectionSpaceData&& data)
    : m_data(WTFMove(data)) { }

CoreIPCNSURLProtectionSpace::CoreIPCNSURLProtectionSpace(const RetainPtr<NSURLProtectionSpace>& ps)
    : CoreIPCNSURLProtectionSpace(ps.get()) { }

RetainPtr<id> CoreIPCNSURLProtectionSpace::toID() const
{
    auto dict = adoptNS([[NSMutableDictionary alloc] initWithCapacity:7]); // Initialized with the count of members in CoreIPCNSURLProtectionSpaceData

    if (m_data.host)
        [dict setObject:m_data.host->toID().get() forKey:@"host"];

    [dict setObject:[NSNumber numberWithUnsignedShort:m_data.port] forKey:@"port"];
    [dict setObject:[NSNumber numberWithUnsignedChar:static_cast<uint8_t>(m_data.type)] forKey:@"type"];

    if (m_data.realm)
        [dict setObject:m_data.realm->toID().get() forKey:@"realm"];

    [dict setObject:[NSNumber numberWithUnsignedChar:static_cast<uint8_t>(m_data.scheme)] forKey:@"scheme"];

    if (m_data.trust) {
        if (RetainPtr trust = m_data.trust->createSecTrust())
            [dict setObject:(id)trust.get() forKey:@"trust"];
    }

    if (m_data.distnames) {
        auto array = adoptNS([[NSMutableArray alloc] initWithCapacity:m_data.distnames->size()]);
        for (auto& value : *m_data.distnames)
            [array addObject:value.toID().get()];
        [dict setObject:array.get() forKey:@"distnames"];
    }
    return adoptNS([[NSURLProtectionSpace alloc] _initWithWebKitPropertyListData:dict.get()]);
}

#undef SET_OBJECT

} // namespace WebKit

#endif
