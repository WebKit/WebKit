/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "MediaSample.h"
#include <wtf/Assertions.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>

namespace WebCore {

// Bundles N non-displaying decode prerequisites with a displaying sample,
// presenting itself with the displaying sample's timing.
//
// SourceBufferPrivate's coded-frame processing uses this to keep out-of-order
// decode prerequisites alive when an incoming sample's PTS shadows their
// original presentation slots. Without it, those prerequisites would be
// cascade-evicted, leaving a gap in the buffered range until the next random
// access point.
//
// Lives only in the trackBuffer's sample storage; the trackBuffer's enqueue
// path unpacks it into its constituent samples at the decode-queue boundary,
// so platform-specific renderer code never sees a CombinedMediaSample.
class CombinedMediaSample final : public MediaSample {
public:
    // `prerequisites` are inner samples that decode but do not display.
    // Each must already be non-displaying and have a (DTS, PTS) key distinct
    // from `displaying` and from every other prerequisite. Their DTS may be
    // less than or greater than `displaying`'s — the trackBuffer's enqueue
    // path inserts each sample into the decode queue using its own key, so
    // ordering is recovered there. `displaying` is the sample whose timing
    // the combined sample reports.
    //
    // Nesting is normalized away: any `prerequisites` entry that is itself a
    // CombinedMediaSample contributes its own prerequisites + its displaying
    // sample as flat individual prerequisites of the resulting object. If
    // `displaying` is already a CombinedMediaSample, the new prerequisites
    // are prepended to its existing ones and its inner displaying sample is
    // adopted as the new displaying — so no caller ever has to look through
    // a CombinedMediaSample wrapping another CombinedMediaSample.
    static Ref<CombinedMediaSample> create(Vector<Ref<MediaSample>>&& prerequisites, Ref<MediaSample>&& displaying)
    {
        auto flattened = flattenPrerequisites(WTF::move(prerequisites));
        if (RefPtr combined = dynamicDowncast<CombinedMediaSample>(displaying)) {
            flattened.appendVector(combined->m_prerequisiteSamples);
            return adoptRef(*new CombinedMediaSample(WTF::move(flattened), combined->m_displayingSample.copyRef()));
        }

        return adoptRef(*new CombinedMediaSample(WTF::move(flattened), WTF::move(displaying)));
    }

    const Vector<Ref<MediaSample>>& prerequisiteSamples() const { return m_prerequisiteSamples; }
    MediaSample& displayingSample() const { return m_displayingSample.get(); }

    // MediaSample overrides — timing forwarded to the displaying sample so the
    // trackBuffer's (DTS, PTS) keys stay unique.
    MediaTime presentationTime() const final { return m_displayingSample->presentationTime(); }
    MediaTime decodeTime()       const final { return m_displayingSample->decodeTime(); }
    MediaTime duration()         const final { return m_displayingSample->duration(); }
    TrackID trackID()            const final { return m_displayingSample->trackID(); }
    FloatSize presentationSize() const final { return m_displayingSample->presentationSize(); }
    SampleFlags flags()          const final { return m_displayingSample->flags(); }
    Type type()                  const final { return Type::Combined; }

    size_t sizeInBytes() const final
    {
        size_t total = m_displayingSample->sizeInBytes();
        for (auto& sample : m_prerequisiteSamples)
            total += sample->sizeInBytes();
        return total;
    }

    // CombinedMediaSample is never enqueued to the renderer directly.
    PlatformSample platformSample() const final
    {
        ASSERT_NOT_REACHED();
        return m_displayingSample->platformSample();
    }

    // Inner-sample timing is set at construction. The trackBuffer should not
    // reassign timing of a sample after it has been added.
    void offsetTimestampsBy(const MediaTime&) final { ASSERT_NOT_REACHED(); }
    void setTimestamps(const MediaTime&, const MediaTime&) final { ASSERT_NOT_REACHED(); }

    // Used by the trackBuffer's preroll path: build a new combined sample
    // whose displaying sample has itself been replaced with its non-displaying
    // copy. The prerequisites were already non-displaying.
    Ref<MediaSample> createNonDisplayingCopy() const final
    {
        return adoptRef(*new CombinedMediaSample(copyToVectorOf<Ref<MediaSample>>(m_prerequisiteSamples), m_displayingSample->createNonDisplayingCopy()));
    }

private:
    CombinedMediaSample(Vector<Ref<MediaSample>>&& prerequisites, Ref<MediaSample>&& displaying)
        : m_prerequisiteSamples(WTF::move(prerequisites))
        , m_displayingSample(WTF::move(displaying))
    {
    }

    static Vector<Ref<MediaSample>> flattenPrerequisites(Vector<Ref<MediaSample>>&& prerequisites)
    {
        if (!std::any_of(prerequisites.begin(), prerequisites.end(), [](auto& sample) {
            return is<CombinedMediaSample>(sample);
        }))
            return prerequisites;

        Vector<Ref<MediaSample>> flattened;
        flattened.reserveInitialCapacity(prerequisites.size());
        for (auto& prereq : prerequisites) {
            if (RefPtr combined = dynamicDowncast<CombinedMediaSample>(prereq.get())) {
                flattened.appendVector(combined->m_prerequisiteSamples);
                flattened.append(combined->m_displayingSample->createNonDisplayingCopy());
            } else
                flattened.append(prereq);
        }

        return flattened;
    }

    const Vector<Ref<MediaSample>> m_prerequisiteSamples;
    const Ref<MediaSample> m_displayingSample;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CombinedMediaSample)
    static bool isType(const WebCore::MediaSample& sample) { return sample.type() == WebCore::MediaSample::Type::Combined; }
SPECIALIZE_TYPE_TRAITS_END()
