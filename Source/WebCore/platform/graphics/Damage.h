/*
 * Copyright (C) 2024, 2025 Igalia S.L.
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

#if USE(COORDINATED_GRAPHICS)
#include "FloatRect.h"
#include "LayoutSize.h"
#include "Region.h"
#include <wtf/BitVector.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/text/TextStream.h>

// A helper class to store damage rectangles in a few approximated ways
// to trade-off the CPU cost of the data structure and the resolution
// it brings (i.e. how good approximation reflects the reality).

// The simplest way to store the damage is to maintain a minimum
// bounding rectangle (bounding box) of all incoming damage rectangles.
// This way the amount of memory used is minimal (just a single rect)
// and the add() operations are cheap as it's always about unite().
// While this method works well in many scenarios, it fails to model
// the small rectangles that are very far apart.

// The more sophisticated method to store the damage is to store a
// limited vector of rectangles. Unless the limit of rectangles is hit
// each rectangle is stored as-is.
// Once the new rectangle cannot be added without extending the vector
// past the limit, the unification mechanism starts.
// Unification mechanism - once enabled - uses an artificial grid
// to map incoming rects into cells that can store up to 1 rectangle
// each. If more than one rect gets mapped to the same cell, such
// rectangles are unified using a minimum bounding rectangle.
// This way the amount of memory used is limited as the vector of
// rectangles cannot grow past the limit. At the same time, the
// CPU utilization is also limited as the rect addition cost
// is O(1) excluding the vector addition complexity.
// And since the vector size is limited, the cost of adding to vector
// cannot get out of hand either.
// This method is more expensive than simple "bonding box", however,
// it yields surprisingly good approximation results.
// Moreover, the approximation resolution can be controlled by tweaking
// the artificial grid size - the more rows/cols the better the
// resolution at the expense of higher memory/CPU utilization.

namespace WebCore {

class Damage {
    WTF_FORBID_HEAP_ALLOCATION;

public:
    using Rects = Vector<IntRect, 1>;

    enum class Mode : uint8_t {
        Rectangles, // Tracks dirty regions as rectangles, only unifying when maximum is reached.
        BoundingBox, // Dirty region is always the minimum bounding box of all added rectangles.
        Full, // All area is always dirty.
    };

    static constexpr uint32_t NoMaxRectangles = 0;

    explicit Damage(const IntRect& rect, Mode mode = Mode::Rectangles, uint32_t maxRectangles = NoMaxRectangles)
        : m_mode(mode)
        , m_rect(rect)
    {
        initialize(maxRectangles);
    }

    explicit Damage(const IntSize& size, Mode mode = Mode::Rectangles, uint32_t maxRectangles = NoMaxRectangles)
        : Damage({ { }, size }, mode, maxRectangles)
    {
    }

    explicit Damage(const FloatSize& size, Mode mode = Mode::Rectangles, uint32_t maxRectangles = NoMaxRectangles)
        : Damage(ceiledIntSize(LayoutSize(size)), mode, maxRectangles)
    {
    }

    Damage(Damage&&) = default;
    Damage(const Damage&) = default;
    Damage& operator=(const Damage&) = default;
    Damage& operator=(Damage&&) = default;

    ALWAYS_INLINE Mode mode() const { return m_mode; }

    ALWAYS_INLINE const IntRect& at(size_t index) const LIFETIME_BOUND
    {
        switch (m_mode) {
        case Mode::Rectangles:
            if (!m_rects.indices.isEmpty()) {
                size_t i = 0;
                for (const auto& rect : *this) {
                    if (i++ == index)
                        return rect;
                }
            }
            return m_rects.rects.at(index);
        case Mode::BoundingBox:
            ASSERT(!index);
            return m_minimumBoundingRectangle;
        case Mode::Full:
            ASSERT(!index);
            return m_rect;
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

    const IntRect& operator[](size_t index) const LIFETIME_BOUND { return at(index); }

    ALWAYS_INLINE size_t size() const
    {
        // FIXME: we should not allow to create a Damage for an empty rect.
        if (m_rect.isEmpty())
            return 0;

        switch (m_mode) {
        case Mode::Rectangles:
            if (m_rects.indices.isEmpty())
                return m_rects.rects.size();
            return m_rects.indices.bitCount();
        case Mode::BoundingBox:
            return m_minimumBoundingRectangle.isEmpty() ? 0 : 1;
        case Mode::Full:
            return 1;
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

    ALWAYS_INLINE bool isEmpty() const
    {
        // FIXME: we should not allow to create a Damage for an empty rect.
        if (m_rect.isEmpty())
            return true;

        switch (m_mode) {
        case Mode::Rectangles:
            return m_rects.indices.isEmpty() && m_rects.rects.isEmpty();
        case Mode::BoundingBox:
            return m_minimumBoundingRectangle.isEmpty();
        case Mode::Full:
            return false;
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

    ALWAYS_INLINE const IntRect& bounds() const
    {
        switch (m_mode) {
        case Mode::Rectangles:
        case Mode::BoundingBox:
            return m_minimumBoundingRectangle;
        case Mode::Full:
            return m_rect;
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

    class iterator {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(iterator);
    public:
        iterator() = default;

        iterator(const Damage& damage, size_t index)
            : m_damage(&damage)
            , m_index(index)
        {
        }

        iterator(const Damage& damage, BitVector::iterator iter)
            : m_damage(&damage)
            , m_iter(iter)
            , m_index(*iter)
        {
        }

        const IntRect& operator*()
        {
            switch (m_damage->m_mode) {
            case Mode::Rectangles:
                return m_damage->m_rects.rects.at(m_index);
            case Mode::BoundingBox:
                ASSERT(!m_index);
                return m_damage->m_minimumBoundingRectangle;
            case Mode::Full:
                ASSERT(!m_index);
                return m_damage->m_rect;
            }
            RELEASE_ASSERT_NOT_REACHED();
        }

        iterator& operator++()
        {
            if (m_iter) {
                ++m_iter.value();
                m_index = *m_iter.value();
            } else
                ++m_index;

            return *this;
        }

        bool operator==(const iterator& other) const
        {
            return m_index == other.m_index;
        }

    private:
        const Damage* m_damage { nullptr };
        std::optional<BitVector::iterator> m_iter;
        size_t m_index { 0 };
    };

    iterator begin() const LIFETIME_BOUND
    {
        switch (m_mode) {
        case Mode::Rectangles:
            if (!m_rects.indices.isEmpty())
                return iterator(*this, m_rects.indices.begin());
            [[fallthrough]];
        case Mode::BoundingBox:
        case Mode::Full:
            return iterator(*this, 0);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    iterator end() const LIFETIME_BOUND
    {
        switch (m_mode) {
        case Mode::Rectangles:
            if (!m_rects.indices.isEmpty())
                return iterator(*this, m_rects.indices.end());
            [[fallthrough]];
        case Mode::BoundingBox:
        case Mode::Full:
            return iterator(*this, size());
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    ALWAYS_INLINE const Rects& rectsForTesting() const
    {
        return m_rects.rects;
    }

    Region regionForTesting() const
    {
        Region region;

        // FIXME: we should not allow to create a Damage for an empty rect.
        if (m_rect.isEmpty())
            return region;

        for (const auto& rect : *this)
            region.unite(Region { rect });

        return region;
    }

    // May return overlapping rects.
    ALWAYS_INLINE Rects rects() const
    {
        // FIXME: we should not allow to create a Damage for an empty rect.
        if (m_rect.isEmpty())
            return { };

        Rects result;
        result.reserveInitialCapacity(size());
        for (const auto& rect : *this)
            result.append(rect);
        return result;
    }

    void makeFull()
    {
        if (m_mode == Mode::Full)
            return;

        m_mode = Mode::Full;
        initialize(NoMaxRectangles);
    }

    bool add(const IntRect& rect)
    {
        if (rect.isEmpty() || !shouldAdd())
            return false;

        if (rect.contains(m_rect)) {
            makeFull();
            return true;
        }

        const auto rectsCount = size();
        if (!rectsCount || rect.contains(m_minimumBoundingRectangle)) {
            if (m_mode == Mode::Rectangles) {
                if (rectsCount) {
                    m_rects.rects.clear();
                    m_rects.indices = { };
                    m_rects.shouldUnite = m_rects.gridCells.width() == 1 && m_rects.gridCells.height() == 1;
                }
                m_rects.rects.append(rect);
            }

            m_minimumBoundingRectangle = rect;
            return true;
        }

        if (rectsCount == 1 && m_minimumBoundingRectangle.contains(rect))
            return false;

        m_minimumBoundingRectangle.unite(rect);
        if (m_mode == Mode::BoundingBox) {
            ASSERT(rectsCount == 1);
            return true;
        }

        ASSERT(m_mode == Mode::Rectangles);
        if (m_rects.shouldUnite) {
            unite(rect);
            return true;
        }

        if (rectsCount == m_rects.gridCells.unclampedArea()) {
            m_rects.shouldUnite = true;
            uniteExistingRects();
            unite(rect);
            return true;
        }

        m_rects.rects.append(rect);
        return true;
    }

    ALWAYS_INLINE bool add(const FloatRect& rect)
    {
        if (rect.isEmpty() || !shouldAdd())
            return false;

        return add(enclosingIntRect(rect));
    }

    ALWAYS_INLINE bool add(const Rects& rects)
    {
        if (rects.isEmpty() || !shouldAdd())
            return false;

        // When adding rects to an empty Damage and we know we will need to unite,
        // we can unite the rects directly.
        if (m_mode == Mode::Rectangles && isEmpty()) {
            auto rectsCount = rects.size();
            auto gridArea = m_rects.gridCells.unclampedArea();

            if (rectsCount > gridArea) {
                m_rects.rects.grow(gridArea);
                if (rectsCount > 1)
                    m_rects.indices = BitVector(rectsCount);

                for (const auto& rect : rects) {
                    if (rect.isEmpty())
                        continue;

                    if (rect.contains(m_rect)) {
                        makeFull();
                        return true;
                    }

                    m_minimumBoundingRectangle.unite(rect);
                    unite(rect);
                }

                if (m_minimumBoundingRectangle.isEmpty()) {
                    // All rectangles were empty.
                    m_rects.rects.clear();
                    m_rects.indices = { };
                    return false;
                }

                m_rects.shouldUnite = true;
                if (m_rects.rects.size() == 1)
                    m_rects.indices = { };

                return true;
            }
        }

        bool returnValue = false;
        for (const auto& rect : rects)
            returnValue |= add(rect);
        return returnValue;
    }

    ALWAYS_INLINE bool add(const Damage& other)
    {
        if (other.isEmpty() || !shouldAdd())
            return false;

        if (other.m_mode == Mode::Full && m_rect == other.m_rect) {
            makeFull();
            return true;
        }

        if (m_mode == Mode::Rectangles) {
            // When both Damage are already united and have the same rect and grid, we can just iterate the rects and unite them.
            if (m_rects.shouldUnite && m_mode == other.m_mode && m_rect == other.m_rect && m_rects.gridCells == other.m_rects.gridCells && other.m_rects.shouldUnite && m_rects.rects.size() == other.m_rects.rects.size()) {
                for (unsigned i = 0; i < m_rects.rects.size(); ++i)
                    m_rects.rects[i].unite(other.m_rects.rects[i]);
                return true;
            }
        }

        switch (other.m_mode) {
        case Mode::Rectangles:
            return add(other.m_rects.rects);
        case Mode::BoundingBox:
            return add(other.m_minimumBoundingRectangle);
        case Mode::Full:
            return add(other.m_rect);
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

private:
    IntSize gridSize(int maxRectangles) const
    {
        const float widthToHeightRatio = static_cast<float>(m_rect.width()) / m_rect.height();
        if (widthToHeightRatio >= 1) {
            int gridHeight = std::max<int>(1, std::floor(std::sqrt(static_cast<float>(maxRectangles) / widthToHeightRatio)));
            while (gridHeight > 1 && maxRectangles % gridHeight)
                gridHeight--;
            return { maxRectangles / gridHeight, gridHeight };
        }

        int gridWidth = std::max<int>(1, std::floor(std::sqrt(static_cast<float>(maxRectangles) * widthToHeightRatio)));
        while (gridWidth > 1 && maxRectangles % gridWidth)
            gridWidth--;
        return { gridWidth, maxRectangles / gridWidth };
    }

    void initialize(uint32_t maxRectangles)
    {
        switch (m_mode) {
        case Mode::Rectangles:
            if (maxRectangles != NoMaxRectangles) {
                m_rects.gridCells = gridSize(maxRectangles);
                m_rects.cellSize = IntSize(std::ceil(static_cast<float>(m_rect.width()) / m_rects.gridCells.width()), std::ceil(static_cast<float>(m_rect.height()) / m_rects.gridCells.height()));
            } else {
                static constexpr int defaultCellSize { 256 };
                m_rects.cellSize = { defaultCellSize, defaultCellSize };
                m_rects.gridCells = IntSize(std::ceil(static_cast<float>(m_rect.width()) / m_rects.cellSize.width()), std::ceil(static_cast<float>(m_rect.height()) / m_rects.cellSize.height())).expandedTo({ 1, 1 });
            }

            m_rects.shouldUnite = m_rects.gridCells.width() == 1 && m_rects.gridCells.height() == 1;
            break;
        case Mode::BoundingBox:
        case Mode::Full:
            m_minimumBoundingRectangle = { };
            m_rects = { };
            break;
        }
    }

    ALWAYS_INLINE bool shouldAdd() const
    {
        // FIXME: we should not allow to create a Damage for an empty rect.
        return !m_rect.isEmpty() && m_mode != Mode::Full;
    }

    void uniteExistingRects()
    {
        auto rectsCount = m_rects.rects.size();
        Rects rectsCopy(rectsCount);
        m_rects.rects.swap(rectsCopy);
        if (rectsCount > 1)
            m_rects.indices = BitVector(rectsCount);

        for (const auto& rect : rectsCopy)
            unite(rect);
    }

    ALWAYS_INLINE size_t cellIndexForRect(const IntRect& rect) const
    {
        ASSERT(m_rects.rects.size() > 1);

        const auto rectCenter = IntPoint(rect.center() - m_rect.location());
        const auto rectCell = flooredIntPoint(FloatPoint { static_cast<float>(rectCenter.x()) / m_rects.cellSize.width(), static_cast<float>(rectCenter.y()) / m_rects.cellSize.height() });
        return std::clamp(rectCell.x(), 0, m_rects.gridCells.width() - 1) + std::clamp(rectCell.y(), 0, m_rects.gridCells.height() - 1) * m_rects.gridCells.width();
    }

    void unite(const IntRect& rect)
    {
        // When merging cannot be avoided, we use m_rects to store minimal bounding rectangles
        // and perform merging while trying to keep minimal bounding rectangles small and
        // separated from each other.
        if (m_rects.rects.size() == 1) {
            m_rects.rects[0] = m_minimumBoundingRectangle;
            return;
        }

        const auto index = cellIndexForRect(rect);
        ASSERT(index < m_rects.rects.size());
        m_rects.rects[index].unite(rect);
        m_rects.indices.set(index);
    }

    Mode m_mode { Mode::Rectangles };
    IntRect m_rect;
    IntRect m_minimumBoundingRectangle;
    struct {
        Rects rects;
        BitVector indices;
        bool shouldUnite { false };
        IntSize cellSize;
        IntSize gridCells;
    } m_rects;
};

static inline WTF::TextStream& operator<<(WTF::TextStream& ts, const Damage& damage)
{
    return streamSizedContainer(ts, damage);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
