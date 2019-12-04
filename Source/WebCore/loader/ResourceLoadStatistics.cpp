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

typedef WTF::HashMap<RegistrableDomain, unsigned, RegistrableDomain::RegistrableDomainHash, HashTraits<RegistrableDomain>, HashTraits<unsigned>>::KeyValuePairType ResourceLoadStatisticsValue;

static void encodeHashSet(KeyedEncoder& encoder, const String& label,  const String& key, const HashSet<RegistrableDomain>& hashSet)
{
    if (hashSet.isEmpty())
        return;
    
    encoder.encodeObjects(label, hashSet.begin(), hashSet.end(), [&key](KeyedEncoder& encoderInner, const RegistrableDomain& domain) {
        encoderInner.encodeString(key, domain.string());
    });
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
    encoder.encodeString("PrevalentResourceDomain"_s, registrableDomain.string());

    encoder.encodeDouble("lastSeen"_s, lastSeen.secondsSinceEpoch().value());

    // User interaction
    encoder.encodeBool("hadUserInteraction"_s, hadUserInteraction);
    encoder.encodeDouble("mostRecentUserInteraction"_s, mostRecentUserInteractionTime.secondsSinceEpoch().value());
    encoder.encodeBool("grandfathered"_s, grandfathered);

    // Storage access
    encodeHashSet(encoder, "storageAccessUnderTopFrameDomains"_s, "domain"_s, storageAccessUnderTopFrameDomains);

    // Top frame stats
    encodeHashSet(encoder, "topFrameUniqueRedirectsTo"_s, "domain"_s, topFrameUniqueRedirectsTo);
    encodeHashSet(encoder, "topFrameUniqueRedirectsFrom"_s, "domain"_s, topFrameUniqueRedirectsFrom);
    encodeHashSet(encoder, "topFrameLinkDecorationsFrom"_s, "domain", topFrameLinkDecorationsFrom);
    encoder.encodeBool("gotLinkDecorationFromPrevalentResource"_s, gotLinkDecorationFromPrevalentResource);
    encodeHashSet(encoder, "topFrameLoadedThirdPartyScripts"_s, "domain", topFrameLoadedThirdPartyScripts);

    // Subframe stats
    encodeHashSet(encoder, "subframeUnderTopFrameDomains"_s, "domain"_s, subframeUnderTopFrameDomains);
    
    // Subresource stats
    encodeHashSet(encoder, "subresourceUnderTopFrameDomains"_s, "domain"_s, subresourceUnderTopFrameDomains);
    encodeHashSet(encoder, "subresourceUniqueRedirectsTo"_s, "domain"_s, subresourceUniqueRedirectsTo);
    encodeHashSet(encoder, "subresourceUniqueRedirectsFrom"_s, "domain"_s, subresourceUniqueRedirectsFrom);

    // Prevalent Resource
    encoder.encodeBool("isPrevalentResource"_s, isPrevalentResource);
    encoder.encodeBool("isVeryPrevalentResource"_s, isVeryPrevalentResource);
    encoder.encodeUInt32("dataRecordsRemoved"_s, dataRecordsRemoved);

    encoder.encodeUInt32("timesAccessedAsFirstPartyDueToUserInteraction"_s, timesAccessedAsFirstPartyDueToUserInteraction);
    encoder.encodeUInt32("timesAccessedAsFirstPartyDueToStorageAccessAPI"_s, timesAccessedAsFirstPartyDueToStorageAccessAPI);

#if ENABLE(WEB_API_STATISTICS)
    encodeFontHashSet(encoder, "fontsFailedToLoad", fontsFailedToLoad);
    encodeFontHashSet(encoder, "fontsSuccessfullyLoaded", fontsSuccessfullyLoaded);
    encodeHashCountedSet(encoder, "topFrameRegistrableDomainsWhichAccessedWebAPIs", topFrameRegistrableDomainsWhichAccessedWebAPIs);
    encodeCanvasActivityRecord(encoder, "canvasActivityRecord", canvasActivityRecord);
    encodeOptionSet(encoder, "navigatorFunctionsAccessedBitMask", navigatorFunctionsAccessed);
    encodeOptionSet(encoder, "screenFunctionsAccessedBitMask", screenFunctionsAccessed);
#endif
}

static void decodeHashCountedSet(KeyedDecoder& decoder, const String& label, HashCountedSet<RegistrableDomain>& hashCountedSet)
{
    Vector<String> ignore;
    decoder.decodeObjects(label, ignore, [&hashCountedSet](KeyedDecoder& decoderInner, String& domain) {
        if (!decoderInner.decodeString("origin", domain))
            return false;
        
        unsigned count;
        if (!decoderInner.decodeUInt32("count", count))
            return false;

        hashCountedSet.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(domain), count);
        return true;
    });
}

