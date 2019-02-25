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
#include "AdClickAttribution.h"

#include <wtf/RandomNumber.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

bool AdClickAttribution::isValid() const
{
    return m_conversion
        && m_conversion.value().isValid()
        && m_campaign.isValid()
        && !m_source.registrableDomain.isEmpty()
        && !m_destination.registrableDomain.isEmpty()
        && m_earliestTimeToSend;
}

void AdClickAttribution::setConversion(Conversion&& conversion)
{
    if (!conversion.isValid() || (m_conversion && m_conversion->priority > conversion.priority))
        return;

    m_conversion = WTFMove(conversion);
    // 24-48 hour delay before sending. This helps privacy since the conversion and the attribution
    // requests are detached and the time of the attribution does not reveal the time of the conversion.
    m_earliestTimeToSend = m_timeOfAdClick + 24_h + Seconds(randomNumber() * (24_h).value());
}

URL AdClickAttribution::url() const
{
    if (!isValid())
        return URL();

    StringBuilder builder;
    builder.appendLiteral("https://");
    builder.append(m_source.registrableDomain.string());
    builder.appendLiteral("/.well-known/ad-click-attribution/");
    builder.appendNumber(m_conversion.value().data);
    builder.append('/');
    builder.appendNumber(m_campaign.id);

    URL url { URL(), builder.toString() };
    if (url.isValid())
        return url;

    return URL();
}

URL AdClickAttribution::referrer() const
{
    if (!isValid())
        return URL();

    StringBuilder builder;
    builder.appendLiteral("https://");
    builder.append(m_destination.registrableDomain.string());
    builder.append('/');

    URL url { URL(), builder.toString() };
    if (url.isValid())
        return url;
    
    return URL();
}

String AdClickAttribution::toString() const
{
    StringBuilder builder;
    builder.appendLiteral("Source: ");
    builder.append(m_source.registrableDomain.string());
    builder.appendLiteral("\nDestination: ");
    builder.append(m_destination.registrableDomain.string());
    builder.appendLiteral("\nCampaign ID: ");
    builder.appendNumber(m_campaign.id);
    if (m_conversion) {
        builder.appendLiteral("\nConversion data: ");
        builder.appendNumber(m_conversion.value().data);
    } else
        builder.appendLiteral("\nNo conversion data.");
    builder.append('\n');

    return builder.toString();
}

}
