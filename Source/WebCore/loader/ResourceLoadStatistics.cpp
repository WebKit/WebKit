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

#include "config.h"
#include "ResourceLoadStatistics.h"

#include "KeyedCoding.h"
#include "PublicSuffix.h"
#include <wtf/MainThread.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

static Seconds timestampResolution { 5_s };

typedef WTF::HashMap<String, unsigned, StringHash, HashTraits<String>, HashTraits<unsigned>>::KeyValuePairType ResourceLoadStatisticsValue;

static void encodeHashCountedSet(KeyedEncoder& encoder, const String& label, const HashCountedSet<String>& hashCountedSet)
{
    if (hashCountedSet.isEmpty())
        return;

    encoder.encodeObjects(label, hashCountedSet.begin(), hashCountedSet.end(), [](KeyedEncoder& encoderInner, const ResourceLoadStatisticsValue& origin) {
        encoderInner.encodeString("origin", origin.key);
        encoderInner.encodeUInt32("count", origin.value);
    });
}

static void encodeHashSet(KeyedEncoder& encoder, const String& label,  const String& key, const HashSet<String>& hashSet)
{
    if (hashSet.isEmpty())
        return;
    
    encoder.encodeObjects(label, hashSet.begin(), hashSet.end(), [&key](KeyedEncoder& encoderInner, const String& origin) {
        encoderInner.encodeString(key, origin);
    });
}

static void encodeOriginHashSet(KeyedEncoder& encoder, const String& label, const HashSet<String>& hashSet)
{
    encodeHashSet(encoder, label, "origin", hashSet);
}

template<typename T>
static void encodeOptionSet(KeyedEncoder& encoder, const String& label, const OptionSet<T>& optionSet)
{
    if (optionSet.isEmpty())
        return;
    
    uint64_t optionSetBitMask = optionSet.toRaw();
    encoder.encodeUInt64(label, optionSetBitMask);
}

#if ENABLE(WEB_API_STATISTICS)
static void encodeFontHashSet(KeyedEncoder& encoder, const String& label, const HashSet<String>& hashSet)
{
    encodeHashSet(encoder, label, "font", hashSet);
}
    
static void encodeCanvasActivityRecord(KeyedEncoder& encoder, const String& label, const CanvasActivityRecord& canvasActivityRecord)
{
    encoder.encodeObject(label, canvasActivityRecord, [] (KeyedEncoder& encoderInner, const CanvasActivityRecord& canvasActivityRecord) {
        encoderInner.encodeBool("wasDataRead", canvasActivityRecord.wasDataRead);
        encoderInner.encodeObjects("textWritten", canvasActivityRecord.textWritten.begin(), canvasActivityRecord.textWritten.end(), [] (KeyedEncoder& encoderInner2, const String& text) {
            encoderInner2.encodeString("text", text);
        });
    });
}
#endif

void ResourceLoadStatistics::encode(KeyedEncoder& encoder) const
{
    encoder.encodeString("PrevalentResourceOrigin", registrableDomain.string());
    
    encoder.encodeDouble("lastSeen", lastSeen.secondsSinceEpoch().value());
    
    // User interaction
    encoder.encodeBool("hadUserInteraction", hadUserInteraction);
    encoder.encodeDouble("mostRecentUserInteraction", mostRecentUserInteractionTime.secondsSinceEpoch().value());
    encoder.encodeBool("grandfathered", grandfathered);

    // Storage access
    encodeOriginHashSet(encoder, "storageAccessUnderTopFrameOrigins", storageAccessUnderTopFrameOrigins);

    // Top frame stats
    encodeHashCountedSet(encoder, "topFrameUniqueRedirectsTo", topFrameUniqueRedirectsTo);
    encodeHashCountedSet(encoder, "topFrameUniqueRedirectsFrom", topFrameUniqueRedirectsFrom);

    // Subframe stats
    encodeHashCountedSet(encoder, "subframeUnderTopFrameOrigins", subframeUnderTopFrameOrigins);
    
    // Subresource stats
    encodeHashCountedSet(encoder, "subresourceUnderTopFrameOrigins", subresourceUnderTopFrameOrigins);
    encodeHashCountedSet(encoder, "subresourceUniqueRedirectsTo", subresourceUniqueRedirectsTo);
    encodeHashCountedSet(encoder, "subresourceUniqueRedirectsFrom", subresourceUniqueRedirectsFrom);

    // Prevalent Resource
    encoder.encodeBool("isPrevalentResource", isPrevalentResource);
    encoder.encodeBool("isVeryPrevalentResource", isVeryPrevalentResource);
    encoder.encodeUInt32("dataRecordsRemoved", dataRecordsRemoved);

    encoder.encodeUInt32("timesAccessedAsFirstPartyDueToUserInteraction", timesAccessedAsFirstPartyDueToUserInteraction);
    encoder.encodeUInt32("timesAccessedAsFirstPartyDueToStorageAccessAPI", timesAccessedAsFirstPartyDueToStorageAccessAPI);

#if ENABLE(WEB_API_STATISTICS)
    encodeFontHashSet(encoder, "fontsFailedToLoad", fontsFailedToLoad);
    encodeFontHashSet(encoder, "fontsSuccessfullyLoaded", fontsSuccessfullyLoaded);
    encodeHashCountedSet(encoder, "topFrameRegistrableDomainsWhichAccessedWebAPIs", topFrameRegistrableDomainsWhichAccessedWebAPIs);
    encodeCanvasActivityRecord(encoder, "canvasActivityRecord", canvasActivityRecord);
    encodeOptionSet(encoder, "navigatorFunctionsAccessedBitMask", navigatorFunctionsAccessed);
    encodeOptionSet(encoder, "screenFunctionsAccessedBitMask", screenFunctionsAccessed);
#endif
}

