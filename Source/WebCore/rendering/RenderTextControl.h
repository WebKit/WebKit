/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/) 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "RenderBlockFlow.h"
#include "RenderFlexibleBox.h"

namespace WebCore {

class TextControlInnerTextElement;
class HTMLTextFormControlElement;

class RenderTextControl : public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED(RenderTextControl);
public:
    virtual ~RenderTextControl();

    WEBCORE_EXPORT HTMLTextFormControlElement& textFormControlElement() const;

#if PLATFORM(IOS_FAMILY)
    bool canScroll() const;

    // Returns the line height of the inner renderer.
    int innerLineHeight() const override;
#endif

protected:
    RenderTextControl(HTMLTextFormControlElement&, RenderStyle&&);

    // This convenience function should not be made public because innerTextElement may outlive the render tree.
    RefPtr<TextControlInnerTextElement> innerTextElement() const;

    int scrollbarThickness() const;

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void hitInnerTextElement(HitTestResult&, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset);

    int textBlockLogicalWidth() const;
    int textBlockLogicalHeight() const;

    float scaleEmToUnits(int x) const;

    virtual float getAverageCharWidth();
    virtual LayoutUnit preferredContentLogicalWidth(float charWidth) const = 0;
    virtual LayoutUnit computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const = 0;

    LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;
    void layoutExcludedChildren(bool relayoutChildren) override;

private:
    void element() const = delete;

    const char* renderName() const override { return "RenderTextControl"; }
    bool isTextControl() const final { return true; }
    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    void computePreferredLogicalWidths() override;
    bool avoidsFloats() const override { return true; }
    bool canHaveGeneratedChildren() const override { return false; }
    
    void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) override;

    bool canBeProgramaticallyScrolled() const override { return true; }
};

// Renderer for our inner container, for <search> and others.
// We can't use RenderFlexibleBox directly, because flexboxes have a different
// baseline definition, and then inputs of different types wouldn't line up
// anymore.
class RenderTextControlInnerContainer final : public RenderFlexibleBox {
    WTF_MAKE_ISO_ALLOCATED(RenderTextControlInnerContainer);
public:
    explicit RenderTextControlInnerContainer(Element& element, RenderStyle&& style)
        : RenderFlexibleBox(element, WTFMove(style))
    { }
    virtual ~RenderTextControlInnerContainer() = default;

    int baselinePosition(FontBaseline baseline, bool firstLine, LineDirectionMode direction, LinePositionMode position) const override
    {
        return RenderBlock::baselinePosition(baseline, firstLine, direction, position);
    }
    std::optional<int> firstLineBaseline() const override { return RenderBlock::firstLineBaseline(); }
    std::optional<int> inlineBlockBaseline(LineDirectionMode direction) const override { return RenderBlock::inlineBlockBaseline(direction); }

private:
    bool isFlexibleBoxImpl() const override { return true; }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderTextControl, isTextControl())
