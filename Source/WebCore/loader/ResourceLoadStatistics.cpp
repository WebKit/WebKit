/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

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

void ResourceLoadStatistics::encode(KeyedEncoder& encoder) const
{
    encoder.encodeString("PrevalentResourceOrigin", highLevelDomain);
    
    // User interaction
    encoder.encodeBool("hadUserInteraction", hadUserInteraction);
    
    // Top frame stats
    encoder.encodeBool("topFrameHasBeenNavigatedToBefore", topFrameHasBeenNavigatedToBefore);
    encoder.encodeUInt32("topFrameHasBeenRedirectedTo", topFrameHasBeenRedirectedTo);
    encoder.encodeUInt32("topFrameHasBeenRedirectedFrom", topFrameHasBeenRedirectedFrom);
    encoder.encodeUInt32("topFrameInitialLoadCount", topFrameInitialLoadCount);
    encoder.encodeUInt32("topFrameHasBeenNavigatedTo", topFrameHasBeenNavigatedTo);
    encoder.encodeUInt32("topFrameHasBeenNavigatedFrom", topFrameHasBeenNavigatedFrom);
    
    // Subframe stats
    encoder.encodeBool("subframeHasBeenLoadedBefore", subframeHasBeenLoadedBefore);
    encoder.encodeUInt32("subframeHasBeenRedirectedTo", subframeHasBeenRedirectedTo);
    encoder.encodeUInt32("subframeHasBeenRedirectedFrom", subframeHasBeenRedirectedFrom);
    encoder.encodeUInt32("subframeSubResourceCount", subframeSubResourceCount);
    encodeHashCountedSet(encoder, "subframeUnderTopFrameOrigins", subframeUnderTopFrameOrigins);
    encodeHashCountedSet(encoder, "subframeUniqueRedirectsTo", subframeUniqueRedirectsTo);
    encoder.encodeUInt32("subframeHasBeenNavigatedTo", subframeHasBeenNavigatedTo);
    encoder.encodeUInt32("subframeHasBeenNavigatedFrom", subframeHasBeenNavigatedFrom);
    
    // Subresource stats
    encoder.encodeUInt32("subresourceHasBeenRedirectedFrom", subresourceHasBeenRedirectedFrom);
    encoder.encodeUInt32("subresourceHasBeenRedirectedTo", subresourceHasBeenRedirectedTo);
    encoder.encodeUInt32("subresourceHasBeenSubresourceCount", subresourceHasBeenSubresourceCount);
    encoder.encodeDouble("subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited", subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited);
    encodeHashCountedSet(encoder, "subresourceUnderTopFrameOrigins", subresourceUnderTopFrameOrigins);
    encodeHashCountedSet(encoder, "subresourceUniqueRedirectsTo", subresourceUniqueRedirectsTo);
    
    // Prevalent Resource
    encodeHashCountedSet(encoder, "redirectedToOtherPrevalentResourceOrigins", redirectedToOtherPrevalentResourceOrigins);
    encoder.encodeBool("isPrevalentResource", isPrevalentResource);
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

bool ResourceLoadStatistics::decode(KeyedDecoder& decoder)
{
    if (!decoder.decodeString("PrevalentResourceOrigin", highLevelDomain))
        return false;
    
    // User interaction
    if (!decoder.decodeBool("hadUserInteraction", hadUserInteraction))
        return false;
    
    // Top frame stats
    if (!decoder.decodeBool("topFrameHasBeenNavigatedToBefore", topFrameHasBeenNavigatedToBefore))
        return false;
    
    if (!decoder.decodeUInt32("topFrameHasBeenRedirectedTo", topFrameHasBeenRedirectedTo))
        return false;
    
    if (!decoder.decodeUInt32("topFrameHasBeenRedirectedFrom", topFrameHasBeenRedirectedFrom))
        return false;
    
    if (!decoder.decodeUInt32("topFrameInitialLoadCount", topFrameInitialLoadCount))
        return false;
    
    if (!decoder.decodeUInt32("topFrameHasBeenNavigatedTo", topFrameHasBeenNavigatedTo))
        return false;
    
    if (!decoder.decodeUInt32("topFrameHasBeenNavigatedFrom", topFrameHasBeenNavigatedFrom))
        return false;
    
    // Subframe stats
    if (!decoder.decodeBool("subframeHasBeenLoadedBefore", subframeHasBeenLoadedBefore))
        return false;
    
    if (!decoder.decodeUInt32("subframeHasBeenRedirectedTo", subframeHasBeenRedirectedTo))
        return false;
    
    if (!decoder.decodeUInt32("subframeHasBeenRedirectedFrom", subframeHasBeenRedirectedFrom))
        return false;
    
    if (!decoder.decodeUInt32("subframeSubResourceCount", subframeSubResourceCount))
        return false;

    decodeHashCountedSet(decoder, "subframeUnderTopFrameOrigins", subframeUnderTopFrameOrigins);
    decodeHashCountedSet(decoder, "subframeUniqueRedirectsTo", subframeUniqueRedirectsTo);
    
    if (!decoder.decodeUInt32("subframeHasBeenNavigatedTo", subframeHasBeenNavigatedTo))
        return false;
    
    if (!decoder.decodeUInt32("subframeHasBeenNavigatedFrom", subframeHasBeenNavigatedFrom))
        return false;
    
    // Subresource stats
    if (!decoder.decodeUInt32("subresourceHasBeenRedirectedFrom", subresourceHasBeenRedirectedFrom))
        return false;
    
    if (!decoder.decodeUInt32("subresourceHasBeenRedirectedTo", subresourceHasBeenRedirectedTo))
        return false;
    
    if (!decoder.decodeUInt32("subresourceHasBeenSubresourceCount", subresourceHasBeenSubresourceCount))
        return false;
    
    if (!decoder.decodeDouble("subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited", subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited))
        return false;

    decodeHashCountedSet(decoder, "subresourceUnderTopFrameOrigins", subresourceUnderTopFrameOrigins);
    decodeHashCountedSet(decoder, "subresourceUniqueRedirectsTo", subresourceUniqueRedirectsTo);
    
    // Prevalent Resource
    decodeHashCountedSet(decoder, "redirectedToOtherPrevalentResourceOrigins", redirectedToOtherPrevalentResourceOrigins);
    
    if (!decoder.decodeBool("isPrevalentResource", isPrevalentResource))
        return false;
    
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

String ResourceLoadStatistics::toString() const
{
    StringBuilder builder;
    
    // User interaction
    appendBoolean(builder, "hadUserInteraction", hadUserInteraction);
    builder.append('\n');
    
    // Top frame stats
    appendBoolean(builder, "topFrameHasBeenNavigatedToBefore", topFrameHasBeenNavigatedToBefore);
    builder.append('\n');
    builder.appendLiteral("    topFrameHasBeenRedirectedTo: ");
    builder.appendNumber(topFrameHasBeenRedirectedTo);
    builder.append('\n');
    builder.appendLiteral("    topFrameHasBeenRedirectedFrom: ");
    builder.appendNumber(topFrameHasBeenRedirectedFrom);
    builder.append('\n');
    builder.appendLiteral("    topFrameInitialLoadCount: ");
    builder.appendNumber(topFrameInitialLoadCount);
    builder.append('\n');
    builder.appendLiteral("    topFrameHasBeenNavigatedTo: ");
    builder.appendNumber(topFrameHasBeenNavigatedTo);
    builder.append('\n');
    builder.appendLiteral("    topFrameHasBeenNavigatedFrom: ");
    builder.appendNumber(topFrameHasBeenNavigatedFrom);
    builder.append('\n');
    
    // Subframe stats
    appendBoolean(builder, "subframeHasBeenLoadedBefore", subframeHasBeenLoadedBefore);
    builder.append('\n');
    builder.appendLiteral("    subframeHasBeenRedirectedTo: ");
    builder.appendNumber(subframeHasBeenRedirectedTo);
    builder.append('\n');
    builder.appendLiteral("    subframeHasBeenRedirectedFrom: ");
    builder.appendNumber(subframeHasBeenRedirectedFrom);
    builder.append('\n');
    builder.appendLiteral("    subframeSubResourceCount: ");
    builder.appendNumber(subframeSubResourceCount);
    builder.append('\n');
    appendHashCountedSet(builder, "subframeUnderTopFrameOrigins", subframeUnderTopFrameOrigins);
    appendHashCountedSet(builder, "subframeUniqueRedirectsTo", subframeUniqueRedirectsTo);
    builder.appendLiteral("    subframeHasBeenNavigatedTo: ");
    builder.appendNumber(subframeHasBeenNavigatedTo);
    builder.append('\n');
    builder.appendLiteral("    subframeHasBeenNavigatedFrom: ");
    builder.appendNumber(subframeHasBeenNavigatedFrom);
    builder.append('\n');
    
    // Subresource stats
    builder.appendLiteral("    subresourceHasBeenRedirectedFrom: ");
    builder.appendNumber(subresourceHasBeenRedirectedFrom);
    builder.append('\n');
    builder.appendLiteral("    subresourceHasBeenRedirectedTo: ");
    builder.appendNumber(subresourceHasBeenRedirectedTo);
    builder.append('\n');
    builder.appendLiteral("    subresourceHasBeenSubresourceCount: ");
    builder.appendNumber(subresourceHasBeenSubresourceCount);
    builder.append('\n');
    builder.appendLiteral("    subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited: ");
    builder.appendNumber(subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited);
    builder.append('\n');
    appendHashCountedSet(builder, "subresourceUnderTopFrameOrigins", subresourceUnderTopFrameOrigins);
    appendHashCountedSet(builder, "subresourceUniqueRedirectsTo", subresourceUniqueRedirectsTo);
    
    // Prevalent Resource
    appendHashCountedSet(builder, "redirectedToOtherPrevalentResourceOrigins", redirectedToOtherPrevalentResourceOrigins);
    appendBoolean(builder, "isPrevalentResource", isPrevalentResource);
    builder.append('\n');

    return builder.toString();
}

template <typename T>
static void mergeHashCountedSet(HashCountedSet<T>& to, const HashCountedSet<T>& from)
{
    for (auto& entry : from)
        to.add(entry.key, entry.value);
}

void ResourceLoadStatistics::merge(const ResourceLoadStatistics& other)
{
    ASSERT(other.highLevelDomain == highLevelDomain);

    hadUserInteraction |= other.hadUserInteraction;
    
    // Top frame stats
    topFrameHasBeenRedirectedTo += other.topFrameHasBeenRedirectedTo;
    topFrameHasBeenRedirectedFrom += other.topFrameHasBeenRedirectedFrom;
    topFrameInitialLoadCount += other.topFrameInitialLoadCount;
    topFrameHasBeenNavigatedTo += other.topFrameHasBeenNavigatedTo;
    topFrameHasBeenNavigatedFrom += other.topFrameHasBeenNavigatedFrom;
    topFrameHasBeenNavigatedToBefore |= other.topFrameHasBeenNavigatedToBefore;
    
    // Subframe stats
    mergeHashCountedSet(subframeUnderTopFrameOrigins, other.subframeUnderTopFrameOrigins);
    subframeHasBeenRedirectedTo += other.subframeHasBeenRedirectedTo;
    subframeHasBeenRedirectedFrom += other.subframeHasBeenRedirectedFrom;
    mergeHashCountedSet(subframeUniqueRedirectsTo, other.subframeUniqueRedirectsTo);
    subframeSubResourceCount += other.subframeSubResourceCount;
    subframeHasBeenNavigatedTo += other.subframeHasBeenNavigatedTo;
    subframeHasBeenNavigatedFrom += other.subframeHasBeenNavigatedFrom;
    subframeHasBeenLoadedBefore |= other.subframeHasBeenLoadedBefore;
    
    // Subresource stats
    mergeHashCountedSet(subresourceUnderTopFrameOrigins, other.subresourceUnderTopFrameOrigins);
    subresourceHasBeenSubresourceCount += other.subresourceHasBeenSubresourceCount;
    subresourceHasBeenRedirectedFrom += other.subresourceHasBeenRedirectedFrom;
    subresourceHasBeenRedirectedTo += other.subresourceHasBeenRedirectedTo;
    mergeHashCountedSet(subresourceUniqueRedirectsTo, other.subresourceUniqueRedirectsTo);
    
    // Prevalent resource stats
    mergeHashCountedSet(redirectedToOtherPrevalentResourceOrigins, other.redirectedToOtherPrevalentResourceOrigins);
    isPrevalentResource |= other.isPrevalentResource;
}

}
