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

#pragma once

#include "HTMLInputElement.h"
#include "RenderTextControl.h"

namespace WebCore {

class RenderTextControlSingleLine : public RenderTextControl {
    WTF_MAKE_ISO_ALLOCATED(RenderTextControlSingleLine);
public:
    RenderTextControlSingleLine(HTMLInputElement&, RenderStyle&&);
    virtual ~RenderTextControlSingleLine();

protected:
    void centerRenderer(RenderBox& renderer) const;
    HTMLElement* containerElement() const;
    HTMLElement* innerBlockElement() const;
    HTMLInputElement& inputElement() const;

private:
    void textFormControlElement() const = delete;

    bool hasControlClip() const override;
    LayoutRect controlClipRect(const LayoutPoint&) const override;
    bool isTextField() const final { return true; }

    void layout() override;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    void autoscroll(const IntPoint&) override;

    // Subclassed to forward to our inner div.
    int scrollLeft() const override;
    int scrollTop() const override;
    int scrollWidth() const override;
    int scrollHeight() const override;
    void setScrollLeft(int, const ScrollPositionChangeOptions&) override;
    void setScrollTop(int, const ScrollPositionChangeOptions&) override;
    bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1, Element** stopElement = nullptr, RenderBox* startBox = nullptr, const IntPoint& wheelEventAbsolutePoint = IntPoint()) final;
    bool logicalScroll(ScrollLogicalDirection, ScrollGranularity, float multiplier = 1, Element** stopElement = 0) final;

    int textBlockWidth() const;
    float getAverageCharWidth() override;
    LayoutUnit preferredContentLogicalWidth(float charWidth) const override;
    LayoutUnit computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const override;
    
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    HTMLElement* innerSpinButtonElement() const;
};

inline HTMLElement* RenderTextControlSingleLine::containerElement() const
{
    return inputElement().containerElement();
}

inline HTMLElement* RenderTextControlSingleLine::innerBlockElement() const
{
    return inputElement().innerBlockElement();
}

// ----------------------------

class RenderTextControlInnerBlock final : public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED(RenderTextControlInnerBlock);
public:
    RenderTextControlInnerBlock(Element& element, RenderStyle&& style)
        : RenderBlockFlow(element, WTFMove(style))
    {
    }

private:
    bool hasLineIfEmpty() const override { return true; }
    bool isTextControlInnerBlock() const override { return true; }
    bool canBeProgramaticallyScrolled() const override
    {
        auto* shadowHost = element()->shadowHost();
        if (is<HTMLInputElement>(shadowHost))
            return !downcast<HTMLInputElement>(*shadowHost).hasAutoFillStrongPasswordButton();
        return true;
    }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderTextControlSingleLine, isTextField())
SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderTextControlInnerBlock, isTextControlInnerBlock())
