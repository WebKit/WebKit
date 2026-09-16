/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include <WebCore/RenderBoxModelObject.h>
#include <wtf/Platform.h>

namespace WebCore {

class Position;
class RenderBlock;
class RenderFragmentContainer;

class RenderInline : public RenderBoxModelObject {
    WTF_MAKE_TZONE_ALLOCATED(RenderInline);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderInline);
public:
    RenderInline(Type, Element&, Style::ComputedStyle&&);
    RenderInline(Type, Document&, Style::ComputedStyle&&);
    virtual ~RenderInline();





    bool requiresLayer() const override;

protected:
    void styleDidChange(Style::Difference, const Style::ComputedStyle* oldStyle) override;

private:
    ASCIILiteral renderName() const override;

    bool canHaveChildren() const final { return true; }

    void layout() final { ASSERT_NOT_REACHED(); } // Do nothing for layout()

    void paint(PaintInfo&, const LayoutPoint&) final;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;

    LayoutUnit offsetWidth() const final { return borderBoxRectInContainer().width(); }
    LayoutUnit offsetHeight() const final { return borderBoxRectInContainer().height(); }

protected:
    RepaintRects localRectsForRepaint(RepaintOutlineBounds) const override;
    LayoutRect rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const final;


private:

    LayoutRect frameRectForStickyPositioning() const final { return borderBoxRectInContainer(); }

    void imageChanged(WrappedImagePtr, const IntRect* = 0) final;
};


} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderInline, isInlineBox())
