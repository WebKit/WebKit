/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "WKDataDetectorTypes.h"

#if PLATFORM(IOS_FAMILY)

#import <WebCore/DataDetection.h>

static inline WebCore::DataDetectorTypes fromWKDataDetectorTypes(uint64_t types)
{
    if (static_cast<WKDataDetectorTypes>(types) == WKDataDetectorTypeNone)
        return WebCore::DataDetectorTypeNone;
    if (static_cast<WKDataDetectorTypes>(types) == WKDataDetectorTypeAll)
        return WebCore::DataDetectorTypeAll;

    uint32_t value = WebCore::DataDetectorTypeNone;
    if (types & WKDataDetectorTypePhoneNumber)
        value |= WebCore::DataDetectorTypePhoneNumber;
    if (types & WKDataDetectorTypeLink)
        value |= WebCore::DataDetectorTypeLink;
    if (types & WKDataDetectorTypeAddress)
        value |= WebCore::DataDetectorTypeAddress;
    if (types & WKDataDetectorTypeCalendarEvent)
        value |= WebCore::DataDetectorTypeCalendarEvent;
    if (types & WKDataDetectorTypeTrackingNumber)
        value |= WebCore::DataDetectorTypeTrackingNumber;
    if (types & WKDataDetectorTypeFlightNumber)
        value |= WebCore::DataDetectorTypeFlightNumber;
    if (types & WKDataDetectorTypeLookupSuggestion)
        value |= WebCore::DataDetectorTypeLookupSuggestion;

    return static_cast<WebCore::DataDetectorTypes>(value);
}

#endif
