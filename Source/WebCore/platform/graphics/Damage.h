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

#include "FloatRect.h"
#include "Region.h"
#include <wtf/ForbidHeapAllocation.h>

namespace WebCore {

class Damage {
    WTF_FORBID_HEAP_ALLOCATION;

public:
    using Rects = Vector<IntRect, 1>;

    enum class ShouldPropagate : bool {
        No,
        Yes,
    };

    Damage() = default;
    Damage(Damage&&) = default;
    Damage(const Damage&) = default;
    Damage& operator=(const Damage&) = default;

    static const Damage& invalid()
    {
        static const Damage invalidDamage(true);
        return invalidDamage;
    }

    ALWAYS_INLINE const Region& region() const { return m_region; }
    ALWAYS_INLINE IntRect bounds() const { return m_region.bounds(); }
    ALWAYS_INLINE Rects rects() const { return m_region.rects(); }
    ALWAYS_INLINE bool isEmpty() const  { return !m_invalid && m_region.isEmpty(); }
    ALWAYS_INLINE bool isInvalid() const { return m_invalid; }

    void invalidate()
    {
        m_invalid = true;
    }

    ALWAYS_INLINE void add(const Region& region)
    {
        if (isInvalid())
            return;
        m_region.unite(region);
        mergeIfNeeded();
    }

    ALWAYS_INLINE void add(const IntRect& rect)
    {
        if (isInvalid())
            return;
        m_region.unite(rect);
        mergeIfNeeded();
    }

    ALWAYS_INLINE void add(const FloatRect& rect)
    {
        add(enclosingIntRect(rect));
    }

    ALWAYS_INLINE void add(const Damage& other)
    {
        m_invalid = other.isInvalid();
        add(other.m_region);
    }

private:
    bool m_invalid;
    Region m_region;

    // From RenderView.cpp::repaintViewRectangle():
    // Region will get slow if it gets too complex.
    // Merge all rects so far to bounds if this happens.
    static constexpr auto maximumGridSize = 16 * 16;

    ALWAYS_INLINE void mergeIfNeeded()
    {
        if (UNLIKELY(m_region.gridSize() > maximumGridSize))
            m_region = Region(m_region.bounds());
    }

    explicit Damage(bool invalid, Region&& region = { })
        : m_invalid(invalid)
        , m_region(WTFMove(region))
    {
    }

    friend struct IPC::ArgumentCoder<Damage, void>;
};

static inline WTF::TextStream& operator<<(WTF::TextStream& ts, const Damage& damage)
{
    if (damage.isInvalid())
        return ts << "Damage[invalid]";
    return ts << "Damage" << damage.rects();
}

};