static void decodeHashCountedSet(KeyedDecoder& decoder, const String& label, HashCountedSet<String>& hashCountedSet)
{
    Vector<String> ignore;
    decoder.decodeObjects(label, ignore, [&hashCountedSet](KeyedDecoder& decoderInner, String& origin) {
        if (!decoderInner.decodeString("origin", origin))
            return false;
        
        unsigned count;
        if (!decoderInner.decodeUInt32("count", count))
            return false;

        hashCountedSet.add(origin, count);
        return true;
    });
}

static void decodeHashSet(KeyedDecoder& decoder, const String& label, const String& key, HashSet<String>& hashSet)
{
    Vector<String> ignore;
    decoder.decodeObjects(label, ignore, [&hashSet, &key](KeyedDecoder& decoderInner, String& origin) {
        if (!decoderInner.decodeString(key, origin))
            return false;
        
        hashSet.add(origin);
        return true;
    });
}

static void decodeOriginHashSet(KeyedDecoder& decoder, const String& label, HashSet<String>& hashSet)
{
    decodeHashSet(decoder, label, "origin", hashSet);
}

template<typename T>
static void decodeOptionSet(KeyedDecoder& decoder, const String& label, OptionSet<T>& optionSet)
{
    uint64_t optionSetBitMask = 0;
    decoder.decodeUInt64(label, optionSetBitMask);
    optionSet = OptionSet<T>::fromRaw(optionSetBitMask);
}

#if ENABLE(WEB_API_STATISTICS)
static void decodeFontHashSet(KeyedDecoder& decoder, const String& label, HashSet<String>& hashSet)
{
    decodeHashSet(decoder, label, "font", hashSet);
}
    
static void decodeCanvasActivityRecord(KeyedDecoder& decoder, const String& label, CanvasActivityRecord& canvasActivityRecord)
{
    decoder.decodeObject(label, canvasActivityRecord, [] (KeyedDecoder& decoderInner, CanvasActivityRecord& canvasActivityRecord) {
        if (!decoderInner.decodeBool("wasDataRead", canvasActivityRecord.wasDataRead))
            return false;
        Vector<String> ignore;
        decoderInner.decodeObjects("textWritten", ignore, [&canvasActivityRecord] (KeyedDecoder& decoderInner2, String& text) {
            if (!decoderInner2.decodeString("text", text))
                return false;
            canvasActivityRecord.textWritten.add(text);
            return true;
        });
        return true;
    });
}
#endif