static void decodeHashSet(KeyedDecoder& decoder, const String& label, const String& key, HashSet<RegistrableDomain>& hashSet)
{
    Vector<String> ignore;
    decoder.decodeObjects(label, ignore, [&hashSet, &key](KeyedDecoder& decoderInner, String& domain) {
        if (!decoderInner.decodeString(key, domain))
            return false;
        
        hashSet.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString(domain));
        return true;
    });
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
    if (modelVersion >= 15) {
        if (!decoder.decodeString("PrevalentResourceDomain", registrableDomainAsString))
            return false;
    } else {
        if (!decoder.decodeString("PrevalentResourceOrigin", registrableDomainAsString))
            return false;
    }
    registrableDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString(registrableDomainAsString);

    // User interaction
    if (!decoder.decodeBool("hadUserInteraction", hadUserInteraction))
        return false;

    // Storage access
    if (modelVersion >= 15)
        decodeHashSet(decoder, "storageAccessUnderTopFrameDomains", "domain", storageAccessUnderTopFrameDomains);
    else
        decodeHashSet(decoder, "storageAccessUnderTopFrameOrigins", "origin", storageAccessUnderTopFrameDomains);

    // Top frame stats
    if (modelVersion >= 15) {
        decodeHashSet(decoder, "topFrameUniqueRedirectsTo", "domain", topFrameUniqueRedirectsTo);
        decodeHashSet(decoder, "topFrameUniqueRedirectsFrom", "domain", topFrameUniqueRedirectsFrom);
    } else if (modelVersion >= 11) {
        HashCountedSet<RegistrableDomain> topFrameUniqueRedirectsToCounted;
        decodeHashCountedSet(decoder, "topFrameUniqueRedirectsTo", topFrameUniqueRedirectsToCounted);
        for (auto& domain : topFrameUniqueRedirectsToCounted.values())
            topFrameUniqueRedirectsTo.add(domain);
        
        HashCountedSet<RegistrableDomain> topFrameUniqueRedirectsFromCounted;
        decodeHashCountedSet(decoder, "topFrameUniqueRedirectsFrom", topFrameUniqueRedirectsFromCounted);
        for (auto& domain : topFrameUniqueRedirectsFromCounted.values())
            topFrameUniqueRedirectsFrom.add(domain);
    }

    if (modelVersion >= 16) {
        decodeHashSet(decoder, "topFrameLinkDecorationsFrom", "domain", topFrameLinkDecorationsFrom);
        if (!decoder.decodeBool("gotLinkDecorationFromPrevalentResource", gotLinkDecorationFromPrevalentResource))
            return false;
    }

    if (modelVersion >= 17) {
        HashCountedSet<RegistrableDomain> topFrameLoadedThirdPartyScriptsCounted;
        decodeHashCountedSet(decoder, "topFrameLoadedThirdPartyScripts", topFrameLoadedThirdPartyScriptsCounted);
        for (auto& domain : topFrameLoadedThirdPartyScriptsCounted.values())
            topFrameLoadedThirdPartyScripts.add(domain);
    }

    // Subframe stats
    if (modelVersion >= 15)
        decodeHashSet(decoder, "subframeUnderTopFrameDomains", "domain", subframeUnderTopFrameDomains);
    else if (modelVersion >= 14) {
        HashCountedSet<RegistrableDomain> subframeUnderTopFrameDomainsCounted;
        decodeHashCountedSet(decoder, "subframeUnderTopFrameOrigins", subframeUnderTopFrameDomainsCounted);
        for (auto& domain : subframeUnderTopFrameDomainsCounted.values())
            subframeUnderTopFrameDomains.add(domain);
    }

    // Subresource stats
    if (modelVersion >= 15) {
        decodeHashSet(decoder, "subresourceUnderTopFrameDomains", "domain", subresourceUnderTopFrameDomains);
        decodeHashSet(decoder, "subresourceUniqueRedirectsTo", "domain", subresourceUniqueRedirectsTo);
        decodeHashSet(decoder, "subresourceUniqueRedirectsFrom", "domain", subresourceUniqueRedirectsFrom);
    } else {
        HashCountedSet<RegistrableDomain> subresourceUnderTopFrameDomainsCounted;
        decodeHashCountedSet(decoder, "subresourceUnderTopFrameOrigins", subresourceUnderTopFrameDomainsCounted);
        for (auto& domain : subresourceUnderTopFrameDomainsCounted.values())
            subresourceUnderTopFrameDomains.add(domain);

        HashCountedSet<RegistrableDomain> subresourceUniqueRedirectsToCounted;
        decodeHashCountedSet(decoder, "subresourceUniqueRedirectsTo", subresourceUniqueRedirectsToCounted);
        for (auto& domain : subresourceUniqueRedirectsToCounted.values())
            subresourceUniqueRedirectsTo.add(domain);
        if (modelVersion >= 11) {
            HashCountedSet<RegistrableDomain> subresourceUniqueRedirectsFromCounted;
            decodeHashCountedSet(decoder, "subresourceUniqueRedirectsFrom", subresourceUniqueRedirectsFromCounted);
            for (auto& domain : subresourceUniqueRedirectsFromCounted.values())
                subresourceUniqueRedirectsFrom.add(domain);
        }
    }


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

