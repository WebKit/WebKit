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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "FloatSizeHash.h"
#include "ImageBuffer.h"
#include "ImageDrawDescriptor.h"
#include "StyleAppleColorFilter.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace Style {

template<typename> class GeneratedImageCache;

template<typename Value> class GeneratedImageCacheEntry {
    WTF_MAKE_TZONE_ALLOCATED_TEMPLATE(GeneratedImageCacheEntry);
    WTF_MAKE_NONCOPYABLE(GeneratedImageCacheEntry);
public:
    GeneratedImageCacheEntry(GeneratedImageCache<Value>& cache, FloatSize size, const AppleColorFilter& colorFilter, Ref<Value>&& value)
        : m_cache { cache }
        , m_size { size }
        , m_colorFilter { colorFilter }
        , m_value { WTF::move(value) }
        , m_evictionTimer { *this, &GeneratedImageCacheEntry::evictionTimerFired, timeToKeep }
    {
        m_evictionTimer.restart();
    }

    FloatSize size() const { return m_size; }
    const AppleColorFilter& colorFilter() const LIFETIME_BOUND { return m_colorFilter; }
    NODELETE Value& value() const LIFETIME_BOUND { return m_value; }

    NODELETE RefPtr<ImageBuffer> patternBuffer(const ImageDrawDescriptor& descriptor, FloatSize tileSize, FloatSize scaleFactor) const
    {
        if (!m_patternBuffer || m_patternTileSize != tileSize || m_patternDrawLuminanceMask != descriptor.drawLuminanceMask || !areEssentiallyEqual(scaleFactor, m_patternScaleFactor))
            return nullptr;
        return m_patternBuffer;
    }

    void setPatternBuffer(Ref<ImageBuffer>&& buffer, const ImageDrawDescriptor& descriptor, FloatSize tileSize, FloatSize scaleFactor)
    {
        m_patternBuffer = WTF::move(buffer);
        m_patternTileSize = tileSize;
        m_patternDrawLuminanceMask = descriptor.drawLuminanceMask;
        m_patternScaleFactor = scaleFactor;
    }

    const RefPtr<ImageBuffer>& patternBufferForTesting() const LIFETIME_BOUND { return m_patternBuffer; }

    void puntEvictionTimer() { m_evictionTimer.restart(); }

private:
    static constexpr auto timeToKeep = 3_s;

    void evictionTimerFired()
    {
        // NOTE: This is essentially a "delete this", the object is no longer valid after this line.
        m_cache.evict(*this);
    }

    GeneratedImageCache<Value>& m_cache;
    const FloatSize m_size;
    const AppleColorFilter m_colorFilter;
    const Ref<Value> m_value;
    RefPtr<ImageBuffer> m_patternBuffer;
    FloatSize m_patternTileSize;
    DrawLuminanceMask m_patternDrawLuminanceMask { DrawLuminanceMask::No };
    FloatSize m_patternScaleFactor;
    DeferrableOneShotTimer m_evictionTimer;
};

WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_IMPL(template<typename Value>, GeneratedImageCacheEntry<Value>);

// What a generated image recently drew with, each kept until it goes unused for a few seconds.
template<typename Value> class GeneratedImageCache {
    WTF_MAKE_NONCOPYABLE(GeneratedImageCache);
public:
    using Entry = GeneratedImageCacheEntry<Value>;
    using Entries = HashMap<FloatSize, Vector<std::unique_ptr<Entry>, 1>>;

    GeneratedImageCache() = default;

    Entry& ensure(FloatSize size, const AppleColorFilter& colorFilter, NOESCAPE const Invocable<Ref<Value>()> auto& createValue)
    {
        ASSERT(!size.isEmpty());

        auto& entries = m_entries.add(size, Vector<std::unique_ptr<Entry>, 1> { }).iterator->value;
        for (auto& entry : entries) {
            if (entry->colorFilter() == colorFilter) {
                entry->puntEvictionTimer();
                return *entry;
            }
        }

        entries.append(makeUnique<Entry>(*this, size, colorFilter, createValue()));
        return *entries.last();
    }

    const Entries& entries() const LIFETIME_BOUND { return m_entries; }

private:
    friend Entry;

    void evict(Entry& entry)
    {
        auto it = m_entries.find(entry.size());
        ASSERT(it != m_entries.end());

        bool removed = it->value.removeFirstMatching([&](auto& candidate) {
            return candidate.get() == &entry;
        });
        ASSERT_UNUSED(removed, removed);

        if (it->value.isEmpty())
            m_entries.remove(it);
    }

    Entries m_entries;
};

} // namespace Style
} // namespace WebCore