bool ResourceLoadStatistics::decode(KeyedDecoder& decoder, unsigned modelVersion)
{
    String registrableDomainAsString;
    if (!decoder.decodeString("PrevalentResourceOrigin", registrableDomainAsString))
        return false;
    registrableDomain = RegistrableDomain(registrableDomainAsString);

    // User interaction
    if (!decoder.decodeBool("hadUserInteraction", hadUserInteraction))
        return false;

    // Storage access
    decodeOriginHashSet(decoder, "storageAccessUnderTopFrameOrigins", storageAccessUnderTopFrameOrigins);

    // Top frame stats
    if (modelVersion >= 11) {
        decodeHashCountedSet(decoder, "topFrameUniqueRedirectsTo", topFrameUniqueRedirectsTo);
        decodeHashCountedSet(decoder, "topFrameUniqueRedirectsFrom", topFrameUniqueRedirectsFrom);
    }

    // Subframe stats
    if (modelVersion >= 14)
        decodeHashCountedSet(decoder, "subframeUnderTopFrameOrigins", subframeUnderTopFrameOrigins);

    // Subresource stats
    decodeHashCountedSet(decoder, "subresourceUnderTopFrameOrigins", subresourceUnderTopFrameOrigins);
    decodeHashCountedSet(decoder, "subresourceUniqueRedirectsTo", subresourceUniqueRedirectsTo);
    if (modelVersion >= 11)
        decodeHashCountedSet(decoder, "subresourceUniqueRedirectsFrom", subresourceUniqueRedirectsFrom);

    // Prevalent Resource
    if (!decoder.decodeBool("isPrevalentResource", isPrevalentResource))
        return false;

    if (modelVersion >= 12) {
        if (!decoder.decodeBool("isVeryPrevalentResource", isVeryPrevalentResource))
            return false;
    }

    // Trigger re-classification based on model 14.
    if (modelVersion < 14) {
        isPrevalentResource = false;
        isVeryPrevalentResource = false;
    }

    if (!decoder.decodeUInt32("dataRecordsRemoved", dataRecordsRemoved))
        return false;

    double mostRecentUserInteractionTimeAsDouble;
    if (!decoder.decodeDouble("mostRecentUserInteraction", mostRecentUserInteractionTimeAsDouble))
        return false;
    mostRecentUserInteractionTime = WallTime::fromRawSeconds(mostRecentUserInteractionTimeAsDouble);

    if (!decoder.decodeBool("grandfathered", grandfathered))
        return false;

    double lastSeenTimeAsDouble;
    if (!decoder.decodeDouble("lastSeen", lastSeenTimeAsDouble))
        return false;
    lastSeen = WallTime::fromRawSeconds(lastSeenTimeAsDouble);

    if (modelVersion >= 11) {
        if (!decoder.decodeUInt32("timesAccessedAsFirstPartyDueToUserInteraction", timesAccessedAsFirstPartyDueToUserInteraction))
            timesAccessedAsFirstPartyDueToUserInteraction = 0;
        if (!decoder.decodeUInt32("timesAccessedAsFirstPartyDueToStorageAccessAPI", timesAccessedAsFirstPartyDueToStorageAccessAPI))
            timesAccessedAsFirstPartyDueToStorageAccessAPI = 0;
    }

#if ENABLE(WEB_API_STATISTICS)
    if (modelVersion >= 13) {
        decodeFontHashSet(decoder, "fontsFailedToLoad", fontsFailedToLoad);
        decodeFontHashSet(decoder, "fontsSuccessfullyLoaded", fontsSuccessfullyLoaded);
        decodeHashCountedSet(decoder, "topFrameRegistrableDomainsWhichAccessedWebAPIs", topFrameRegistrableDomainsWhichAccessedWebAPIs);
        decodeCanvasActivityRecord(decoder, "canvasActivityRecord", canvasActivityRecord);
        decodeOptionSet(decoder, "navigatorFunctionsAccessedBitMask", navigatorFunctionsAccessed);
        decodeOptionSet(decoder, "screenFunctionsAccessedBitMask", screenFunctionsAccessed);
    }
#endif

    return true;
}

static void appendBoolean(StringBuilder& builder, const String& label, bool flag)
{
    builder.appendLiteral("    ");
    builder.append(label);
    builder.appendLiteral(": ");
    builder.append(flag ? "Yes" : "No");
}

static void appendHashCountedSet(StringBuilder& builder, const String& label, const HashCountedSet<String>& hashCountedSet)
{
    if (hashCountedSet.isEmpty())
        return;

    builder.appendLiteral("    ");
    builder.append(label);
    builder.appendLiteral(":\n");

    for (auto& entry : hashCountedSet) {
        builder.appendLiteral("        ");
        builder.append(entry.key);
        builder.appendLiteral(": ");
        builder.appendNumber(entry.value);
        builder.append('\n');
    }
}

static void appendHashSet(StringBuilder& builder, const String& label, const HashSet<String>& hashSet)
{
    if (hashSet.isEmpty())
        return;
    
    builder.appendLiteral("    ");
    builder.append(label);
    builder.appendLiteral(":\n");
    
    for (auto& entry : hashSet) {
        builder.appendLiteral("        ");
        builder.append(entry);
        builder.append('\n');
    }
}

