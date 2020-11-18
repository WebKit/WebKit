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

#import <WebCore/DataDetectorType.h>

static inline WebCore::DataDetectorType fromWKDataDetectorTypes(uint64_t types)
{
    uint32_t value = 0;
    if (types & WKDataDetectorTypePhoneNumber)
        value |= static_cast<uint32_t>(WebCore::DataDetectorType::PhoneNumber);
    if (types & WKDataDetectorTypeLink)
        value |= static_cast<uint32_t>(WebCore::DataDetectorType::Link);
    if (types & WKDataDetectorTypeAddress)
        value |= static_cast<uint32_t>(WebCore::DataDetectorType::Address);
    if (types & WKDataDetectorTypeCalendarEvent)
        value |= static_cast<uint32_t>(WebCore::DataDetectorType::CalendarEvent);
    if (types & WKDataDetectorTypeTrackingNumber)
        value |= static_cast<uint32_t>(WebCore::DataDetectorType::TrackingNumber);
    if (types & WKDataDetectorTypeFlightNumber)
        value |= static_cast<uint32_t>(WebCore::DataDetectorType::FlightNumber);
    if (types & WKDataDetectorTypeLookupSuggestion)
        value |= static_cast<uint32_t>(WebCore::DataDetectorType::LookupSuggestion);
    return static_cast<WebCore::DataDetectorType>(value);
}

#endif
