/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "AcceleratedTimeline.h"

#if ENABLE(THREADED_ANIMATIONS)

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AcceleratedTimeline);

Ref<AcceleratedTimeline> AcceleratedTimeline::create(const TimelineIdentifier& identifier, Seconds originTime)
{
    return adoptRef(*new AcceleratedTimeline(identifier, originTime));
}

Ref<AcceleratedTimeline> AcceleratedTimeline::create(const TimelineIdentifier& identifier, ProgressResolutionData progressResolutionData)
{
    return adoptRef(*new AcceleratedTimeline(identifier, progressResolutionData));
}

Ref<AcceleratedTimeline> AcceleratedTimeline::create(TimelineIdentifier&& identifier, Data&& data)
{
    return adoptRef(*new AcceleratedTimeline(WTF::move(identifier), WTF::move(data)));
}

AcceleratedTimeline::AcceleratedTimeline(const TimelineIdentifier& identifier, Seconds originTime)
    : m_identifier(identifier)
    , m_data(originTime)
{
}

AcceleratedTimeline::AcceleratedTimeline(const TimelineIdentifier& identifier, ProgressResolutionData progressResolutionData)
    : m_identifier(identifier)
    , m_data(progressResolutionData)
{
}

AcceleratedTimeline::AcceleratedTimeline(TimelineIdentifier&& identifier, Data&& data)
    : m_identifier(WTF::move(identifier))
    , m_data(WTF::move(data))
{
}

std::optional<Seconds> AcceleratedTimeline::originTime() const
{
    if (auto* originTime = std::get_if<Seconds>(&m_data))
        return *originTime;
    return std::nullopt;
}

std::optional<ProgressResolutionData> AcceleratedTimeline::progressResolutionData() const
{
    if (auto* progressResolutationData = std::get_if<ProgressResolutionData>(&m_data))
        return *progressResolutationData;
    return std::nullopt;
}

void AcceleratedTimeline::setProgressResolutionData(ProgressResolutionData&& progressResolutionData)
{
    ASSERT(!originTime());
    m_data = WTF::move(progressResolutionData);
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
