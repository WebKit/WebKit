/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NetworkAdClickAttribution.h"

#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebKit {

using AdClickAttribution = WebCore::AdClickAttribution;
using Source = WebCore::AdClickAttribution::Source;
using Destination = WebCore::AdClickAttribution::Destination;
using DestinationMap = HashMap<Destination, AdClickAttribution>;

DestinationMap& NetworkAdClickAttribution::ensureDestinationMapForSource(const Source& source)
{
    return m_adClickAttributionMap.ensure(source, [] {
        return DestinationMap { };
    }).iterator->value;
}

void NetworkAdClickAttribution::store(AdClickAttribution&& adClickAttribution)
{
    auto& destinationMapForSource = ensureDestinationMapForSource(adClickAttribution.source());
    destinationMapForSource.add(adClickAttribution.destination(), WTFMove(adClickAttribution));
}

void NetworkAdClickAttribution::clear(CompletionHandler<void()>&& completionHandler)
{
    m_adClickAttributionMap.clear();
    completionHandler();
}

void NetworkAdClickAttribution::toString(CompletionHandler<void(String)>&& completionHandler) const
{
    if (m_adClickAttributionMap.isEmpty())
        return completionHandler("No stored Ad Click Attribution data.");

    StringBuilder builder;
    for (auto& innerMap : m_adClickAttributionMap.values()) {
        unsigned attributionNumber = 1;
        for (auto& attribution : innerMap.values()) {
            builder.appendLiteral("WebCore::AdClickAttribution ");
            builder.appendNumber(attributionNumber++);
            builder.append('\n');
            builder.append(attribution.toString());
        }
    }

    completionHandler(builder.toString());
}

} // namespace WebKit
