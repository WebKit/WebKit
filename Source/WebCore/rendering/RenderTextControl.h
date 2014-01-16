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

#ifndef RenderTextControl_h
#define RenderTextControl_h

#include "RenderBlockFlow.h"
#include "RenderFlexibleBox.h"

namespace WebCore {

class TextControlInnerTextElement;
class HTMLTextFormControlElement;

class RenderTextControl : public RenderBlockFlow {
public:
    virtual ~RenderTextControl();

    HTMLTextFormControlElement& textFormControlElement() const;
    virtual PassRef<RenderStyle> createInnerTextStyle(const RenderStyle* startStyle) const = 0;

#if PLATFORM(IOS)
    bool canScroll() const;

    // Returns the line height of the inner renderer.
    virtual int innerLineHeight() const override;
#endif

protected:
    RenderTextControl(HTMLTextFormControlElement&, PassRef<RenderStyle>);

    // This convenience function should not be made public because innerTextElement may outlive the render tree.
    TextControlInnerTextElement* innerTextElement() const;

    int scrollbarThickness() const;
    void adjustInnerTextStyle(const RenderStyle* startStyle, RenderStyle* textBlockStyle) const;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void hitInnerTextElement(HitTestResult&, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset);

    int textBlockLogicalWidth() const;
    int textBlockLogicalHeight() const;

    float scaleEmToUnits(int x) const;

    static bool hasValidAvgCharWidth(AtomicString family);
    virtual float getAvgCharWidth(AtomicString family);
    virtual LayoutUnit preferredContentLogicalWidth(float charWidth) const = 0;
    virtual LayoutUnit computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const = 0;

    virtual void updateFromElement() override;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;
    virtual RenderObject* layoutSpecialExcludedChild(bool relayoutChildren) override;

private:
    void element() const WTF_DELETED_FUNCTION;

    virtual const char* renderName() const override { return "RenderTextControl"; }
    virtual bool isTextControl() const override final { return true; }
    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    virtual void computePreferredLogicalWidths() override;
    virtual void removeLeftoverAnonymousBlock(RenderBlock*) override { }
    virtual bool avoidsFloats() const override { return true; }
    virtual bool canHaveGeneratedChildren() const override { return false; }
    virtual bool canBeReplacedWithInlineRunIn() const override;
    
    virtual void addFocusRingRects(Vector<IntRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) override;

    virtual bool canBeProgramaticallyScrolled() const override { return true; }

    virtual bool requiresForcedStyleRecalcPropagation() const override { return true; }
};

RENDER_OBJECT_TYPE_CASTS(RenderTextControl, isTextControl())

// Renderer for our inner container, for <search> and others.
// We can't use RenderFlexibleBox directly, because flexboxes have a different
// baseline definition, and then inputs of different types wouldn't line up
// anymore.
class RenderTextControlInnerContainer final : public RenderFlexibleBox {
public:
    explicit RenderTextControlInnerContainer(Element& element, PassRef<RenderStyle> style)
        : RenderFlexibleBox(element, std::move(style))
    { }
    virtual ~RenderTextControlInnerContainer() { }

    virtual int baselinePosition(FontBaseline baseline, bool firstLine, LineDirectionMode direction, LinePositionMode position) const override
    {
        return RenderBlock::baselinePosition(baseline, firstLine, direction, position);
    }
    virtual int firstLineBaseline() const override { return RenderBlock::firstLineBaseline(); }
    virtual int inlineBlockBaseline(LineDirectionMode direction) const override { return RenderBlock::inlineBlockBaseline(direction); }

};


} // namespace WebCore

#endif // RenderTextControl_h
