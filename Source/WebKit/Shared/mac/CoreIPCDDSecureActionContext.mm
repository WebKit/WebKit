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
#import "CoreIPCDDSecureActionContext.h"

#import <pal/cocoa/DataDetectorsCoreSoftLink.h>
#import <pal/mac/DataDetectorsSoftLink.h>

#if PLATFORM(MAC) && HAVE(WK_SECURE_CODING_DATA_DETECTORS)

@interface DDSecureActionContext(WKSecureCoding)
- (NSDictionary *)_webKitPropertyListData;
- (instancetype)_initWithWebKitPropertyListData:(NSDictionary *)plist;
@end

namespace WebKit {

CoreIPCDDSecureActionContext::CoreIPCDDSecureActionContext(DDSecureActionContext * object)
{
    RetainPtr dictionary = [object _webKitPropertyListData];

    RetainPtr NSRectTypeString = [NSString stringWithCString:@encode(NSRect) encoding:NSASCIIStringEncoding];

    if (auto *highlightFrame = dynamic_objc_cast<NSValue>([dictionary.get() objectForKey:@"highlightFrame"])) {
        RetainPtr type = [NSString stringWithCString:[highlightFrame objCType] encoding:NSASCIIStringEncoding];
        if ([type.get() isEqual:NSRectTypeString.get()])
            m_data.highlightFrame = WebCore::enclosingIntRect([highlightFrame rectValue]);
    }

    if (auto *aimFrame = dynamic_objc_cast<NSValue>([dictionary.get() objectForKey:@"aimFrame"])) {
        RetainPtr type = [NSString stringWithCString:[aimFrame objCType] encoding:NSASCIIStringEncoding];
        if ([type.get() isEqual:NSRectTypeString.get()])
            m_data.aimFrame = WebCore::enclosingIntRect([aimFrame rectValue]);
    }

    if (auto *eventTitle = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"eventTitle"]))
        m_data.eventTitle = eventTitle;

    if (auto *leadingText = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"leadingText"]))
        m_data.leadingText = leadingText;

    if (auto *trailingText = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"trailingText"]))
        m_data.trailingText = trailingText;

    if (auto *coreSpotlightUniqueIdentifier = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"coreSpotlightUniqueIdentifier"]))
        m_data.coreSpotlightUniqueIdentifier = coreSpotlightUniqueIdentifier;

    if (auto *referenceDate = dynamic_objc_cast<NSDate>([dictionary.get() objectForKey:@"referenceDate"]))
        m_data.referenceDate = referenceDate;

    if (auto *hostUUID = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"hostUUID"]))
        m_data.hostUUID = hostUUID;

    if (auto *authorABUUID = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"authorABUUID"]))
        m_data.authorABUUID = authorABUUID;

    if (auto *authorEmailAddress = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"authorEmailAddress"]))
        m_data.authorEmailAddress = authorEmailAddress;

    if (auto *authorName = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"authorName"]))
        m_data.authorName = authorName;

    if (auto *url = dynamic_objc_cast<NSURL>([dictionary.get() objectForKey:@"url"]))
        m_data.url = url;

    if (auto *matchedString = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"matchedString"]))
        m_data.matchedString = matchedString;

    if (auto *allResults = dynamic_objc_cast<NSArray>([dictionary.get() objectForKey:@"allResults"])) {
        Vector<RetainPtr<DDScannerResult>> result;
        result.reserveInitialCapacity(allResults.count);
        for (id item in allResults) {
            if ([item isKindOfClass:PAL::getDDScannerResultClassSingleton()])
                result.append((DDScannerResult *)item);
        };
        m_data.allResults = WTF::move(result);
    }

    if (auto *groupAllResults = dynamic_objc_cast<NSArray>([dictionary.get() objectForKey:@"groupAllResults"])) {
        Vector<RetainPtr<DDScannerResult>> result;
        result.reserveInitialCapacity(groupAllResults.count);
        for (id item in groupAllResults) {
            if ([item isKindOfClass:PAL::getDDScannerResultClassSingleton()])
                result.append((DDScannerResult *)item);
        };
        m_data.groupAllResults = WTF::move(result);
    }

    if (auto *groupCategory = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"groupCategory"]))
        m_data.groupCategory = groupCategory;

    if (auto *groupTranscript = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"groupTranscript"]))
        m_data.groupTranscript = groupTranscript;

    if (auto *selectionString = dynamic_objc_cast<NSString>([dictionary.get() objectForKey:@"selectionString"]))
        m_data.selectionString = selectionString;

    DDScannerResult *mainResult = [dictionary.get() objectForKey:@"mainResult"];
    if ([mainResult isKindOfClass:PAL::getDDScannerResultClassSingleton()])
        m_data.mainResult = mainResult;

    if (auto *immediate = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"immediate"]))
        m_data.immediate = [immediate boolValue];

    if (auto *isRightClick = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"isRightClick"]))
        m_data.isRightClick = [isRightClick boolValue];

    if (auto *bypassScreentimeContactShield = dynamic_objc_cast<NSNumber>([dictionary.get() objectForKey:@"bypassScreentimeContactShield"]))
        m_data.bypassScreentimeContactShield = [bypassScreentimeContactShield boolValue];

    if (auto *authorNameComponents = dynamic_objc_cast<NSPersonNameComponents>([dictionary.get() objectForKey:@"authorNameComponents"]))
        m_data.authorNameComponents = authorNameComponents;
}

