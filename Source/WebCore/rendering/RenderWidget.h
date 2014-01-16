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

#ifndef RenderWidget_h
#define RenderWidget_h

#include "HTMLFrameOwnerElement.h"
#include "OverlapTestRequestClient.h"
#include "RenderReplaced.h"
#include "Widget.h"
#include <wtf/WeakPtr.h>

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
    static void scheduleWidgetToMove(Widget* widget, FrameView* frame) { widgetNewParentMap().set(widget, frame); }

private:
    typedef HashMap<RefPtr<Widget>, FrameView*> WidgetToParentMap;
    static WidgetToParentMap& widgetNewParentMap();

    void moveWidgets();

    static unsigned s_widgetHierarchyUpdateSuspendCount;
};
    
class RenderWidget : public RenderReplaced, private OverlapTestRequestClient {
public:
    virtual ~RenderWidget();

    HTMLFrameOwnerElement& frameOwnerElement() const { return toHTMLFrameOwnerElement(nodeForNonAnonymous()); }

    Widget* widget() const { return m_widget.get(); }
    void setWidget(PassRefPtr<Widget>);

    static RenderWidget* find(const Widget*);

    void updateWidgetPosition();
    IntRect windowClipRect() const;

#if USE(ACCELERATED_COMPOSITING)
    bool requiresAcceleratedCompositing() const;
#endif

    WeakPtr<RenderWidget> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

protected:
    RenderWidget(HTMLFrameOwnerElement&, PassRef<RenderStyle>);

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override final;
    virtual void layout() override;
    virtual void paint(PaintInfo&, const LayoutPoint&) override;
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;
    virtual void paintContents(PaintInfo&, const LayoutPoint&);
#if USE(ACCELERATED_COMPOSITING)
    virtual bool requiresLayer() const override;
#endif

private:
    void element() const WTF_DELETED_FUNCTION;

    virtual bool isWidget() const override final { return true; }

    virtual bool needsPreferredWidthsRecalculation() const override final;
    virtual RenderBox* embeddedContentBox() const override final;

    virtual void willBeDestroyed() override final;
    virtual void setSelectionState(SelectionState) override final;
    virtual void setOverlapTestResult(bool) override final;

    bool setWidgetGeometry(const LayoutRect&);
    bool updateWidgetGeometry();

    WeakPtrFactory<RenderWidget> m_weakPtrFactory;
    RefPtr<Widget> m_widget;
    IntRect m_clipRect; // The rectangle needs to remain correct after scrolling, so it is stored in content view coordinates, and not clipped to window.
};

RENDER_OBJECT_TYPE_CASTS(RenderWidget, isWidget())

} // namespace WebCore

#endif // RenderWidget_h
