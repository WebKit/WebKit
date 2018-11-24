/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

OBJC_CLASS DDActionContext;
OBJC_CLASS NSArray;
OBJC_CLASS NSDictionary;

namespace WebCore {

class Document;
class Element;
class FloatRect;
class HitTestResult;
class Range;
class URL;

enum DataDetectorTypes {
    DataDetectorTypeNone = 0,
    DataDetectorTypePhoneNumber = 1 << 0,
    DataDetectorTypeLink = 1 << 1,
    DataDetectorTypeAddress = 1 << 2,
    DataDetectorTypeCalendarEvent = 1 << 3,
    DataDetectorTypeTrackingNumber = 1 << 4,
    DataDetectorTypeFlightNumber = 1 << 5,
    DataDetectorTypeLookupSuggestion = 1 << 6,
    DataDetectorTypeAll = ULONG_MAX
};

class DataDetection {
public:
#if PLATFORM(MAC)
    WEBCORE_EXPORT static RetainPtr<DDActionContext> detectItemAroundHitTestResult(const HitTestResult&, FloatRect& detectedDataBoundingBox, RefPtr<Range>& detectedDataRange);
#endif
    WEBCORE_EXPORT static NSArray *detectContentInRange(RefPtr<Range>& contextRange, DataDetectorTypes, NSDictionary *context);
    WEBCORE_EXPORT static void removeDataDetectedLinksInDocument(Document&);
#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT static bool canBePresentedByDataDetectors(const URL&);
    WEBCORE_EXPORT static bool isDataDetectorLink(Element&);
    WEBCORE_EXPORT static String dataDetectorIdentifier(Element&);
    WEBCORE_EXPORT static bool shouldCancelDefaultAction(Element&);
    WEBCORE_EXPORT static bool requiresExtendedContext(Element&);
#endif

    static const String& dataDetectorURLProtocol();
    static bool isDataDetectorURL(const URL&);
};

} // namespace WebCore

#endif

