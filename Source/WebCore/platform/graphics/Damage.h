/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "FloatRect.h"
#include "Region.h"
#include <wtf/ForbidHeapAllocation.h>

namespace WebCore {

class Damage {
    WTF_FORBID_HEAP_ALLOCATION;

public:
    using Rects = Vector<IntRect, 1>;

    enum class Propagation : uint8_t {
        None,
        Region,
        Unified,
    };

    static constexpr int s_tileSize { 256 };

    Damage()
        : m_tileSize(s_tileSize, s_tileSize)
    {
        // FIXME: Add a constructor to pass the size.
        resize({ 2048, 1024 });
    }

    Damage(Damage&&) = default;
    Damage(const Damage&) = default;
    Damage& operator=(const Damage&) = default;
    Damage& operator=(Damage&&) = default;

    static const Damage& invalid()
    {
        static const Damage invalidDamage(true);
        return invalidDamage;
    }

    Region region() const
    {
        Region region;
        for (const auto& rect : rects())
            region.unite(rect);
        return region;
    }

    ALWAYS_INLINE const IntRect& bounds() const { return m_minimumBoundingRectangle; }

    // May return both empty and overlapping rects.
    ALWAYS_INLINE const Rects& rects() const { return m_rects; }
    ALWAYS_INLINE bool isEmpty() const  { return !m_invalid && m_rects.isEmpty(); }
    ALWAYS_INLINE bool isInvalid() const { return m_invalid; }

    void resize(const IntSize& size)
    {
        if (m_size == size)
            return;

        m_size = size;
        m_gridSize = { std::max(1, m_size.width() / m_tileSize.width()), std::max(1, m_size.height() / m_tileSize.height()) };
        m_rects.clear();
        m_shouldUnite = m_gridSize.width() == 1 && m_gridSize.height() == 1;
    }

    ALWAYS_INLINE void add(const Region& region)
    {
        if (isInvalid() || region.isEmpty())
            return;

        for (const auto& rect : region.rects())
            add(rect);
    }

    void add(const IntRect& rect)
    {
        if (isInvalid() || rect.isEmpty())
            return;

        if (rect.contains(m_minimumBoundingRectangle)) {
            m_rects.clear();
            m_rects.append(rect);
            m_minimumBoundingRectangle = rect;
            return;
        }

        const auto rectsCount = m_rects.size();
        if (rectsCount == 1 && m_minimumBoundingRectangle.contains(rect))
            return;

        m_minimumBoundingRectangle.unite(rect);

        if (m_shouldUnite) {
            unite(rect);
            return;
        }

        if (rectsCount == m_gridSize.unclampedArea()) {
            m_shouldUnite = true;
            uniteExistingRects();
            unite(rect);
            return;
        }

        m_rects.append(rect);
    }

    ALWAYS_INLINE void add(const FloatRect& rect)
    {
        if (isInvalid() || rect.isEmpty())
            return;

        add(enclosingIntRect(rect));
    }

    ALWAYS_INLINE void add(const Damage& other)
    {
        m_invalid = other.isInvalid();
        if (isInvalid()) {
            m_rects.clear();
            return;
        }

        for (const auto& rect : other.rects())
            add(rect);
    }

private:
    explicit Damage(bool invalid)
        : m_invalid(invalid)
    {
    }

    void uniteExistingRects()
    {
        Rects rectsCopy(m_rects.size());
        m_rects.swap(rectsCopy);

        for (const auto& rect : rectsCopy)
            unite(rect);
    }

    ALWAYS_INLINE size_t tileIndexForRect(const IntRect& rect) const
    {
        if (m_rects.size() == 1)
            return 0;

        const auto rectCenter = rect.center();
        const auto rectCell = flooredIntPoint(FloatPoint { static_cast<float>(rectCenter.x()) / m_tileSize.width(), static_cast<float>(rectCenter.y()) / m_tileSize.height() });
        return std::clamp(rectCell.x(), 0, m_gridSize.width() - 1) + std::clamp(rectCell.y(), 0, m_gridSize.height() - 1) * m_gridSize.width();
    }

    void unite(const IntRect& rect)
    {
        // When merging cannot be avoided, we use m_rects to store minimal bounding rectangles
        // and perform merging while trying to keep minimal bounding rectangles small and
        // separated from each other.
        const auto index = tileIndexForRect(rect);
        ASSERT(index < m_rects.size());
        m_rects[index].unite(rect);
    }

    IntSize m_size;
    bool m_invalid { false };
    bool m_shouldUnite { false };
    IntSize m_tileSize;
    IntSize m_gridSize;
    Rects m_rects;
    IntRect m_minimumBoundingRectangle;

    friend bool operator==(const Damage&, const Damage&) = default;
};

class FrameDamageHistory {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(FrameDamageHistory);
public:
    using SimplifiedDamage = std::pair<bool, Region>;

    const Vector<SimplifiedDamage>& damageInformation() const { return m_damageInfo; }
    void addDamage(const SimplifiedDamage& damage) { m_damageInfo.append(damage); }

private:
    Vector<SimplifiedDamage> m_damageInfo;
};

static inline WTF::TextStream& operator<<(WTF::TextStream& ts, const Damage& damage)
{
    if (damage.isInvalid())
        return ts << "Damage[invalid]"_s;
    return ts << "Damage"_s << damage.rects();
}

} // namespace WebCore

#endif // PLATFORM(GTK) || PLATFORM(WPE)
