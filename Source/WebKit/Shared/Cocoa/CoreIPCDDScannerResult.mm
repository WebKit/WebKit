/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "CoreIPCDDScannerResult.h"

#import <pal/cocoa/DataDetectorsCoreSoftLink.h>

#if ENABLE(DATA_DETECTION) && HAVE(WK_SECURE_CODING_DATA_DETECTORS)

namespace WebKit {

CoreIPCDDScannerResult::CoreIPCDDScannerResult(DDScannerResult *d)
{
    if (!d)
        return;

    RetainPtr dictionary = [d _webKitPropertyListData];

    if (auto *AR = dynamic_objc_cast<NSValue>([dictionary objectForKey:@"AR"])) {
        RetainPtr NSRangeTypeString = [NSString stringWithCString:@encode(NSRange) encoding:NSASCIIStringEncoding];
        RetainPtr type = [NSString stringWithCString:AR.objCType encoding:NSASCIIStringEncoding];
        if ([type.get() isEqual:NSRangeTypeString.get()])
            m_data.AR = [AR rangeValue];
    }

    if (auto *MS = dynamic_objc_cast<NSString>([dictionary objectForKey:@"MS"]))
        m_data.MS = MS;

    if (auto *T = dynamic_objc_cast<NSString>([dictionary objectForKey:@"T"]))
        m_data.T = T;

    if (auto *P = dynamic_objc_cast<NSNumber>([dictionary objectForKey:@"P"]))
        m_data.P = P;

    if (auto *VN = dynamic_objc_cast<NSNumber>([dictionary objectForKey:@"VN"]))
        m_data.VN = VN;

    if (auto *SR = dynamic_objc_cast<NSArray>([dictionary objectForKey:@"SR"])) {
        Vector<RetainPtr<DDScannerResult>> result;
        result.reserveInitialCapacity(SR.count);
        for (id item in SR) {
            if ([item isKindOfClass:PAL::getDDScannerResultClassSingleton()])
                result.append((DDScannerResult *)item);
        }
        m_data.SR = WTF::move(result);
    }

    if (auto *V = dynamic_objc_cast<NSString>([dictionary objectForKey:@"V"]))
        m_data.V = V;

    if (auto *CF = dynamic_objc_cast<NSNumber>([dictionary objectForKey:@"CF"]))
        m_data.CF = CF;

    if (auto *C = dynamic_objc_cast<NSDictionary>([dictionary objectForKey:@"C"])) {
#if ASSERT_ENABLED
        // Validate that the contextual data dictionary only contains known keys.
        // This will catch if DataDetectorsCore's serialization changes underneath us.
        RetainPtr allowedKeys = [NSSet setWithObjects:@"C", @"D", @"U", @"urlificationBegin", @"urlificationLength", nil];
        for (NSString *key in C)
            ASSERT_WITH_MESSAGE([allowedKeys.get() containsObject:key], "Unexpected key '%s' in DDScannerResult contextual data dictionary. Expected only: C, D, U, urlificationBegin, urlificationLength", [key UTF8String]);
#endif

        if (auto *addressBookUID = dynamic_objc_cast<NSString>([C objectForKey:@"C"]))
            m_data.AddressBookUID = addressBookUID;

        if (auto *domain = dynamic_objc_cast<NSString>([C objectForKey:@"D"]))
            m_data.Domain = domain;

        if (auto *uuid = dynamic_objc_cast<NSString>([C objectForKey:@"U"]))
            m_data.UUID = uuid;

        if (auto *urlificationBegin = dynamic_objc_cast<NSNumber>([C objectForKey:@"urlificationBegin"]))
            m_data.UrlificationBegin = urlificationBegin;

        if (auto *urlificationLength = dynamic_objc_cast<NSNumber>([C objectForKey:@"urlificationLength"]))
            m_data.UrlificationLength = urlificationLength;
    }
}

CoreIPCDDScannerResult::CoreIPCDDScannerResult(CoreIPCDDScannerResultData&& data)
    : m_data(WTF::move(data)) { }

RetainPtr<id> CoreIPCDDScannerResult::toID() const
{
    RetainPtr propertyList = [NSMutableDictionary dictionaryWithCapacity:9];

    propertyList.get()[@"AR"] = [NSValue valueWithRange:m_data.AR];
    if (m_data.MS)
        propertyList.get()[@"MS"] = m_data.MS.get();
    if (m_data.T)
        propertyList.get()[@"T"] = m_data.T.get();
    if (m_data.P)
        propertyList.get()[@"P"] = m_data.P.get();
    if (m_data.VN)
        propertyList.get()[@"VN"] = m_data.VN.get();

    if (m_data.SR) {
        RetainPtr arr = [NSMutableArray arrayWithCapacity:m_data.SR->size()];
        for (auto& element : *m_data.SR)
            [arr addObject:element.get()];
        propertyList.get()[@"SR"] = arr.get();
    }

    if (m_data.V)
        propertyList.get()[@"V"] = m_data.V.get();
    if (m_data.CF)
        propertyList.get()[@"CF"] = m_data.CF.get();

    if (m_data.AddressBookUID || m_data.Domain || m_data.UUID
        || m_data.UrlificationBegin || m_data.UrlificationLength) {
        auto contextualData = [NSMutableDictionary dictionary];

        if (m_data.AddressBookUID)
            contextualData[@"C"] = m_data.AddressBookUID.get();
        if (m_data.Domain)
            contextualData[@"D"] = m_data.Domain.get();
        if (m_data.UUID)
            contextualData[@"U"] = m_data.UUID.get();
        if (m_data.UrlificationBegin)
            contextualData[@"urlificationBegin"] = m_data.UrlificationBegin.get();
        if (m_data.UrlificationLength)
            contextualData[@"urlificationLength"] = m_data.UrlificationLength.get();

        propertyList.get()[@"C"] = contextualData;
    }

    return adoptNS([[PAL::getDDScannerResultClassSingleton() alloc] _initWithWebKitPropertyListData:propertyList.get()]);
}

}

#endif // ENABLE(DATA_DETECTION) && HAVE(WK_SECURE_CODING_DATA_DETECTORS)
