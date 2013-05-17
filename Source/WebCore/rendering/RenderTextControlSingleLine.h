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
    RenderTextControlSingleLine(Element*);
    virtual ~RenderTextControlSingleLine();
    // FIXME: Move create*Style() to their classes.
    virtual PassRefPtr<RenderStyle> createInnerTextStyle(const RenderStyle* startStyle) const;
    PassRefPtr<RenderStyle> createInnerBlockStyle(const RenderStyle* startStyle) const;

    void capsLockStateMayHaveChanged();

protected:
    virtual void centerContainerIfNeeded(RenderBox*) const { }
    virtual LayoutUnit computeLogicalHeightLimit() const;
    HTMLElement* containerElement() const;
    HTMLElement* innerBlockElement() const;
    HTMLInputElement* inputElement() const;
    virtual void updateFromElement() OVERRIDE;

private:
    virtual bool hasControlClip() const OVERRIDE;
    virtual LayoutRect controlClipRect(const LayoutPoint&) const OVERRIDE;
    virtual bool isTextField() const OVERRIDE FINAL { return true; }

    virtual void paint(PaintInfo&, const LayoutPoint&) OVERRIDE;
    virtual void layout() OVERRIDE;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) OVERRIDE;

    virtual void autoscroll(const IntPoint&) OVERRIDE;

    // Subclassed to forward to our inner div.
    virtual int scrollLeft() const OVERRIDE;
    virtual int scrollTop() const OVERRIDE;
    virtual int scrollWidth() const OVERRIDE;
    virtual int scrollHeight() const OVERRIDE;
    virtual void setScrollLeft(int) OVERRIDE;
    virtual void setScrollTop(int) OVERRIDE;
    virtual bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1, Node** stopNode = 0) OVERRIDE;
    virtual bool logicalScroll(ScrollLogicalDirection, ScrollGranularity, float multiplier = 1, Node** stopNode = 0) OVERRIDE;

    int textBlockWidth() const;
    virtual float getAvgCharWidth(AtomicString family) OVERRIDE;
    virtual LayoutUnit preferredContentLogicalWidth(float charWidth) const OVERRIDE;
    virtual LayoutUnit computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const OVERRIDE;
    
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE;

    virtual RenderStyle* textBaseStyle() const OVERRIDE;

    bool textShouldBeTruncated() const;

    HTMLElement* innerSpinButtonElement() const;

    bool m_shouldDrawCapsLockIndicator;
    LayoutUnit m_desiredInnerTextLogicalHeight;
};

inline HTMLElement* RenderTextControlSingleLine::containerElement() const
{
    return inputElement()->containerElement();
}

inline HTMLElement* RenderTextControlSingleLine::innerBlockElement() const
{
    return inputElement()->innerBlockElement();
}

inline RenderTextControlSingleLine* toRenderTextControlSingleLine(RenderObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isTextField());
    return static_cast<RenderTextControlSingleLine*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderTextControlSingleLine(const RenderTextControlSingleLine*);

// ----------------------------

class RenderTextControlInnerBlock : public RenderBlock {
public:
    RenderTextControlInnerBlock(Element* element) : RenderBlock(element) { }

private:
    virtual bool hasLineIfEmpty() const { return true; }
};

}

#endif
