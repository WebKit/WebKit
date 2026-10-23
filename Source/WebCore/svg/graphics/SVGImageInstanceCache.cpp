/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "SVGImageInstanceCache.h"

#include "SVGImageInstance.h"
#include <wtf/PointerComparison.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGImageInstanceCache);

static constexpr size_t maximumInstanceCount = 8;

SVGImageInstanceCache::~SVGImageInstanceCache()
{
    m_defaultInstance = nullptr;
    m_entries.clear();
}

void SVGImageInstanceCache::reset(RefPtr<SVGImageInstance>&& defaultInstance)
{
    removeInstances(0);
    m_defaultInstance = WTF::move(defaultInstance);
    m_entries.clear();
    if (m_defaultInstance)
        m_entries.append({ *m_defaultInstance, 0 });
}

auto SVGImageInstanceCache::instanceForDraw(SVGImage& image, const ImageEnvironmentSettings* environmentSettings, const SVGImageDocumentState& state) -> Lookup
{
    removeInstancesHeldByGoneTimelines();

    if (auto index = findMatching(environmentSettings, state))
        return { moveToFront(*index) };

    if (RefPtr timeline = state.heldAnimationTimeline.get(); timeline && timeline->mode() == ImageAnimationTimeline::Mode::Paused) {
        if (auto index = findPlayingToHold(environmentSettings, state))
            return { moveToFront(*index), true };
    }

    if (m_entries.size() >= maximumInstanceCount) {
        if (auto index = findToRestyle(environmentSettings)) {
            bool tookPlayingInstance = state.heldAnimationTimeline && !m_entries[*index].instance->animationIsHeld();
            return { moveToFront(*index), tookPlayingInstance };
        }
    }

    return { create(image, environmentSettings, state) };
}

SVGImageInstance* SVGImageInstanceCache::matchingInstance(const ImageEnvironmentSettings* environmentSettings, const SVGImageDocumentState& state) const
{
    auto index = findMatching(environmentSettings, state);
    return index ? m_entries[*index].instance.ptr() : nullptr;
}

std::optional<size_t> SVGImageInstanceCache::findMatching(const ImageEnvironmentSettings* environmentSettings, const SVGImageDocumentState& state) const
{
    auto index = m_entries.findIf([&](auto& entry) {
        return arePointingToEqualData(entry.instance->environmentSettings(), environmentSettings) && entry.instance->matchesDocumentState(state);
    });
    return index == notFound ? std::nullopt : std::optional { index };
}

std::optional<size_t> SVGImageInstanceCache::findPlayingToHold(const ImageEnvironmentSettings* environmentSettings, const SVGImageDocumentState& state) const
{
    auto playingState = state;
    playingState.heldAnimationTimeline = nullptr;
    auto index = m_entries.findIf([&](auto& entry) {
        return arePointingToEqualData(entry.instance->environmentSettings(), environmentSettings) && !entry.instance->animationIsHeld() && entry.instance->matchesDocumentState(playingState);
    });
    return index == notFound ? std::nullopt : std::optional { index };
}

std::optional<size_t> SVGImageInstanceCache::findToRestyle(const ImageEnvironmentSettings* environmentSettings) const
{
    auto index = m_entries.reverseFindIf([&](auto& entry) {
        return entry.instance.ptr() != m_defaultInstance && arePointingToEqualData(entry.instance->environmentSettings(), environmentSettings);
    });
    return index == notFound ? std::nullopt : std::optional { index };
}

Ref<SVGImageInstance> SVGImageInstanceCache::create(SVGImage& image, const ImageEnvironmentSettings* environmentSettings, const SVGImageDocumentState& state)
{
    Ref instance = SVGImageInstance::create(image, environmentSettings);

    RefPtr timeline = state.heldAnimationTimeline.get();
    if (!timeline || timeline->mode() != ImageAnimationTimeline::Mode::Stopped) {
        if (RefPtr source = animationTimesSource(environmentSettings))
            instance->copyAnimationTimes(*source);
    }

    size_t decodedSize = instance->approximateMemoryCost();
    m_unreportedDecodedSizeChange += decodedSize;
    m_entries.insert(0, { instance.copyRef(), decodedSize });

    while (m_entries.size() > maximumInstanceCount) {
        auto leastRecentlyDrawn = m_entries.reverseFindIf([&](auto& entry) {
            return entry.instance.ptr() != m_defaultInstance;
        });
        m_unreportedDecodedSizeChange -= m_entries[leastRecentlyDrawn].decodedSize;
        m_entries.removeAt(leastRecentlyDrawn);
    }

    return instance;
}

SVGImageInstance* SVGImageInstanceCache::animationTimesSource(const ImageEnvironmentSettings* environmentSettings) const
{
    SVGImageInstance* heldInstance = nullptr;
    for (auto& entry : m_entries) {
        if (!arePointingToEqualData(entry.instance->environmentSettings(), environmentSettings))
            continue;
        if (!entry.instance->animationIsHeld())
            return entry.instance.ptr();
        if (!heldInstance)
            heldInstance = entry.instance.ptr();
    }
    return heldInstance;
}

Ref<SVGImageInstance> SVGImageInstanceCache::moveToFront(size_t index)
{
    if (index) {
        auto entry = WTF::move(m_entries[index]);
        m_entries.removeAt(index);
        m_entries.insert(0, WTF::move(entry));
    }
    return m_entries.first().instance;
}

void SVGImageInstanceCache::removeInstances(size_t keepCount)
{
    size_t kept = 0;
    m_entries.removeAllMatching([&](auto& entry) {
        if (entry.instance.ptr() == m_defaultInstance || kept++ < keepCount)
            return false;
        m_unreportedDecodedSizeChange -= entry.decodedSize;
        return true;
    });
}

void SVGImageInstanceCache::removeInstancesHeldByGoneTimelines()
{
    m_entries.removeAllMatching([&](auto& entry) {
        if (!entry.instance->heldAnimationTimelineIsGone())
            return false;
        if (entry.instance.ptr() == m_defaultInstance) {
            entry.instance->releaseGoneTimeline();
            return false;
        }
        m_unreportedDecodedSizeChange -= entry.decodedSize;
        return true;
    });
}

void SVGImageInstanceCache::forEach(NOESCAPE const ScopedLambda<void(SVGImageInstance&)>& function) const
{
    for (auto& entry : m_entries)
        function(entry.instance);
}

bool SVGImageInstanceCache::containsIf(NOESCAPE const ScopedLambda<bool(const SVGImageInstance&)>& predicate) const
{
    return m_entries.containsIf([&](auto& entry) {
        return predicate(entry.instance);
    });
}

} // namespace WebCore