#if ENABLE(WEB_API_STATISTICS)
static ASCIILiteral navigatorAPIEnumToString(ResourceLoadStatistics::NavigatorAPI navigatorEnum)
{
    switch (navigatorEnum) {
    case ResourceLoadStatistics::NavigatorAPI::JavaEnabled:
        return "javaEnabled"_s;
    case ResourceLoadStatistics::NavigatorAPI::MimeTypes:
        return "mimeTypes"_s;
    case ResourceLoadStatistics::NavigatorAPI::CookieEnabled:
        return "cookieEnabled"_s;
    case ResourceLoadStatistics::NavigatorAPI::Plugins:
        return "plugins"_s;
    case ResourceLoadStatistics::NavigatorAPI::UserAgent:
        return "userAgent"_s;
    case ResourceLoadStatistics::NavigatorAPI::AppVersion:
        return "appVersion"_s;
    }
    ASSERT_NOT_REACHED();
    return "Invalid navigator API"_s;
}

static ASCIILiteral screenAPIEnumToString(ResourceLoadStatistics::ScreenAPI screenEnum)
{
    switch (screenEnum) {
    case ResourceLoadStatistics::ScreenAPI::Height:
        return "height"_s;
    case ResourceLoadStatistics::ScreenAPI::Width:
        return "width"_s;
    case ResourceLoadStatistics::ScreenAPI::ColorDepth:
        return "colorDepth"_s;
    case ResourceLoadStatistics::ScreenAPI::PixelDepth:
        return "pixelDepth"_s;
    case ResourceLoadStatistics::ScreenAPI::AvailLeft:
        return "availLeft"_s;
    case ResourceLoadStatistics::ScreenAPI::AvailTop:
        return "availTop"_s;
    case ResourceLoadStatistics::ScreenAPI::AvailHeight:
        return "availHeight"_s;
    case ResourceLoadStatistics::ScreenAPI::AvailWidth:
        return "availWidth"_s;
    }
    ASSERT_NOT_REACHED();
    return "Invalid screen API"_s;
}
    
static void appendNavigatorAPIOptionSet(StringBuilder& builder, const OptionSet<ResourceLoadStatistics::NavigatorAPI>& optionSet)
{
    if (optionSet.isEmpty())
        return;
    builder.appendLiteral("    navigatorFunctionsAccessed:\n");
    for (auto navigatorAPI : optionSet) {
        builder.appendLiteral("        ");
        builder.append(navigatorAPIEnumToString(navigatorAPI).characters());
        builder.append('\n');
    }
}
    
static void appendScreenAPIOptionSet(StringBuilder& builder, const OptionSet<ResourceLoadStatistics::ScreenAPI>& optionSet)
{
    if (optionSet.isEmpty())
        return;
    builder.appendLiteral("    screenFunctionsAccessed:\n");
    for (auto screenAPI : optionSet) {
        builder.appendLiteral("        ");
        builder.append(screenAPIEnumToString(screenAPI).characters());
        builder.append('\n');
    }
}
#endif

String ResourceLoadStatistics::toString() const
{
    StringBuilder builder;
    builder.appendLiteral("High level domain: ");
    builder.append(registrableDomain.string());
    builder.append('\n');
    builder.appendLiteral("    lastSeen: ");
    builder.appendNumber(lastSeen.secondsSinceEpoch().value());
    builder.append('\n');
    
    // User interaction
    appendBoolean(builder, "hadUserInteraction", hadUserInteraction);
    builder.append('\n');
    builder.appendLiteral("    mostRecentUserInteraction: ");
    builder.appendNumber(mostRecentUserInteractionTime.secondsSinceEpoch().value());
    builder.append('\n');
    appendBoolean(builder, "grandfathered", grandfathered);
    builder.append('\n');

    // Storage access
    appendHashSet(builder, "storageAccessUnderTopFrameOrigins", storageAccessUnderTopFrameOrigins);

    // Top frame stats
    appendHashCountedSet(builder, "topFrameUniqueRedirectsTo", topFrameUniqueRedirectsTo);
    appendHashCountedSet(builder, "topFrameUniqueRedirectsFrom", topFrameUniqueRedirectsFrom);

    // Subframe stats
    appendHashCountedSet(builder, "subframeUnderTopFrameOrigins", subframeUnderTopFrameOrigins);
    
    // Subresource stats
    appendHashCountedSet(builder, "subresourceUnderTopFrameOrigins", subresourceUnderTopFrameOrigins);
    appendHashCountedSet(builder, "subresourceUniqueRedirectsTo", subresourceUniqueRedirectsTo);
    appendHashCountedSet(builder, "subresourceUniqueRedirectsFrom", subresourceUniqueRedirectsFrom);

    // Prevalent Resource
    appendBoolean(builder, "isPrevalentResource", isPrevalentResource);
    builder.append('\n');
    appendBoolean(builder, "isVeryPrevalentResource", isVeryPrevalentResource);
    builder.append('\n');
    builder.appendLiteral("    dataRecordsRemoved: ");
    builder.appendNumber(dataRecordsRemoved);
    builder.append('\n');

#if ENABLE(WEB_API_STATISTICS)
    appendHashSet(builder, "fontsFailedToLoad", fontsFailedToLoad);
    appendHashSet(builder, "fontsSuccessfullyLoaded", fontsSuccessfullyLoaded);
    appendHashCountedSet(builder, "topFrameRegistrableDomainsWhichAccessedWebAPIs", topFrameRegistrableDomainsWhichAccessedWebAPIs);
    appendNavigatorAPIOptionSet(builder, navigatorFunctionsAccessed);
    appendScreenAPIOptionSet(builder, screenFunctionsAccessed);
    appendHashSet(builder, "canvasTextWritten", canvasActivityRecord.textWritten);
    appendBoolean(builder, "canvasReadData", canvasActivityRecord.wasDataRead);
    builder.append('\n');
    builder.append('\n');
#endif

    return builder.toString();
}

