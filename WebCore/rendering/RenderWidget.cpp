/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RenderWidget.h"

#include "AnimationController.h"
#include "AXObjectCache.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "RenderView.h"
#include "RenderWidgetProtector.h"

using namespace std;

namespace WebCore {

static HashMap<const Widget*, RenderWidget*>& widgetRendererMap()
{
    static HashMap<const Widget*, RenderWidget*>* staticWidgetRendererMap = new HashMap<const Widget*, RenderWidget*>;
    return *staticWidgetRendererMap;
}

RenderWidget::RenderWidget(Node* node)
    : RenderReplaced(node)
    , m_widget(0)
    , m_frameView(node->document()->view())
    , m_refCount(0)
{
    view()->addWidget(this);

    // Reference counting is used to prevent the widget from being
    // destroyed while inside the Widget code, which might not be
    // able to handle that.
    ref();
}

void RenderWidget::destroy()
{
    // We can't call the base class's destroy because we don't
    // want to unconditionally delete ourselves (we're ref-counted).
    // So the code below includes copied and pasted contents of
    // both RenderBox::destroy() and RenderObject::destroy().
    // Fix originally made for <rdar://problem/4228818>.

    // <rdar://problem/6937089> suggests that node() can be null by the time we call renderArena()
    // in the end of this function. One way this might happen is if this function was invoked twice
    // in a row, so bail out and turn a crash into an assertion failure in debug builds and a leak
    // in release builds.
    ASSERT(node());
    if (!node())
        return;

    animation()->cancelAnimations(this);

    if (RenderView* v = view())
        v->removeWidget(this);

    if (AXObjectCache::accessibilityEnabled()) {
        document()->axObjectCache()->childrenChanged(this->parent());
        document()->axObjectCache()->remove(this);
    }
    remove();

    if (m_widget) {
        if (m_frameView)
            m_frameView->removeChild(m_widget.get());
        widgetRendererMap().remove(m_widget.get());
    }
    
    // removes from override size map
    if (hasOverrideSize())
        setOverrideSize(-1);

    if (style() && (style()->height().isPercent() || style()->minHeight().isPercent() || style()->maxHeight().isPercent()))
        RenderBlock::removePercentHeightDescendant(this);

    if (hasLayer()) {
        layer()->clearClipRects();
        setHasLayer(false);
        destroyLayer();
    }

    // <rdar://problem/6937089> suggests that node() can be null here. One way this might happen is
    // if this function was re-entered (and therefore the null check at the beginning did not fail),
    // so bail out and turn a crash into an assertion failure in debug builds and a leak in release
    // builds.
    ASSERT(node());
    if (!node())
        return;

    // Grab the arena from node()->document()->renderArena() before clearing the node pointer.
    // Clear the node before deref-ing, as this may be deleted when deref is called.
    RenderArena* arena = renderArena();
    setNode(0);
    deref(arena);
}

RenderWidget::~RenderWidget()
{
    ASSERT(m_refCount <= 0);
    clearWidget();
}

void RenderWidget::setWidgetGeometry(const IntRect& frame)
{
    if (node() && m_widget->frameRect() != frame) {
        RenderWidgetProtector protector(this);
        RefPtr<Node> protectedNode(node());
        m_widget->setFrameRect(frame);
    }
}

void RenderWidget::setWidget(PassRefPtr<Widget> widget)
{
    if (widget != m_widget) {
        if (m_widget) {
            m_widget->removeFromParent();
            widgetRendererMap().remove(m_widget.get());
            clearWidget();
        }
        m_widget = widget;
        if (m_widget) {
            widgetRendererMap().add(m_widget.get(), this);
            // if we've already received a layout, apply the calculated space to the
            // widget immediately, but we have to have really been full constructed (with a non-null
            // style pointer).
            if (style()) {
                if (!needsLayout())
                    setWidgetGeometry(absoluteContentBox());
                if (style()->visibility() != VISIBLE)
                    m_widget->hide();
                else
                    m_widget->show();
            }
            m_frameView->addChild(m_widget.get());
        }
    }
}

void RenderWidget::layout()
{
    ASSERT(needsLayout());

    setNeedsLayout(false);
}

void RenderWidget::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderReplaced::styleDidChange(diff, oldStyle);
    if (m_widget) {
        if (style()->visibility() != VISIBLE)
            m_widget->hide();
        else
            m_widget->show();
    }
}

