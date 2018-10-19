/*
 * Copyright (C) 2016-2018 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CanvasActivityRecord.h"
#include "URL.h"
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/WallTime.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class KeyedDecoder;
class KeyedEncoder;

struct ResourceLoadStatistics {
    explicit ResourceLoadStatistics(const String& primaryDomain)
        : highLevelDomain(primaryDomain)
    {
    }

    ResourceLoadStatistics() = default;

    ResourceLoadStatistics(const ResourceLoadStatistics&) = delete;
    ResourceLoadStatistics& operator=(const ResourceLoadStatistics&) = delete;
    ResourceLoadStatistics(ResourceLoadStatistics&&) = default;
    ResourceLoadStatistics& operator=(ResourceLoadStatistics&&) = default;

    WEBCORE_EXPORT static String primaryDomain(const URL&);
    WEBCORE_EXPORT static String primaryDomain(StringView host);

    WEBCORE_EXPORT static WallTime reduceTimeResolution(WallTime);

    WEBCORE_EXPORT void encode(KeyedEncoder&) const;
    WEBCORE_EXPORT bool decode(KeyedDecoder&, unsigned modelVersion);

    WEBCORE_EXPORT String toString() const;

    WEBCORE_EXPORT void merge(const ResourceLoadStatistics&);

    String highLevelDomain;

    WallTime lastSeen;
    
    // User interaction
    bool hadUserInteraction { false };
    // Timestamp. Default value is negative, 0 means it was reset.
    WallTime mostRecentUserInteractionTime { WallTime::fromRawSeconds(-1) };
    bool grandfathered { false };

    // Storage access
    HashSet<String> storageAccessUnderTopFrameOrigins;

    // Top frame stats
    HashCountedSet<String> topFrameUniqueRedirectsTo;
    HashCountedSet<String> topFrameUniqueRedirectsFrom;

    // Subframe stats
    HashCountedSet<String> subframeUnderTopFrameOrigins;
    
    // Subresource stats
    HashCountedSet<String> subresourceUnderTopFrameOrigins;
    HashCountedSet<String> subresourceUniqueRedirectsTo;
    HashCountedSet<String> subresourceUniqueRedirectsFrom;

    // Prevalent resource stats
    bool isPrevalentResource { false };
    bool isVeryPrevalentResource { false };
    unsigned dataRecordsRemoved { 0 };
    unsigned timesAccessedAsFirstPartyDueToUserInteraction { 0 };
    unsigned timesAccessedAsFirstPartyDueToStorageAccessAPI { 0 };

    // This set represents the registrable domain of the top frame where web API
    // were used in the top frame or one of its subframes.
    HashCountedSet<String> topFrameRegistrableDomainsWhichAccessedWebAPIs;
    HashSet<String> fontsFailedToLoad;
    HashSet<String> fontsSuccessfullyLoaded;
    CanvasActivityRecord canvasActivityRecord;
    enum class NavigatorAPI : uint64_t {
        AppVersion = 1 << 0,
        UserAgent = 1 << 1,
        Plugins = 1 << 2,
        MimeTypes = 1 << 3,
        CookieEnabled = 1 << 4,
        JavaEnabled = 1 << 5,
    };
    enum class ScreenAPI : uint64_t {
        Height = 1 << 0,
        Width = 1 << 1,
        ColorDepth = 1 << 2,
        PixelDepth = 1 << 3,
        AvailLeft = 1 << 4,
        AvailTop = 1 << 5,
        AvailHeight = 1 << 6,
        AvailWidth = 1 << 7,
    };
    OptionSet<NavigatorAPI> navigatorFunctionsAccessed;
    OptionSet<ScreenAPI> screenFunctionsAccessed;
};

} // namespace WebCore
