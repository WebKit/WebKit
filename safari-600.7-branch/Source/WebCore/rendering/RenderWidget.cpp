/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2009, 2010 Apple Inc. All rights reserved.
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

#include "AXObjectCache.h"
#include "FloatRoundedRect.h"
#include "Frame.h"
#include "HTMLFrameOwnerElement.h"
#include "HitTestResult.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderView.h"
#include <wtf/StackStats.h>
#include <wtf/Ref.h>

namespace WebCore {

static HashMap<const Widget*, RenderWidget*>& widgetRendererMap()
{
    static HashMap<const Widget*, RenderWidget*>* staticWidgetRendererMap = new HashMap<const Widget*, RenderWidget*>;
    return *staticWidgetRendererMap;
}

unsigned WidgetHierarchyUpdatesSuspensionScope::s_widgetHierarchyUpdateSuspendCount = 0;

WidgetHierarchyUpdatesSuspensionScope::WidgetToParentMap& WidgetHierarchyUpdatesSuspensionScope::widgetNewParentMap()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(WidgetToParentMap, map, ());
    return map;
}

void WidgetHierarchyUpdatesSuspensionScope::moveWidgets()
{
    WidgetToParentMap map;
    widgetNewParentMap().swap(map);
    WidgetToParentMap::iterator end = map.end();
    for (WidgetToParentMap::iterator it = map.begin(); it != end; ++it) {
        Widget* child = it->key.get();
        ScrollView* currentParent = child->parent();
        FrameView* newParent = it->value;
        if (newParent != currentParent) {
            if (currentParent)
                currentParent->removeChild(child);
            if (newParent)
                newParent->addChild(child);
        }
    }
}

static void moveWidgetToParentSoon(Widget* child, FrameView* parent)
{
    if (!WidgetHierarchyUpdatesSuspensionScope::isSuspended()) {
        if (parent)
            parent->addChild(child);
        else
            child->removeFromParent();
        return;
    }
    WidgetHierarchyUpdatesSuspensionScope::scheduleWidgetToMove(child, parent);
}

RenderWidget::RenderWidget(HTMLFrameOwnerElement& element, PassRef<RenderStyle> style)
    : RenderReplaced(element, WTF::move(style))
    , m_weakPtrFactory(this)
{
    setInline(false);
}

void RenderWidget::willBeDestroyed()
{
#if PLATFORM(IOS)
    if (hasLayer())
        layer()->willBeDestroyed();
#endif

    if (AXObjectCache* cache = document().existingAXObjectCache()) {
        cache->childrenChanged(this->parent());
        cache->remove(this);
    }

    setWidget(nullptr);

    RenderReplaced::willBeDestroyed();
}

RenderWidget::~RenderWidget()
{
}

// Widgets are always placed on integer boundaries, so rounding the size is actually
// the desired behavior. This function is here because it's otherwise seldom what we
// want to do with a LayoutRect.
static inline IntRect roundedIntRect(const LayoutRect& rect)
{
    return IntRect(roundedIntPoint(rect.location()), roundedIntSize(rect.size()));
}

bool RenderWidget::setWidgetGeometry(const LayoutRect& frame)
{
    IntRect clipRect = roundedIntRect(enclosingLayer()->childrenClipRect());
    IntRect newFrameRect = roundedIntRect(frame);
    IntRect oldFrameRect = m_widget->frameRect();
    bool clipChanged = m_clipRect != clipRect;
    bool boundsChanged = oldFrameRect != newFrameRect;

    if (!boundsChanged && !clipChanged)
        return false;

    m_clipRect = clipRect;

    WeakPtr<RenderWidget> weakThis = createWeakPtr();
    // These calls *may* cause this renderer to disappear from underneath...
    if (boundsChanged)
        m_widget->setFrameRect(newFrameRect);
    else if (clipChanged)
        m_widget->clipRectChanged();
    // ...so we follow up with a sanity check.
    if (!weakThis)
        return true;

    if (boundsChanged && hasLayer() && layer()->isComposited())
        layer()->backing()->updateAfterWidgetResize();

    return oldFrameRect.size() != newFrameRect.size();
}

bool RenderWidget::updateWidgetGeometry()
{
    if (!m_widget->transformsAffectFrameRect())
        return setWidgetGeometry(absoluteContentBox());

    LayoutRect contentBox = contentBoxRect();
    LayoutRect absoluteContentBox(localToAbsoluteQuad(FloatQuad(contentBox)).boundingBox());
    if (m_widget->isFrameView()) {
        contentBox.setLocation(absoluteContentBox.location());
        return setWidgetGeometry(contentBox);
    }

    return setWidgetGeometry(absoluteContentBox);
}

