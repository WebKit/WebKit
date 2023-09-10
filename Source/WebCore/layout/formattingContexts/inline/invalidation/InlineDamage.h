/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "InlineDisplayContent.h"
#include "InlineLineTypes.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/OptionSet.h>

namespace WebCore {
namespace Layout {

class Box;
class InlineInvalidation;

class InlineDamage {
    WTF_MAKE_ISO_ALLOCATED_INLINE(InlineDamage);
public:
    InlineDamage() = default;
    ~InlineDamage();

    enum class Type : uint8_t {
        // Can't decide the type of damage. Let's nuke all the things.
        Invalid,
        // Content changed or some style property that drives soft wrap opportunities (e.g. going from white-space: pre to normal).
        // This tells us to re-process the inline content and run line layout.
        NeedsContentUpdateAndLineLayout,
        // Same content but either the constraint or some style that may affect line breaking changed (e.g. font-size or containing block set new horizontal constraint). 
        NeedsLineLayout,
        // Line breaking positions are the same, only height related style changed (e.g img's height changes).
        NeedsVerticalAdjustment,
        // Line breaking positions are the same, runs may show up at a different horizontal position (e.g. text-align changes).
        NeedsHorizontalAdjustment 
    };
    Type type() const { return m_damageType; }

    enum class Reason : uint8_t {
        Append        = 1 << 0,
        Insert        = 1 << 1,
        Remove        = 1 << 2,
        ContentChange = 1 << 3
    };
    OptionSet<Reason> reasons() const { return m_damageReasons; }
    // FIXME: Add support for damage range with multiple, different damage types.
    struct Position {
        size_t lineIndex { 0 };
        InlineItemPosition inlineItemPosition { };
    };
    std::optional<Position> start() const { return m_startPosition; }
    using TrailingDisplayBoxList = Vector<InlineDisplay::Box>;
    std::optional<InlineDisplay::Box> trailingContentForLine(size_t lineIndex) const;

    void addDetachedBox(UniqueRef<Box>&& layoutBox) { m_detachedLayoutBoxes.append(WTFMove(layoutBox)); }

#ifndef NDEBUG
    bool hasDetachedContent() const { return !m_detachedLayoutBoxes.isEmpty(); }
#endif

private:
    friend class InlineInvalidation;

    void setDamageReason(Reason reason) { m_damageReasons.add(reason); }
    void setDamageType(Type type) { m_damageType = type; }
    void setDamagedPosition(Position position) { m_startPosition = position; }
    void reset();
    void setTrailingDisplayBoxes(TrailingDisplayBoxList&& trailingDisplayBoxes) { m_trailingDisplayBoxes = WTFMove(trailingDisplayBoxes); }

    Type m_damageType { Type::Invalid };
    OptionSet<Reason> m_damageReasons;
    std::optional<Position> m_startPosition;
    TrailingDisplayBoxList m_trailingDisplayBoxes;
    Vector<UniqueRef<Box>> m_detachedLayoutBoxes;
};

inline std::optional<InlineDisplay::Box> InlineDamage::trailingContentForLine(size_t lineIndex) const
{
    if (m_trailingDisplayBoxes.isEmpty()) {
        // Couldn't compute trailing positions for damaged lines.
        return { };
    }
    if (!start() || start()->lineIndex > lineIndex) {
        ASSERT_NOT_REACHED();
        return { };
    }
    auto relativeLineIndex = lineIndex - start()->lineIndex;
    if (relativeLineIndex >= m_trailingDisplayBoxes.size()) {
        // At the time of the damage, we didn't have this line yet -e.g content insert at a new line.
        return { };
    }
    return { m_trailingDisplayBoxes[relativeLineIndex] };
}

inline InlineDamage::~InlineDamage()
{
    m_trailingDisplayBoxes.clear();
    m_detachedLayoutBoxes.clear();
}

inline void InlineDamage::reset()
{
    m_damageType = { };
    m_damageReasons = { };
    m_startPosition = { };
    m_trailingDisplayBoxes.clear();
    // Never reset m_detachedLayoutBoxes. We need to keep those layout boxes around until after layout.
}

}
}
