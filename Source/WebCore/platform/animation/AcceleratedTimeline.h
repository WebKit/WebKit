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

#pragma once

#if ENABLE(THREADED_ANIMATIONS)

#include <WebCore/ProgressResolutionData.h>
#include <WebCore/TimelineIdentifier.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Variant.h>

namespace WebCore {

class AcceleratedTimeline : public RefCounted<AcceleratedTimeline> {
    WTF_MAKE_TZONE_ALLOCATED(AcceleratedTimeline);
public:

    static Ref<AcceleratedTimeline> create(const TimelineIdentifier&, Seconds originTime);
    static Ref<AcceleratedTimeline> create(const TimelineIdentifier&, ProgressResolutionData);

    bool isMonotonic() const { return !!originTime(); }
    bool isProgressBased() const { return !isMonotonic(); }

    WEBCORE_EXPORT std::optional<Seconds> originTime() const;
    WEBCORE_EXPORT std::optional<ProgressResolutionData> progressResolutionData() const;

    // Encoding support.
    using Data = Variant<Seconds, ProgressResolutionData>;
    WEBCORE_EXPORT static Ref<AcceleratedTimeline> create(TimelineIdentifier&&, Data&&);
    const TimelineIdentifier& identifier() const { return m_identifier; }
    const Data& data() const { return m_data; }

    virtual ~AcceleratedTimeline() = default;

private:
    AcceleratedTimeline(const TimelineIdentifier&, Seconds originTime);
    AcceleratedTimeline(const TimelineIdentifier&, ProgressResolutionData);
    AcceleratedTimeline(TimelineIdentifier&&, Data&&);

    TimelineIdentifier m_identifier;
    Data m_data;
};

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