void RenderWidget::setWidget(PassRefPtr<Widget> widget)
{
    if (widget == m_widget)
        return;

    if (m_widget) {
        moveWidgetToParentSoon(m_widget.get(), 0);
        view().frameView().willRemoveWidgetFromRenderTree(*m_widget);
        widgetRendererMap().remove(m_widget.get());
        m_widget = nullptr;
    }
    m_widget = widget;
    if (m_widget) {
        widgetRendererMap().add(m_widget.get(), this);
        view().frameView().didAddWidgetToRenderTree(*m_widget);
        // If we've already received a layout, apply the calculated space to the
        // widget immediately, but we have to have really been fully constructed.
        if (hasInitializedStyle()) {
            if (!needsLayout()) {
                WeakPtr<RenderWidget> weakThis = createWeakPtr();
                updateWidgetGeometry();
                if (!weakThis)
                    return;
            }

            if (style().visibility() != VISIBLE)
                m_widget->hide();
            else {
                m_widget->show();
                repaint();
            }
        }
        moveWidgetToParentSoon(m_widget.get(), &view().frameView());
    }
}

void RenderWidget::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    clearNeedsLayout();
}

void RenderWidget::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderReplaced::styleDidChange(diff, oldStyle);
    if (m_widget) {
        if (style().visibility() != VISIBLE)
            m_widget->hide();
        else
            m_widget->show();
    }
}

void RenderWidget::paintContents(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + location();

    // Tell the widget to paint now. This is the only time the widget is allowed
    // to paint itself. That way it will composite properly with z-indexed layers.
    IntPoint widgetLocation = m_widget->frameRect().location();
    IntPoint paintLocation(roundToInt(adjustedPaintOffset.x() + borderLeft() + paddingLeft()),
        roundToInt(adjustedPaintOffset.y() + borderTop() + paddingTop()));
    LayoutRect paintRect = paintInfo.rect;

    LayoutSize widgetPaintOffset = paintLocation - widgetLocation;
    // When painting widgets into compositing layers, tx and ty are relative to the enclosing compositing layer,
    // not the root. In this case, shift the CTM and adjust the paintRect to be root-relative to fix plug-in drawing.
    if (!widgetPaintOffset.isZero()) {
        paintInfo.context->translate(widgetPaintOffset);
        paintRect.move(-widgetPaintOffset);
    }
    m_widget->paint(paintInfo.context, pixelSnappedIntRect(paintRect));

    if (!widgetPaintOffset.isZero())
        paintInfo.context->translate(-widgetPaintOffset);

    if (m_widget->isFrameView()) {
        FrameView* frameView = toFrameView(m_widget.get());
        bool runOverlapTests = !frameView->useSlowRepaintsIfNotOverlapped() || frameView->hasCompositedContentIncludingDescendants();
        if (paintInfo.overlapTestRequests && runOverlapTests) {
            ASSERT(!paintInfo.overlapTestRequests->contains(this));
            paintInfo.overlapTestRequests->set(this, m_widget->frameRect());
        }
    }
}

void RenderWidget::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!shouldPaint(paintInfo, paintOffset))
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + location();

    if (hasBoxDecorations() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection))
        paintBoxDecorations(paintInfo, adjustedPaintOffset);

    if (paintInfo.phase == PaintPhaseMask) {
        paintMask(paintInfo, adjustedPaintOffset);
        return;
    }

    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && hasOutline())
        paintOutline(paintInfo, LayoutRect(adjustedPaintOffset, size()));

    if (paintInfo.phase != PaintPhaseForeground)
        return;

    if (style().hasBorderRadius()) {
        LayoutRect borderRect = LayoutRect(adjustedPaintOffset, size());

        if (borderRect.isEmpty())
            return;

        // Push a clip if we have a border radius, since we want to round the foreground content that gets painted.
        paintInfo.context->save();
        FloatRoundedRect roundedInnerRect = FloatRoundedRect(style().getRoundedInnerBorderFor(borderRect,
            paddingTop() + borderTop(), paddingBottom() + borderBottom(), paddingLeft() + borderLeft(), paddingRight() + borderRight(), true, true));
        clipRoundedInnerRect(paintInfo.context, borderRect, roundedInnerRect);
    }

    if (m_widget)
        paintContents(paintInfo, paintOffset);

    if (style().hasBorderRadius())
        paintInfo.context->restore();

    // Paint a partially transparent wash over selected widgets.
    if (isSelected() && !document().printing()) {
        // FIXME: selectionRect() is in absolute, not painting coordinates.
        paintInfo.context->fillRect(pixelSnappedIntRect(selectionRect()), selectionBackgroundColor(), style().colorSpace());
    }

    if (hasLayer() && layer()->canResize())
        layer()->paintResizer(paintInfo.context, roundedIntPoint(adjustedPaintOffset), paintInfo.rect);
}

