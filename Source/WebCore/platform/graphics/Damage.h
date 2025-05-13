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

#if USE(COORDINATED_GRAPHICS)
#include "FloatRect.h"
#include "LayoutSize.h"
#include "Region.h"
#include <wtf/ForbidHeapAllocation.h>

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

    enum class Propagation : uint8_t {
        None,
        Region,
        Unified,
    };

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

    ALWAYS_INLINE const IntRect& bounds() const { return m_minimumBoundingRectangle; }

    // May return both empty and overlapping rects.
    ALWAYS_INLINE const Rects& rects() const { return m_rects; }
    ALWAYS_INLINE bool isEmpty() const  { return m_rects.isEmpty(); }
    ALWAYS_INLINE Mode mode() const { return m_mode; }

    // Removes empty and overlapping rects. May clip to grid.
    Rects rectsForPainting() const
    {
        if (m_rects.size() <= 1 || m_mode != Mode::Rectangles)
            return m_rects;

        Rects rects;
        for (int row = 0; row < m_gridCells.height(); ++row) {
            for (int col = 0; col < m_gridCells.width(); ++col) {
                const IntRect cellRect = { { m_rect.x() + col * m_cellSize.width(), m_rect.y() + row * m_cellSize.height() }, m_cellSize };
                IntRect minimumBoundingRectangleContaingOverlaps;
                for (const auto& rect : m_rects) {
                    if (!rect.isEmpty())
                        minimumBoundingRectangleContaingOverlaps.unite(intersection(cellRect, rect));
                }
                if (!minimumBoundingRectangleContaingOverlaps.isEmpty())
                    rects.append(minimumBoundingRectangleContaingOverlaps);
            }
        }
        return rects;
    }

    void makeFull()
    {
        if (m_mode == Mode::Full)
            return;

        m_mode = Mode::Full;
        m_rects.clear();
        m_shouldUnite = false;
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

        const auto rectsCount = m_rects.size();
        if (!rectsCount || rect.contains(m_minimumBoundingRectangle)) {
            m_rects.clear();
            if (m_mode == Mode::Rectangles)
                m_shouldUnite = m_gridCells.width() == 1 && m_gridCells.height() == 1;
            m_rects.append(rect);
            m_minimumBoundingRectangle = rect;
            return true;
        }

        if (rectsCount == 1 && m_minimumBoundingRectangle.contains(rect))
            return false;

        m_minimumBoundingRectangle.unite(rect);
        if (m_mode == Mode::BoundingBox) {
            ASSERT(rectsCount == 1);
            m_rects[0] = m_minimumBoundingRectangle;
            return true;
        }

        if (m_shouldUnite) {
            unite(rect);
            return true;
        }

        if (rectsCount == m_gridCells.unclampedArea()) {
            m_shouldUnite = true;
            uniteExistingRects();
            unite(rect);
            return true;
        }

        m_rects.append(rect);
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
        if (m_mode == Mode::Rectangles && m_rects.isEmpty()) {
            auto rectsCount = rects.size();
            auto gridArea = m_gridCells.unclampedArea();

            if (rectsCount > gridArea) {
                m_rects.grow(gridArea);
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
                    m_rects.clear();
                    return false;
                }
                m_shouldUnite = true;

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

        // When both Damage are already united and have the same rect and grid, we can just iterate the rects and unite them.
        if (m_mode == Mode::Rectangles && m_shouldUnite && m_mode == other.m_mode && m_rect == other.m_rect && m_gridCells == other.m_gridCells && other.m_shouldUnite && m_rects.size() == other.m_rects.size()) {
            for (unsigned i = 0; i < m_rects.size(); ++i)
                m_rects[i].unite(other.m_rects[i]);
            return true;
        }

        return add(other.rects());
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
                m_gridCells = gridSize(maxRectangles);
                m_cellSize = IntSize(std::ceil(static_cast<float>(m_rect.width()) / m_gridCells.width()), std::ceil(static_cast<float>(m_rect.height()) / m_gridCells.height()));
            } else {
                static constexpr int defaultCellSize { 256 };
                m_cellSize = { defaultCellSize, defaultCellSize };
                m_gridCells = IntSize(std::ceil(static_cast<float>(m_rect.width()) / m_cellSize.width()), std::ceil(static_cast<float>(m_rect.height()) / m_cellSize.height())).expandedTo({ 1, 1 });
            }

            m_shouldUnite = m_gridCells.width() == 1 && m_gridCells.height() == 1;
            break;
        case Mode::BoundingBox:
            break;
        case Mode::Full:
            m_minimumBoundingRectangle = m_rect;
            m_rects.append(m_minimumBoundingRectangle);
            break;
        }
    }

    ALWAYS_INLINE bool shouldAdd() const
    {
        return !m_rect.isEmpty() && m_mode != Mode::Full;
    }

    void uniteExistingRects()
    {
        Rects rectsCopy(m_rects.size());
        m_rects.swap(rectsCopy);

        for (const auto& rect : rectsCopy)
            unite(rect);
    }

    ALWAYS_INLINE size_t cellIndexForRect(const IntRect& rect) const
    {
        ASSERT(m_rects.size() > 1);

        const auto rectCenter = IntPoint(rect.center() - m_rect.location());
        const auto rectCell = flooredIntPoint(FloatPoint { static_cast<float>(rectCenter.x()) / m_cellSize.width(), static_cast<float>(rectCenter.y()) / m_cellSize.height() });
        return std::clamp(rectCell.x(), 0, m_gridCells.width() - 1) + std::clamp(rectCell.y(), 0, m_gridCells.height() - 1) * m_gridCells.width();
    }

    void unite(const IntRect& rect)
    {
        // When merging cannot be avoided, we use m_rects to store minimal bounding rectangles
        // and perform merging while trying to keep minimal bounding rectangles small and
        // separated from each other.
        if (m_rects.size() == 1) {
            m_rects[0] = m_minimumBoundingRectangle;
            return;
        }

        const auto index = cellIndexForRect(rect);
        ASSERT(index < m_rects.size());
        m_rects[index].unite(rect);
    }

    Mode m_mode { Mode::Rectangles };
    IntRect m_rect;
    bool m_shouldUnite { false };
    IntSize m_cellSize;
    IntSize m_gridCells;
    Rects m_rects;
    IntRect m_minimumBoundingRectangle;

    friend bool operator==(const Damage&, const Damage&) = default;
};

class FrameDamageHistory {
    WTF_MAKE_FAST_ALLOCATED;
public:
    const Vector<Region>& damageInformation() const { return m_damageInfo; }

    void addDamage(const Damage& damage)
    {
        Region region;
        for (const auto& rect : damage.rects()) {
            Region subRegion(rect);
            region.unite(subRegion);
        }
        m_damageInfo.append(WTFMove(region));
    }

private:
    // Use a Region to remove overlaps so that Damage rects are more predictable from the testing perspective.
    Vector<Region> m_damageInfo;
};

static inline WTF::TextStream& operator<<(WTF::TextStream& ts, const Damage& damage)
{
    return ts << "Damage"_s << damage.rects();
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