static void appendHashSet(StringBuilder& builder, const String& label, const HashSet<RegistrableDomain>& hashSet)
{
    if (hashSet.isEmpty())
        return;
    
    builder.appendLiteral("    ");
    builder.append(label);
    builder.appendLiteral(":\n");
    
    for (auto& entry : hashSet) {
        builder.appendLiteral("        ");
        builder.append(entry.string());
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

static bool hasHadRecentUserInteraction(WTF::Seconds interactionTimeSeconds)
{
    return interactionTimeSeconds > Seconds(0) && WallTime::now().secondsSinceEpoch() - interactionTimeSeconds < 24_h;
}

String ResourceLoadStatistics::toString() const
{
    StringBuilder builder;
    builder.appendLiteral("Registrable domain: ");
    builder.append(registrableDomain.string());
    builder.append('\n');

    // User interaction
    appendBoolean(builder, "hadUserInteraction", hadUserInteraction);
    builder.append('\n');
    builder.appendLiteral("    mostRecentUserInteraction: ");
    if (hasHadRecentUserInteraction(mostRecentUserInteractionTime.secondsSinceEpoch()))
        builder.appendLiteral("within 24 hours");
    else
        builder.appendLiteral("-1");
    builder.append('\n');
    appendBoolean(builder, "grandfathered", grandfathered);
    builder.append('\n');

    // Storage access
    appendHashSet(builder, "storageAccessUnderTopFrameDomains", storageAccessUnderTopFrameDomains);

    // Top frame stats
    appendHashSet(builder, "topFrameUniqueRedirectsTo", topFrameUniqueRedirectsTo);
    appendHashSet(builder, "topFrameUniqueRedirectsFrom", topFrameUniqueRedirectsFrom);
    appendHashSet(builder, "topFrameLinkDecorationsFrom", topFrameLinkDecorationsFrom);
    appendBoolean(builder, "gotLinkDecorationFromPrevalentResource", gotLinkDecorationFromPrevalentResource);
    builder.append('\n');
    appendHashSet(builder, "topFrameLoadedThirdPartyScripts", topFrameLoadedThirdPartyScripts);

    // Subframe stats
    appendHashSet(builder, "subframeUnderTopFrameDomains", subframeUnderTopFrameDomains);
    
    // Subresource stats
    appendHashSet(builder, "subresourceUnderTopFrameDomains", subresourceUnderTopFrameDomains);
    appendHashSet(builder, "subresourceUniqueRedirectsTo", subresourceUniqueRedirectsTo);
    appendHashSet(builder, "subresourceUniqueRedirectsFrom", subresourceUniqueRedirectsFrom);

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
    mergeHashSet(storageAccessUnderTopFrameDomains, other.storageAccessUnderTopFrameDomains);

    // Top frame stats
    mergeHashSet(topFrameUniqueRedirectsTo, other.topFrameUniqueRedirectsTo);
    mergeHashSet(topFrameUniqueRedirectsFrom, other.topFrameUniqueRedirectsFrom);
    mergeHashSet(topFrameLinkDecorationsFrom, other.topFrameLinkDecorationsFrom);
    gotLinkDecorationFromPrevalentResource |= other.gotLinkDecorationFromPrevalentResource;
    mergeHashSet(topFrameLoadedThirdPartyScripts, other.topFrameLoadedThirdPartyScripts);

    // Subframe stats
    mergeHashSet(subframeUnderTopFrameDomains, other.subframeUnderTopFrameDomains);
    
    // Subresource stats
    mergeHashSet(subresourceUnderTopFrameDomains, other.subresourceUnderTopFrameDomains);
    mergeHashSet(subresourceUniqueRedirectsTo, other.subresourceUniqueRedirectsTo);
    mergeHashSet(subresourceUniqueRedirectsFrom, other.subresourceUniqueRedirectsFrom);

    // Prevalent resource stats
    isPrevalentResource |= other.isPrevalentResource;
    isVeryPrevalentResource |= other.isVeryPrevalentResource;
    dataRecordsRemoved = std::max(dataRecordsRemoved, other.dataRecordsRemoved);
    
#if ENABLE(WEB_API_STATISTICS)
    mergeHashSet(fontsFailedToLoad, other.fontsFailedToLoad);
    mergeHashSet(fontsSuccessfullyLoaded, other.fontsSuccessfullyLoaded);
    mergeHashSet(topFrameRegistrableDomainsWhichAccessedWebAPIs, other.topFrameRegistrableDomainsWhichAccessedWebAPIs);
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