void RenderWidget::paint(PaintInfo& paintInfo, int tx, int ty)
{
    if (!shouldPaint(paintInfo, tx, ty))
        return;

    tx += x();
    ty += y();

    if (hasBoxDecorations() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection))
        paintBoxDecorations(paintInfo, tx, ty);

    if (paintInfo.phase == PaintPhaseMask) {
        paintMask(paintInfo, tx, ty);
        return;
    }

    if (!m_frameView || paintInfo.phase != PaintPhaseForeground || style()->visibility() != VISIBLE)
        return;

#if PLATFORM(MAC)
    if (style()->highlight() != nullAtom && !paintInfo.context->paintingDisabled())
        paintCustomHighlight(tx - x(), ty - y(), style()->highlight(), true);
#endif

    if (style()->hasBorderRadius()) {
        IntRect borderRect = IntRect(tx, ty, width(), height());

        if (borderRect.isEmpty())
            return;

        // Push a clip if we have a border radius, since we want to round the foreground content that gets painted.
        paintInfo.context->save();
        
        IntSize topLeft, topRight, bottomLeft, bottomRight;
        style()->getBorderRadiiForRect(borderRect, topLeft, topRight, bottomLeft, bottomRight);

        paintInfo.context->addRoundedRectClip(borderRect, topLeft, topRight, bottomLeft, bottomRight);
    }

    if (m_widget) {
        // Move the widget if necessary.  We normally move and resize widgets during layout, but sometimes
        // widgets can move without layout occurring (most notably when you scroll a document that
        // contains fixed positioned elements).
        m_widget->move(tx + borderLeft() + paddingLeft(), ty + borderTop() + paddingTop());

        // Tell the widget to paint now.  This is the only time the widget is allowed
        // to paint itself.  That way it will composite properly with z-indexed layers.
        m_widget->paint(paintInfo.context, paintInfo.rect);

        if (m_widget->isFrameView() && paintInfo.overlapTestRequests && !static_cast<FrameView*>(m_widget.get())->useSlowRepaints()) {
            ASSERT(!paintInfo.overlapTestRequests->contains(this));
            paintInfo.overlapTestRequests->set(this, m_widget->frameRect());
        }
    }

    if (style()->hasBorderRadius())
        paintInfo.context->restore();

    // Paint a partially transparent wash over selected widgets.
    if (isSelected() && !document()->printing()) {
        // FIXME: selectionRect() is in absolute, not painting coordinates.
        paintInfo.context->fillRect(selectionRect(), selectionBackgroundColor());
    }
}

void RenderWidget::setOverlapTestResult(bool isOverlapped)
{
    ASSERT(m_widget);
    ASSERT(m_widget->isFrameView());
    static_cast<FrameView*>(m_widget.get())->setIsOverlapped(isOverlapped);
}

void RenderWidget::deref(RenderArena *arena)
{
    if (--m_refCount <= 0)
        arenaDelete(arena, this);
}

void RenderWidget::updateWidgetPosition()
{
    if (!m_widget)
        return;

    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absPos = localToAbsolute();
    absPos.move(borderLeft() + paddingLeft(), borderTop() + paddingTop());

    int w = width() - borderLeft() - borderRight() - paddingLeft() - paddingRight();
    int h = height() - borderTop() - borderBottom() - paddingTop() - paddingBottom();

    IntRect newBounds(absPos.x(), absPos.y(), w, h);
    IntRect oldBounds(m_widget->frameRect());
    bool boundsChanged = newBounds != oldBounds;
    if (boundsChanged) {
        RenderWidgetProtector protector(this);
        RefPtr<Node> protectedNode(node());
        m_widget->setFrameRect(newBounds);
    }
    
    // if the frame bounds got changed, or if view needs layout (possibly indicating
    // content size is wrong) we have to do a layout to set the right widget size
    if (m_widget->isFrameView()) {
        FrameView* frameView = static_cast<FrameView*>(m_widget.get());
        if (boundsChanged || frameView->needsLayout())
            frameView->layout();
    }
}

void RenderWidget::setSelectionState(SelectionState state)
{
    if (selectionState() != state) {
        RenderReplaced::setSelectionState(state);
        if (m_widget)
            m_widget->setIsSelected(isSelected());
    }
}

void RenderWidget::clearWidget()
{
    m_widget = 0;
}

RenderWidget* RenderWidget::find(const Widget* widget)
{
    return widgetRendererMap().get(widget);
}

bool RenderWidget::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty, HitTestAction action)
{
    bool hadResult = result.innerNode();
    bool inside = RenderReplaced::nodeAtPoint(request, result, x, y, tx, ty, action);
    
    // Check to see if we are really over the widget itself (and not just in the border/padding area).
    if (inside && !hadResult && result.innerNode() == node())
        result.setIsOverWidget(contentBoxRect().contains(result.localPoint()));
    return inside;
}

} // namespace WebCore