void RenderWidget::setOverlapTestResult(bool isOverlapped)
{
    ASSERT(m_widget);
    ASSERT(m_widget->isFrameView());
    toFrameView(m_widget.get())->setIsOverlapped(isOverlapped);
}

void RenderWidget::updateWidgetPosition()
{
    if (!m_widget)
        return;

    WeakPtr<RenderWidget> weakThis = createWeakPtr();
    bool widgetSizeChanged = updateWidgetGeometry();
    if (!weakThis)
        return;

    // if the frame size got changed, or if view needs layout (possibly indicating
    // content size is wrong) we have to do a layout to set the right widget size.
    if (m_widget->isFrameView()) {
        FrameView* frameView = toFrameView(m_widget.get());
        // Check the frame's page to make sure that the frame isn't in the process of being destroyed.
        if ((widgetSizeChanged || frameView->needsLayout()) && frameView->frame().page())
            frameView->layout();
    }
}

IntRect RenderWidget::windowClipRect() const
{
    return intersection(view().frameView().contentsToWindow(m_clipRect), view().frameView().windowClipRect());
}

void RenderWidget::setSelectionState(SelectionState state)
{
    // The selection state for our containing block hierarchy is updated by the base class call.
    RenderReplaced::setSelectionState(state);

    if (m_widget)
        m_widget->setIsSelected(isSelected());
}

RenderWidget* RenderWidget::find(const Widget* widget)
{
    return widgetRendererMap().get(widget);
}

bool RenderWidget::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    if (request.allowsChildFrameContent() && widget() && widget()->isFrameView() && toFrameView(widget())->renderView()) {
        FrameView* childFrameView = toFrameView(widget());
        RenderView* childRoot = childFrameView->renderView();

        LayoutPoint adjustedLocation = accumulatedOffset + location();
        LayoutPoint contentOffset = LayoutPoint(borderLeft() + paddingLeft(), borderTop() + paddingTop()) - childFrameView->scrollOffset();
        HitTestLocation newHitTestLocation(locationInContainer, -adjustedLocation - contentOffset);
        HitTestRequest newHitTestRequest(request.type() | HitTestRequest::ChildFrameHitTest);
        HitTestResult childFrameResult(newHitTestLocation);

        bool isInsideChildFrame = childRoot->hitTest(newHitTestRequest, newHitTestLocation, childFrameResult);

        if (newHitTestLocation.isRectBasedTest())
            result.append(childFrameResult);
        else if (isInsideChildFrame)
            result = childFrameResult;

        if (isInsideChildFrame)
            return true;
    }

    bool hadResult = result.innerNode();
    bool inside = RenderReplaced::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, action);

    // Check to see if we are really over the widget itself (and not just in the border/padding area).
    if ((inside || result.isRectBasedTest()) && !hadResult && result.innerNode() == &frameOwnerElement())
        result.setIsOverWidget(contentBoxRect().contains(result.localPoint()));
    return inside;
}

bool RenderWidget::requiresLayer() const
{
    return RenderReplaced::requiresLayer() || requiresAcceleratedCompositing();
}

bool RenderWidget::requiresAcceleratedCompositing() const
{
    // If this is a renderer with a contentDocument and that document needs a layer, then we need a layer.
    if (Document* contentDocument = frameOwnerElement().contentDocument()) {
        if (RenderView* view = contentDocument->renderView())
            return view->usesCompositing();
    }

    return false;
}

bool RenderWidget::needsPreferredWidthsRecalculation() const
{
    if (RenderReplaced::needsPreferredWidthsRecalculation())
        return true;
    return embeddedContentBox();
}

RenderBox* RenderWidget::embeddedContentBox() const
{
    if (!widget() || !widget()->isFrameView())
        return 0;
    return toFrameView(widget())->embeddedContentBox();
}

} // namespace WebCore
