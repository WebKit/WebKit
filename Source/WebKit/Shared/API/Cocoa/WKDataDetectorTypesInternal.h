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

static inline OptionSet<WebCore::DataDetectorType> fromWKDataDetectorTypes(WKDataDetectorTypes types)
{
    OptionSet<WebCore::DataDetectorType> result;
    if (types & WKDataDetectorTypePhoneNumber)
        result.add(WebCore::DataDetectorType::PhoneNumber);
    if (types & WKDataDetectorTypeLink)
        result.add(WebCore::DataDetectorType::Link);
    if (types & WKDataDetectorTypeAddress)
        result.add(WebCore::DataDetectorType::Address);
    if (types & WKDataDetectorTypeCalendarEvent)
        result.add(WebCore::DataDetectorType::CalendarEvent);
    if (types & WKDataDetectorTypeTrackingNumber)
        result.add(WebCore::DataDetectorType::TrackingNumber);
    if (types & WKDataDetectorTypeFlightNumber)
        result.add(WebCore::DataDetectorType::FlightNumber);
    if (types & WKDataDetectorTypeLookupSuggestion)
        result.add(WebCore::DataDetectorType::LookupSuggestion);

    return result;
}

#endif
