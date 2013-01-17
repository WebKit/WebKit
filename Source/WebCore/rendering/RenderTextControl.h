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

#include "RenderBlock.h"

namespace WebCore {

class HTMLTextFormControlElement;

class RenderTextControl : public RenderBlock {
public:
    virtual ~RenderTextControl();

    HTMLTextFormControlElement* textFormControlElement() const;
    virtual PassRefPtr<RenderStyle> createInnerTextStyle(const RenderStyle* startStyle) const = 0;

    VisiblePosition visiblePositionForIndex(int index) const;

protected:
    RenderTextControl(ContainerNode*);

    // This convenience function should not be made public because innerTextElement may outlive the render tree.
    HTMLElement* innerTextElement() const;

    int scrollbarThickness() const;
    void adjustInnerTextStyle(const RenderStyle* startStyle, RenderStyle* textBlockStyle) const;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    void hitInnerTextElement(HitTestResult&, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset);

    int textBlockWidth() const;
    int textBlockHeight() const;

    float scaleEmToUnits(int x) const;

    static bool hasValidAvgCharWidth(AtomicString family);
    virtual float getAvgCharWidth(AtomicString family);
    virtual LayoutUnit preferredContentWidth(float charWidth) const = 0;
    virtual LayoutUnit computeControlHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const = 0;
    virtual RenderStyle* textBaseStyle() const = 0;

    virtual void updateFromElement();
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const OVERRIDE;
    virtual RenderObject* layoutSpecialExcludedChild(bool relayoutChildren);

private:
    virtual const char* renderName() const { return "RenderTextControl"; }
    virtual bool isTextControl() const { return true; }
    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const OVERRIDE;
    virtual void computePreferredLogicalWidths() OVERRIDE;
    virtual void removeLeftoverAnonymousBlock(RenderBlock*) { }
    virtual bool avoidsFloats() const { return true; }
    virtual bool canHaveGeneratedChildren() const OVERRIDE { return false; }
    virtual bool canBeReplacedWithInlineRunIn() const OVERRIDE;
    
    virtual void addFocusRingRects(Vector<IntRect>&, const LayoutPoint&);

    virtual bool canBeProgramaticallyScrolled() const { return true; }

    virtual bool requiresForcedStyleRecalcPropagation() const { return true; }
};

inline RenderTextControl* toRenderTextControl(RenderObject* object)
{ 
    ASSERT(!object || object->isTextControl());
    return static_cast<RenderTextControl*>(object);
}

inline const RenderTextControl* toRenderTextControl(const RenderObject* object)
{ 
    ASSERT(!object || object->isTextControl());
    return static_cast<const RenderTextControl*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderTextControl(const RenderTextControl*);

} // namespace WebCore

#endif // RenderTextControl_h
