/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "FloatingState.h"
#include "FormattingContext.h"
#include "LayoutElementBox.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

class FloatAvoider;
class Box;
class LayoutState;

// FloatingContext is responsible for adjusting the position of a box in the current formatting context
// by taking the floating boxes into account.
// Note that a FloatingContext's inline direction always matches the root's inline direction but it may
// not match the FloatingState's inline direction (i.e. FloatingState may be constructed by a parent BFC with mismatching inline direction).
class FloatingContext {
    WTF_MAKE_ISO_ALLOCATED(FloatingContext);
public:
    FloatingContext(const FormattingContext&, const FloatingState&);

    const FloatingState& floatingState() const { return m_floatingState; }

    LayoutPoint positionForFloat(const Box&, const HorizontalConstraints&) const;
    LayoutPoint positionForNonFloatingFloatAvoider(const Box&) const;

    struct PositionWithClearance {
        LayoutUnit position;
        std::optional<LayoutUnit> clearance;
    };
    std::optional<PositionWithClearance> verticalPositionWithClearance(const Box&) const;

    std::optional<LayoutUnit> top() const;
    std::optional<LayoutUnit> leftBottom() const { return bottom(Clear::Left); }
    std::optional<LayoutUnit> rightBottom() const { return bottom(Clear::Right); }
    std::optional<LayoutUnit> bottom() const { return bottom(Clear::Both); }

    bool isEmpty() const { return m_floatingState.floats().isEmpty(); }

    struct Constraints {
        std::optional<PointInContextRoot> left;
        std::optional<PointInContextRoot> right;
    };
    enum class MayBeAboveLastFloat : uint8_t { Yes, No };
    Constraints constraints(LayoutUnit candidateTop, LayoutUnit candidateBottom, MayBeAboveLastFloat) const;

    FloatingState::FloatItem toFloatItem(const Box& floatBox) const;

private:
    std::optional<LayoutUnit> bottom(Clear) const;

    bool isFloaingCandidateLogicallyLeftPositioned(const Box&) const;
    Clear logicalClear(const Box&) const;

    const LayoutState& layoutState() const { return m_floatingState.layoutState(); }
    const FormattingContext& formattingContext() const { return m_formattingContext; }
    const ElementBox& root() const { return m_formattingContext.root(); }

    void findPositionForFormattingContextRoot(FloatAvoider&) const;

    struct AbsoluteCoordinateValuesForFloatAvoider;
    AbsoluteCoordinateValuesForFloatAvoider absoluteCoordinates(const Box&) const;
    LayoutPoint mapTopLeftToFloatingStateRoot(const Box&) const;
    Point mapPointFromFormattingContextRootToFloatingStateRoot(Point) const;

    const FormattingContext& m_formattingContext;
    const FloatingState& m_floatingState;
};

}
}