template <typename T>
static void mergeHashCountedSet(HashCountedSet<T>& to, const HashCountedSet<T>& from)
{
    for (auto& entry : from)
        to.add(entry.key, entry.value);
}

template <typename T>
static void mergeHashSet(HashSet<T>& to, const HashSet<T>& from)
{
    for (auto& entry : from)
        to.add(entry);
}

void ResourceLoadStatistics::merge(const ResourceLoadStatistics& other)
{
    ASSERT(other.registrableDomain == registrableDomain);

    if (lastSeen < other.lastSeen)
        lastSeen = other.lastSeen;
    
    if (!other.hadUserInteraction) {
        // If user interaction has been reset do so here too.
        // Else, do nothing.
        if (!other.mostRecentUserInteractionTime) {
            hadUserInteraction = false;
            mostRecentUserInteractionTime = { };
        }
    } else {
        hadUserInteraction = true;
        if (mostRecentUserInteractionTime < other.mostRecentUserInteractionTime)
            mostRecentUserInteractionTime = other.mostRecentUserInteractionTime;
    }
    grandfathered |= other.grandfathered;

    // Storage access
    mergeHashSet(storageAccessUnderTopFrameOrigins, other.storageAccessUnderTopFrameOrigins);

    // Top frame stats
    mergeHashCountedSet(topFrameUniqueRedirectsTo, other.topFrameUniqueRedirectsTo);
    mergeHashCountedSet(topFrameUniqueRedirectsFrom, other.topFrameUniqueRedirectsFrom);

    // Subframe stats
    mergeHashCountedSet(subframeUnderTopFrameOrigins, other.subframeUnderTopFrameOrigins);
    
    // Subresource stats
    mergeHashCountedSet(subresourceUnderTopFrameOrigins, other.subresourceUnderTopFrameOrigins);
    mergeHashCountedSet(subresourceUniqueRedirectsTo, other.subresourceUniqueRedirectsTo);
    mergeHashCountedSet(subresourceUniqueRedirectsFrom, other.subresourceUniqueRedirectsFrom);

    // Prevalent resource stats
    isPrevalentResource |= other.isPrevalentResource;
    isVeryPrevalentResource |= other.isVeryPrevalentResource;
    dataRecordsRemoved = std::max(dataRecordsRemoved, other.dataRecordsRemoved);
    
#if ENABLE(WEB_API_STATISTICS)
    mergeHashSet(fontsFailedToLoad, other.fontsFailedToLoad);
    mergeHashSet(fontsSuccessfullyLoaded, other.fontsSuccessfullyLoaded);
    mergeHashCountedSet(topFrameRegistrableDomainsWhichAccessedWebAPIs, other.topFrameRegistrableDomainsWhichAccessedWebAPIs);
    canvasActivityRecord.mergeWith(other.canvasActivityRecord);
    navigatorFunctionsAccessed.add(other.navigatorFunctionsAccessed);
    screenFunctionsAccessed.add(other.screenFunctionsAccessed);
#endif
}

WallTime ResourceLoadStatistics::reduceTimeResolution(WallTime time)
{
    return WallTime::fromRawSeconds(std::floor(time.secondsSinceEpoch() / timestampResolution) * timestampResolution.seconds());
}

}