CoreIPCDDSecureActionContext::CoreIPCDDSecureActionContext(CoreIPCDDSecureActionContextData&& data)
: m_data(WTF::move(data)) { }

RetainPtr<id> CoreIPCDDSecureActionContext::toID() const
{
    RetainPtr dict = adoptNS([[NSMutableDictionary alloc] initWithCapacity:23]);

    [dict setObject:[NSValue valueWithRect:m_data.highlightFrame] forKey:@"highlightFrame"];
    [dict setObject:[NSValue valueWithRect:m_data.aimFrame] forKey:@"aimFrame"];

    if (m_data.eventTitle)
        [dict setObject:m_data.eventTitle.get() forKey:@"eventTitle"];
    if (m_data.leadingText)
        [dict setObject:m_data.leadingText.get() forKey:@"leadingText"];
    if (m_data.trailingText)
        [dict setObject:m_data.trailingText.get() forKey:@"trailingText"];
    if (m_data.coreSpotlightUniqueIdentifier)
        [dict setObject:m_data.coreSpotlightUniqueIdentifier.get() forKey:@"coreSpotlightUniqueIdentifier"];
    if (m_data.referenceDate)
        [dict setObject:m_data.referenceDate.get() forKey:@"referenceDate"];
    if (m_data.hostUUID)
        [dict setObject:m_data.hostUUID.get() forKey:@"hostUUID"];
    if (m_data.authorABUUID)
        [dict setObject:m_data.authorABUUID.get() forKey:@"authorABUUID"];
    if (m_data.authorEmailAddress)
        [dict setObject:m_data.authorEmailAddress.get() forKey:@"authorEmailAddress"];
    if (m_data.authorName)
        [dict setObject:m_data.authorName.get() forKey:@"authorName"];
    if (m_data.url)
        [dict setObject:m_data.url.get() forKey:@"url"];
    if (m_data.matchedString)
        [dict setObject:m_data.matchedString.get() forKey:@"matchedString"];
    if (m_data.allResults) {
        RetainPtr arr = [NSMutableArray arrayWithCapacity:m_data.allResults->size()];
        for (auto& element : *m_data.allResults)
            [arr addObject:element.get()];
        [dict setObject:arr.get() forKey:@"allResults"];
    }
    RetainPtr grpArr = [NSMutableArray arrayWithCapacity:m_data.groupAllResults.size()];
    for (auto& element : m_data.groupAllResults)
        [grpArr addObject:element.get()];
    [dict setObject:grpArr.get() forKey:@"groupAllResults"];
    if (m_data.groupCategory)
        [dict setObject:m_data.groupCategory.get() forKey:@"groupCategory"];
    if (m_data.groupTranscript)
        [dict setObject:m_data.groupTranscript.get() forKey:@"groupTranscript"];
    if (m_data.selectionString)
        [dict setObject:m_data.selectionString.get() forKey:@"selectionString"];
    if (m_data.mainResult)
        [dict setObject:m_data.mainResult.get() forKey:@"mainResult"];

    [dict setObject:@(m_data.immediate) forKey:@"immediate"];
    [dict setObject:@(m_data.isRightClick) forKey:@"isRightClick"];
    if (m_data.bypassScreentimeContactShield)
        [dict setObject:@(*m_data.bypassScreentimeContactShield) forKey:@"bypassScreentimeContactShield"];

    if (m_data.authorNameComponents)
        [dict setObject:m_data.authorNameComponents.get() forKey:@"authorNameComponents"];

    return adoptNS([[PAL::getWKDDActionContextClassSingleton() alloc] _initWithWebKitPropertyListData:dict.get()]);
}

}

#endif // PLATFORM(MAC) && HAVE(WK_SECURE_CODING_DATA_DETECTORS)
