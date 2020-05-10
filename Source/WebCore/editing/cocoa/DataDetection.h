/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(DATA_DETECTION)

#import "FloatRect.h"
#import "SimpleRange.h"
#import <wtf/RetainPtr.h>

OBJC_CLASS DDActionContext;
OBJC_CLASS NSArray;
OBJC_CLASS NSDictionary;

namespace WebCore {

class HitTestResult;

// FIXME: Would be better to use an OptionSet and uint8_t.
enum class DataDetectorTypes : uint32_t {
    PhoneNumber = 1 << 0,
    Link = 1 << 1,
    Address = 1 << 2,
    CalendarEvent = 1 << 3,
    TrackingNumber = 1 << 4,
    FlightNumber = 1 << 5,
    LookupSuggestion = 1 << 6,
};

struct DetectedItem {
    RetainPtr<DDActionContext> actionContext;
    FloatRect boundingBox;
    SimpleRange range;
};

class DataDetection {
public:
#if PLATFORM(MAC)
    WEBCORE_EXPORT static Optional<DetectedItem> detectItemAroundHitTestResult(const HitTestResult&);
#endif
    WEBCORE_EXPORT static NSArray *detectContentInRange(const SimpleRange&, DataDetectorTypes, NSDictionary *context);
    WEBCORE_EXPORT static void removeDataDetectedLinksInDocument(Document&);
#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT static bool canBePresentedByDataDetectors(const URL&);
    WEBCORE_EXPORT static bool isDataDetectorLink(Element&);
    WEBCORE_EXPORT static String dataDetectorIdentifier(Element&);
    WEBCORE_EXPORT static bool canPresentDataDetectorsUIForElement(Element&);
    WEBCORE_EXPORT static bool requiresExtendedContext(Element&);
#endif

    static const String& dataDetectorURLProtocol();
    static bool isDataDetectorURL(const URL&);
};

} // namespace WebCore

#endif
