/*
 * Copyright (C) 2005 Apple Inc.
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

#ifndef RenderButton_h
#define RenderButton_h

#include "RenderFlexibleBox.h"
#include "Timer.h"
#include <memory>

namespace WebCore {

class HTMLFormControlElement;
class RenderTextFragment;

// RenderButtons are just like normal flexboxes except that they will generate an anonymous block child.
// For inputs, they will also generate an anonymous RenderText and keep its style and content up
// to date as the button changes.
class RenderButton final : public RenderFlexibleBox {
public:
    RenderButton(HTMLFormControlElement&, PassRef<RenderStyle>);
    virtual ~RenderButton();

    HTMLFormControlElement& formControlElement() const;

    virtual bool canBeSelectionLeaf() const override;

    virtual void addChild(RenderObject* newChild, RenderObject *beforeChild = 0) override;
    virtual RenderObject* removeChild(RenderObject&) override;
    virtual void removeLeftoverAnonymousBlock(RenderBlock*) override { }
    virtual bool createsAnonymousWrapper() const override { return true; }

    void setupInnerStyle(RenderStyle*);
    virtual void updateFromElement() override;

    virtual bool canHaveGeneratedChildren() const override;
    virtual bool hasControlClip() const override { return true; }
    virtual LayoutRect controlClipRect(const LayoutPoint&) const override;

    void setText(const String&);
    String text() const;

#if PLATFORM(IOS)
    virtual void layout() override;
#endif

private:
    void element() const = delete;

    virtual const char* renderName() const override { return "RenderButton"; }
    virtual bool isRenderButton() const override { return true; }

    virtual void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    virtual bool hasLineIfEmpty() const override;

    virtual bool requiresForcedStyleRecalcPropagation() const override { return true; }

    void timerFired(Timer<RenderButton>&);

    RenderTextFragment* m_buttonText;
    RenderBlock* m_inner;

    std::unique_ptr<Timer<RenderButton>> m_timer;
    bool m_default;
};

RENDER_OBJECT_TYPE_CASTS(RenderButton, isRenderButton())

} // namespace WebCore

#endif // RenderButton_h
