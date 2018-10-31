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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "LayoutBox.h"
#include "LayoutUnit.h"
#include <wtf/IsoMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace Layout {

class FloatingState;
class LayoutState;

class FloatAvoider {
    WTF_MAKE_ISO_ALLOCATED(FloatAvoider);
public:
    FloatAvoider(const Box&, const FloatingState&, const LayoutState&);
    virtual ~FloatAvoider() = default;

    virtual Display::Box::Rect rect() const { return m_absoluteDisplayBox.rect(); }
    Display::Box::Rect rectInContainingBlock() const;

    struct HorizontalConstraints {
        std::optional<PositionInContextRoot> left;
        std::optional<PositionInContextRoot> right;
    };
    void setHorizontalConstraints(HorizontalConstraints);
    void setVerticalConstraint(PositionInContextRoot);

    bool overflowsContainingBlock() const;

    void resetPosition();

protected:
    virtual bool isLeftAligned() const { return layoutBox().style().isLeftToRightDirection(); }
    virtual PositionInContextRoot initialHorizontalPosition() const;
    virtual PositionInContextRoot initialVerticalPosition() const { return m_initialVerticalPosition; }

    void resetHorizontalConstraints();

    virtual PositionInContextRoot horizontalPositionCandidate(HorizontalConstraints);
    virtual PositionInContextRoot verticalPositionCandidate(PositionInContextRoot);

    LayoutUnit marginTop() const { return displayBox().marginTop(); }
    LayoutUnit marginBottom() const { return displayBox().marginBottom(); }
    LayoutUnit marginLeft() const { return displayBox().nonComputedMarginLeft(); }
    LayoutUnit marginRight() const { return displayBox().nonComputedMarginRight(); }

    LayoutUnit marginBoxWidth() const { return marginLeft() + displayBox().width() + marginRight(); }

    const FloatingState& floatingState() const { return m_floatingState; }
    const Box& layoutBox() const { return *m_layoutBox; }
    const Display::Box& displayBox() const { return m_absoluteDisplayBox; }

private:
    WeakPtr<const Box> m_layoutBox;
    const FloatingState& m_floatingState;
    Display::Box m_absoluteDisplayBox;
    Display::Box m_containingBlockAbsoluteDisplayBox;
    PositionInContextRoot m_initialVerticalPosition;
};

}
}
#endif
