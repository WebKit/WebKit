/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009, 2010, 2013 Apple Inc. All rights reserved.
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

#include "HTMLFrameOwnerElement.h"
#include "OverlapTestRequestClient.h"
#include "RenderReplaced.h"
#include "Widget.h"

namespace WebCore {

class WidgetHierarchyUpdatesSuspensionScope {
public:
    WidgetHierarchyUpdatesSuspensionScope()
    {
        s_widgetHierarchyUpdateSuspendCount++;
    }
    ~WidgetHierarchyUpdatesSuspensionScope()
    {
        ASSERT(s_widgetHierarchyUpdateSuspendCount);
        if (s_widgetHierarchyUpdateSuspendCount == 1)
            moveWidgets();
        s_widgetHierarchyUpdateSuspendCount--;
    }

    static bool isSuspended() { return s_widgetHierarchyUpdateSuspendCount; }
    static void scheduleWidgetToMove(Widget& widget, FrameView* frame) { widgetNewParentMap().set(&widget, frame); }

private:
    using WidgetToParentMap = HashMap<RefPtr<Widget>, FrameView*>;
    static WidgetToParentMap& widgetNewParentMap();

    WEBCORE_EXPORT void moveWidgets();
    WEBCORE_EXPORT static unsigned s_widgetHierarchyUpdateSuspendCount;
};
    
class RenderWidget : public RenderReplaced, private OverlapTestRequestClient {
    WTF_MAKE_ISO_ALLOCATED(RenderWidget);
public:
    virtual ~RenderWidget();

    HTMLFrameOwnerElement& frameOwnerElement() const { return downcast<HTMLFrameOwnerElement>(nodeForNonAnonymous()); }

    Widget* widget() const { return m_widget.get(); }
    WEBCORE_EXPORT void setWidget(RefPtr<Widget>&&);

    static RenderWidget* find(const Widget&);

    enum class ChildWidgetState { Valid, Destroyed };
    ChildWidgetState updateWidgetPosition() WARN_UNUSED_RETURN;
    WEBCORE_EXPORT IntRect windowClipRect() const;

    bool requiresAcceleratedCompositing() const;

    void ref() { ++m_refCount; }
    void deref();

protected:
    RenderWidget(HTMLFrameOwnerElement&, RenderStyle&&);

    void willBeDestroyed() override;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;
    void layout() override;
    void paint(PaintInfo&, const LayoutPoint&) override;
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;
    virtual void paintContents(PaintInfo&, const LayoutPoint&);
    bool requiresLayer() const override;

private:
    void element() const = delete;

    bool isWidget() const final { return true; }

    bool needsPreferredWidthsRecalculation() const final;
    RenderBox* embeddedContentBox() const final;

    void setSelectionState(HighlightState) final;
    void setOverlapTestResult(bool) final;

    bool setWidgetGeometry(const LayoutRect&);
    bool updateWidgetGeometry();

    RefPtr<Widget> m_widget;
    IntRect m_clipRect; // The rectangle needs to remain correct after scrolling, so it is stored in content view coordinates, and not clipped to window.
    unsigned m_refCount { 1 };
};

inline void RenderWidget::deref()
{
    ASSERT(m_refCount);
    if (!--m_refCount)
        delete this;
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderWidget, isWidget())
