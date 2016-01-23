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

#ifndef DataDetection_h
#define DataDetection_h

#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>

OBJC_CLASS DDActionContext;
OBJC_CLASS NSArray;

namespace WebCore {

class FloatRect;
class HitTestResult;
class Range;

enum DataDetectorTypes {
    DataDetectorTypeNone = 0,
    DataDetectorTypePhoneNumber = 1 << 0,
    DataDetectorTypeLink = 1 << 1,
    DataDetectorTypeAddress = 1 << 2,
    DataDetectorTypeCalendarEvent = 1 << 3,
    DataDetectorTypeTrackingNumber = 1 << 4, // Not individually selectable with the API
    DataDetectorTypeFlight = 1 << 5, // Not individually selectable with the API
    DataDetectorTypeAll = ULONG_MAX
};

class DataDetection {
public:
#if PLATFORM(MAC)
    WEBCORE_EXPORT static RetainPtr<DDActionContext> detectItemAroundHitTestResult(const HitTestResult&, FloatRect& detectedDataBoundingBox, RefPtr<Range>& detectedDataRange);
#endif
    WEBCORE_EXPORT static NSArray *detectContentInRange(RefPtr<Range>& contextRange, DataDetectorTypes);
};

} // namespace WebCore

#endif // DataDetection_h
