/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef RenderTextControlSingleLine_h
#define RenderTextControlSingleLine_h

#include "HTMLInputElement.h"
#include "RenderTextControl.h"

namespace WebCore {

class HTMLInputElement;

class RenderTextControlSingleLine : public RenderTextControl {
public:
    RenderTextControlSingleLine(HTMLInputElement&, PassRef<RenderStyle>);
    virtual ~RenderTextControlSingleLine();
    // FIXME: Move create*Style() to their classes.
    virtual PassRef<RenderStyle> createInnerTextStyle(const RenderStyle* startStyle) const override;
    PassRef<RenderStyle> createInnerBlockStyle(const RenderStyle* startStyle) const;

    void capsLockStateMayHaveChanged();

protected:
    virtual void centerContainerIfNeeded(RenderBox*) const { }
    virtual LayoutUnit computeLogicalHeightLimit() const;
    HTMLElement* containerElement() const;
    HTMLElement* innerBlockElement() const;
    HTMLInputElement& inputElement() const;

private:
    void textFormControlElement() const WTF_DELETED_FUNCTION;

    virtual bool hasControlClip() const override;
    virtual LayoutRect controlClipRect(const LayoutPoint&) const override;
    virtual bool isTextField() const override final { return true; }

    virtual void paint(PaintInfo&, const LayoutPoint&) override;
    virtual void layout() override;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    virtual void autoscroll(const IntPoint&) override;

    // Subclassed to forward to our inner div.
    virtual int scrollLeft() const override;
    virtual int scrollTop() const override;
    virtual int scrollWidth() const override;
    virtual int scrollHeight() const override;
    virtual void setScrollLeft(int) override;
    virtual void setScrollTop(int) override;
    virtual bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1, Element** stopElement = 0) override final;
    virtual bool logicalScroll(ScrollLogicalDirection, ScrollGranularity, float multiplier = 1, Element** stopElement = 0) override final;

    int textBlockWidth() const;
    virtual float getAvgCharWidth(AtomicString family) override;
    virtual LayoutUnit preferredContentLogicalWidth(float charWidth) const override;
    virtual LayoutUnit computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const override;
    
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    bool textShouldBeTruncated() const;

    HTMLElement* innerSpinButtonElement() const;

    bool m_shouldDrawCapsLockIndicator;
    LayoutUnit m_desiredInnerTextLogicalHeight;
};

inline HTMLElement* RenderTextControlSingleLine::containerElement() const
{
    return inputElement().containerElement();
}

inline HTMLElement* RenderTextControlSingleLine::innerBlockElement() const
{
    return inputElement().innerBlockElement();
}

RENDER_OBJECT_TYPE_CASTS(RenderTextControlSingleLine, isTextField())

// ----------------------------

class RenderTextControlInnerBlock final : public RenderBlockFlow {
public:
    RenderTextControlInnerBlock(Element& element, PassRef<RenderStyle> style)
        : RenderBlockFlow(element, std::move(style))
    {
    }

private:
    virtual bool hasLineIfEmpty() const override { return true; }
    virtual bool isTextControlInnerBlock() const override { return true; }
};

RENDER_OBJECT_TYPE_CASTS(RenderTextControlInnerBlock, isTextControlInnerBlock())

}

#endif
