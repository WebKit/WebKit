/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "RenderBox.h"

#include "CachedImage.h"
#include "ChromeClient.h"
#include "Document.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "Page.h"
#include "RenderArena.h"
#include "RenderFlexibleBox.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderReplica.h"
#include "RenderTableCell.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include <algorithm>
#include <math.h>

#if ENABLE(WML)
#include "WMLNames.h"
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;

// Used by flexible boxes when flexing this element.
typedef WTF::HashMap<const RenderBox*, int> OverrideSizeMap;
static OverrideSizeMap* gOverrideSizeMap = 0;

bool RenderBox::s_wasFloating = false;
bool RenderBox::s_hadOverflowClip = false;

RenderBox::RenderBox(Node* node)
    : RenderObject(node)
    , m_marginLeft(0)
    , m_marginRight(0)
    , m_marginTop(0)
    , m_marginBottom(0)
    , m_minPrefWidth(-1)
    , m_maxPrefWidth(-1)
    , m_layer(0)
    , m_inlineBoxWrapper(0)
{
    setIsBox();
}

RenderBox::~RenderBox()
{
}

void RenderBox::destroy()
{
    // A lot of the code in this function is just pasted into
    // RenderWidget::destroy. If anything in this function changes,
    // be sure to fix RenderWidget::destroy() as well.
    if (hasOverrideSize())
        gOverrideSizeMap->remove(this);

    // This must be done before we destroy the RenderObject.
    if (m_layer)
        m_layer->clearClipRects();

    if (style() && (style()->height().isPercent() || style()->minHeight().isPercent() || style()->maxHeight().isPercent()))
        RenderBlock::removePercentHeightDescendant(this);

    RenderObject::destroy();
}

void RenderBox::styleWillChange(RenderStyle::Diff diff, const RenderStyle* newStyle)
{
    s_wasFloating = isFloating();
    s_hadOverflowClip = hasOverflowClip();

    if (style()) {
        // If our z-index changes value or our visibility changes,
        // we need to dirty our stacking context's z-order list.
        if (newStyle) {
            if (hasLayer() && (style()->hasAutoZIndex() != newStyle->hasAutoZIndex() ||
                               style()->zIndex() != newStyle->zIndex() ||
                               style()->visibility() != newStyle->visibility())) {
                layer()->dirtyStackingContextZOrderLists();
                if (style()->hasAutoZIndex() != newStyle->hasAutoZIndex() || style()->visibility() != newStyle->visibility())
                    layer()->dirtyZOrderLists();
            }
        }
        
        // The background of the root element or the body element could propagate up to
        // the canvas.  Just dirty the entire canvas when our style changes substantially.
        if (diff >= RenderStyle::Repaint && element() &&
                (element()->hasTagName(htmlTag) || element()->hasTagName(bodyTag)))
            view()->repaint();
        else if (parent() && !isText()) {
            // Do a repaint with the old style first, e.g., for example if we go from
            // having an outline to not having an outline.
            if (diff == RenderStyle::RepaintLayer) {
                layer()->repaintIncludingDescendants();
                if (!(style()->clip() == newStyle->clip()))
                    layer()->clearClipRectsIncludingDescendants();
            } else if (diff == RenderStyle::Repaint || newStyle->outlineSize() < style()->outlineSize())
                repaint();
        }

        if (diff == RenderStyle::Layout) {
            // When a layout hint happens, we go ahead and do a repaint of the layer, since the layer could
            // end up being destroyed.
            if (hasLayer()) {
                if (style()->position() != newStyle->position() ||
                    style()->zIndex() != newStyle->zIndex() ||
                    style()->hasAutoZIndex() != newStyle->hasAutoZIndex() ||
                    !(style()->clip() == newStyle->clip()) ||
                    style()->hasClip() != newStyle->hasClip() ||
                    style()->opacity() != newStyle->opacity() ||
                    style()->transform() != newStyle->transform())
                layer()->repaintIncludingDescendants();
            } else if (newStyle->hasTransform() || newStyle->opacity() < 1) {
                // If we don't have a layer yet, but we are going to get one because of transform or opacity,
                //  then we need to repaint the old position of the object.
                repaint();
            }
        
            // When a layout hint happens and an object's position style changes, we have to do a layout
            // to dirty the render tree using the old position value now.
            if (parent() && style()->position() != newStyle->position()) {
                markContainingBlocksForLayout();
                if (style()->position() == StaticPosition)
                    repaint();
                if (isFloating() && !isPositioned() && (newStyle->position() == AbsolutePosition || newStyle->position() == FixedPosition))
                    removeFromObjectLists();
            }
        }
    }

    RenderObject::styleWillChange(diff, newStyle);
}

void RenderBox::styleDidChange(RenderStyle::Diff diff, const RenderStyle* oldStyle)
{
    // We need to ensure that view->maximalOutlineSize() is valid for any repaints that happen
    // during the style change (it's used by clippedOverflowRectForRepaint()).
    if (style()->outlineWidth() > 0 && style()->outlineSize() > maximalOutlineSize(PaintPhaseOutline))
        static_cast<RenderView*>(document()->renderer())->setMaximalOutlineSize(style()->outlineSize());

    RenderObject::styleDidChange(diff, oldStyle);

    if (needsLayout() && oldStyle && (oldStyle->height().isPercent() || oldStyle->minHeight().isPercent() || oldStyle->maxHeight().isPercent()))
        RenderBlock::removePercentHeightDescendant(this);

    bool isRootObject = isRoot();
    bool isViewObject = isRenderView();

    // The root and the RenderView always paint their backgrounds/borders.
    if (isRootObject || isViewObject)
        setHasBoxDecorations(true);

    setInline(style()->isDisplayInlineType());

    switch (style()->position()) {
        case AbsolutePosition:
        case FixedPosition:
            setPositioned(true);
            break;
        default:
            setPositioned(false);

            if (style()->isFloating())
                setFloating(true);

            if (style()->position() == RelativePosition)
                setRelPositioned(true);
            break;
    }

    // We also handle <body> and <html>, whose overflow applies to the viewport.
    if (style()->overflowX() != OVISIBLE && !isRootObject && (isRenderBlock() || isTableRow() || isTableSection())) {
        bool boxHasOverflowClip = true;
        if (isBody()) {
            // Overflow on the body can propagate to the viewport under the following conditions.
            // (1) The root element is <html>.
            // (2) We are the primary <body> (can be checked by looking at document.body).
            // (3) The root element has visible overflow.
            if (document()->documentElement()->hasTagName(htmlTag) &&
                document()->body() == element() &&
                document()->documentElement()->renderer()->style()->overflowX() == OVISIBLE)
                boxHasOverflowClip = false;
        }
        
        // Check for overflow clip.
        // It's sufficient to just check one direction, since it's illegal to have visible on only one overflow value.
        if (boxHasOverflowClip) {
            if (!s_hadOverflowClip)
                // Erase the overflow
                repaint();
            setHasOverflowClip();
        }
    }

    setHasTransform(style()->hasTransform());
    setHasReflection(style()->boxReflect());

    if (requiresLayer()) {
        if (!m_layer) {
            if (s_wasFloating && isFloating())
                setChildNeedsLayout(true);
            m_layer = new (renderArena()) RenderLayer(this);
            setHasLayer(true);
            m_layer->insertOnlyThisLayer();
            if (parent() && !needsLayout() && containingBlock())
                m_layer->updateLayerPositions();
        }
    } else if (m_layer && !isRootObject && !isViewObject) {
        ASSERT(m_layer->parent());
        RenderLayer* layer = m_layer;
        m_layer = 0;
        setHasLayer(false);
        setHasTransform(false); // Either a transform wasn't specified or the object doesn't support transforms, so just null out the bit.
        setHasReflection(false);
        layer->removeOnlyThisLayer();
        if (s_wasFloating && isFloating())
            setChildNeedsLayout(true);
    }

    // If our zoom factor changes and we have a defined scrollLeft/Top, we need to adjust that value into the
    // new zoomed coordinate space.
    if (hasOverflowClip() && oldStyle && style() && oldStyle->effectiveZoom() != style()->effectiveZoom()) {
        int left = scrollLeft();
        if (left) {
            left = (left / oldStyle->effectiveZoom()) * style()->effectiveZoom();
            setScrollLeft(left);
        }
        int top = scrollTop();
        if (top) {
            top = (top / oldStyle->effectiveZoom()) * style()->effectiveZoom();
            setScrollTop(top);
        }
    }

    if (m_layer)
        m_layer->styleChanged(diff, oldStyle);

    // Set the text color if we're the body.
    if (isBody())
        document()->setTextColor(style()->color());
}


int RenderBox::offsetLeft() const
{
    RenderBox* offsetPar = offsetParent();
    if (!offsetPar)
        return 0;
    int xPos = x() - offsetPar->borderLeft();
    if (!isPositioned()) {
        if (isRelPositioned())
            xPos += relativePositionOffsetX();
        RenderObject* curr = parent();
        while (curr && curr != offsetPar) {
            // FIXME: What are we supposed to do inside SVG content?
            if (curr->isBox() && !curr->isTableRow())
                xPos += toRenderBox(curr)->x();
            curr = curr->parent();
        }
        if (offsetPar->isBody() && !offsetPar->isRelPositioned() && !offsetPar->isPositioned())
            xPos += offsetPar->x();
    }
    return xPos;
}

int RenderBox::offsetTop() const
{
    RenderBox* offsetPar = offsetParent();
    if (!offsetPar)
        return 0;
    int yPos = y() - offsetPar->borderTop();
    if (!isPositioned()) {
        if (isRelPositioned())
            yPos += relativePositionOffsetY();
        RenderObject* curr = parent();
        while (curr && curr != offsetPar) {
            // FIXME: What are we supposed to do inside SVG content?
            if (curr->isBox() && !curr->isTableRow())
                yPos += toRenderBox(curr)->y();
            curr = curr->parent();
        }
        if (offsetPar->isBody() && !offsetPar->isRelPositioned() && !offsetPar->isPositioned())
            yPos += offsetPar->y();
    }
    return yPos;
}

RenderBox* RenderBox::offsetParent() const
{
    // FIXME: It feels like this function could almost be written using containing blocks.
    if (isBody())
        return 0;

    bool skipTables = isPositioned() || isRelPositioned();
    float currZoom = style()->effectiveZoom();
    RenderObject* curr = parent();
    while (curr && (!curr->element() ||
                    (!curr->isPositioned() && !curr->isRelPositioned() && !curr->isBody()))) {
        Node* element = curr->element();
        if (!skipTables && element) {
            bool isTableElement = element->hasTagName(tableTag) ||
                                  element->hasTagName(tdTag) ||
                                  element->hasTagName(thTag);

#if ENABLE(WML)
            if (!isTableElement && element->isWMLElement())
                isTableElement = element->hasTagName(WMLNames::tableTag) ||
                                 element->hasTagName(WMLNames::tdTag);
#endif

            if (isTableElement)
                break;
        }

        float newZoom = curr->style()->effectiveZoom();
        if (currZoom != newZoom)
            break;
        currZoom = newZoom;
        curr = curr->parent();
    }
    return curr && curr->isBox() ? toRenderBox(curr) : 0;
}

// More IE extensions.  clientWidth and clientHeight represent the interior of an object
// excluding border and scrollbar.
int RenderBox::clientWidth() const
{
    if (isRenderInline())
        return 0;
    return width() - borderLeft() - borderRight() - verticalScrollbarWidth();
}

int RenderBox::clientHeight() const
{
    if (isRenderInline())
        return 0;
    return height() - borderTop() - borderBottom() - horizontalScrollbarHeight();
}

// scrollWidth/scrollHeight will be the same as clientWidth/clientHeight unless the
// object has overflow:hidden/scroll/auto specified and also has overflow.
int RenderBox::scrollWidth() const
{
    return hasOverflowClip() ? m_layer->scrollWidth() : clientWidth();
}

int RenderBox::scrollHeight() const
{
    return hasOverflowClip() ? m_layer->scrollHeight() : clientHeight();
}

int RenderBox::scrollLeft() const
{
    return hasOverflowClip() ? m_layer->scrollXOffset() : 0;
}

int RenderBox::scrollTop() const
{
    return hasOverflowClip() ? m_layer->scrollYOffset() : 0;
}

void RenderBox::setScrollLeft(int newLeft)
{
    if (hasOverflowClip())
        m_layer->scrollToXOffset(newLeft);
}

void RenderBox::setScrollTop(int newTop)
{
    if (hasOverflowClip())
        m_layer->scrollToYOffset(newTop);
}

int RenderBox::paddingTop(bool) const
{
    int w = 0;
    Length padding = style()->paddingTop();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

int RenderBox::paddingBottom(bool) const
{
    int w = 0;
    Length padding = style()->paddingBottom();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

int RenderBox::paddingLeft(bool) const
{
    int w = 0;
    Length padding = style()->paddingLeft();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

int RenderBox::paddingRight(bool) const
{
    int w = 0;
    Length padding = style()->paddingRight();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

void RenderBox::absoluteRects(Vector<IntRect>& rects, int tx, int ty, bool topLevel)
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    RenderFlow* continuation = virtualContinuation();
    if (topLevel && continuation) {
        rects.append(IntRect(tx, ty - collapsedMarginTop(),
                             width(), height() + collapsedMarginTop() + collapsedMarginBottom()));
        continuation->absoluteRects(rects,
                                    tx - x() + continuation->containingBlock()->x(),
                                    ty - y() + continuation->containingBlock()->y(), topLevel);
    } else
        rects.append(IntRect(tx, ty, width(), height()));
}

void RenderBox::absoluteQuads(Vector<FloatQuad>& quads, bool topLevel)
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    RenderFlow* continuation = virtualContinuation();
    if (topLevel && continuation) {
        FloatRect localRect(0, -collapsedMarginTop(),
                            width(), height() + collapsedMarginTop() + collapsedMarginBottom());
        quads.append(localToAbsoluteQuad(localRect));
        continuation->absoluteQuads(quads, topLevel);
    } else
        quads.append(localToAbsoluteQuad(FloatRect(0, 0, width(), height())));
}

IntRect RenderBox::absoluteContentBox() const
{
    IntRect rect = contentBoxRect();
    FloatPoint absPos = localToAbsolute(FloatPoint());
    rect.move(absPos.x(), absPos.y());
    return rect;
}

FloatQuad RenderBox::absoluteContentQuad() const
{
    IntRect rect = contentBoxRect();
    return localToAbsoluteQuad(FloatRect(rect));
}


IntRect RenderBox::outlineBoundsForRepaint(RenderBox* /*repaintContainer*/) const
{
    IntRect box = borderBoundingBox();
    adjustRectForOutlineAndShadow(box);

    FloatQuad absOutlineQuad = localToAbsoluteQuad(FloatRect(box));
    box = absOutlineQuad.enclosingBoundingBox();

    // FIXME: layoutDelta needs to be applied in parts before/after transforms and
    // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
    box.move(view()->layoutDelta());

    return box;
}

void RenderBox::addFocusRingRects(GraphicsContext* graphicsContext, int tx, int ty)
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    RenderFlow* continuation = virtualContinuation();
    if (continuation) {
        graphicsContext->addFocusRingRect(IntRect(tx, ty - collapsedMarginTop(), width(), height() + collapsedMarginTop() + collapsedMarginBottom()));
        continuation->addFocusRingRects(graphicsContext,
                                        tx - x() + continuation->containingBlock()->x(),
                                        ty - y() + continuation->containingBlock()->y());
    } else
        graphicsContext->addFocusRingRect(IntRect(tx, ty, width(), height()));
}


IntRect RenderBox::reflectionBox() const
{
    IntRect result;
    if (!style()->boxReflect())
        return result;
    IntRect box = borderBoxRect();
    result = box;
    switch (style()->boxReflect()->direction()) {
        case ReflectionBelow:
            result.move(0, box.height() + reflectionOffset());
            break;
        case ReflectionAbove:
            result.move(0, -box.height() - reflectionOffset());
            break;
        case ReflectionLeft:
            result.move(-box.width() - reflectionOffset(), 0);
            break;
        case ReflectionRight:
            result.move(box.width() + reflectionOffset(), 0);
            break;
    }
    return result;
}

int RenderBox::reflectionOffset() const
{
    if (!style()->boxReflect())
        return 0;
    if (style()->boxReflect()->direction() == ReflectionLeft || style()->boxReflect()->direction() == ReflectionRight)
        return style()->boxReflect()->offset().calcValue(borderBoxRect().width());
    return style()->boxReflect()->offset().calcValue(borderBoxRect().height());
}

IntRect RenderBox::reflectedRect(const IntRect& r) const
{
    if (!style()->boxReflect())
        return IntRect();

    IntRect box = borderBoxRect();
    IntRect result = r;
    switch (style()->boxReflect()->direction()) {
        case ReflectionBelow:
            result.setY(box.bottom() + reflectionOffset() + (box.bottom() - r.bottom()));
            break;
        case ReflectionAbove:
            result.setY(box.y() - reflectionOffset() - box.height() + (box.bottom() - r.bottom()));
            break;
        case ReflectionLeft:
            result.setX(box.x() - reflectionOffset() - box.width() + (box.right() - r.right()));
            break;
        case ReflectionRight:
            result.setX(box.right() + reflectionOffset() + (box.right() - r.right()));
            break;
    }
    return result;
}

int RenderBox::verticalScrollbarWidth() const
{
    return includeVerticalScrollbarSize() ? layer()->verticalScrollbarWidth() : 0;
}

int RenderBox::horizontalScrollbarHeight() const
{
    return includeHorizontalScrollbarSize() ? layer()->horizontalScrollbarHeight() : 0;
}

bool RenderBox::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    RenderLayer* l = layer();
    if (l && l->scroll(direction, granularity, multiplier))
        return true;
    RenderBlock* b = containingBlock();
    if (b && !b->isRenderView())
        return b->scroll(direction, granularity, multiplier);
    return false;
}
    
bool RenderBox::canBeProgramaticallyScrolled(bool) const
{
    return (hasOverflowClip() && (scrollsOverflow() || (node() && node()->isContentEditable()))) || (node() && node()->isDocumentNode());
}

void RenderBox::autoscroll()
{
    if (layer())
        layer()->autoscroll();
}

void RenderBox::panScroll(const IntPoint& source)
{
    if (layer())
        layer()->panScrollFromPoint(source);
}

int RenderBox::minPrefWidth() const
{
    if (prefWidthsDirty())
        const_cast<RenderBox*>(this)->calcPrefWidths();
        
    return m_minPrefWidth;
}

int RenderBox::maxPrefWidth() const
{
    if (prefWidthsDirty())
        const_cast<RenderBox*>(this)->calcPrefWidths();
        
    return m_maxPrefWidth;
}

int RenderBox::overrideSize() const
{
    if (!hasOverrideSize())
        return -1;
    return gOverrideSizeMap->get(this);
}

void RenderBox::setOverrideSize(int s)
{
    if (s == -1) {
        if (hasOverrideSize()) {
            setHasOverrideSize(false);
            gOverrideSizeMap->remove(this);
        }
    } else {
        if (!gOverrideSizeMap)
            gOverrideSizeMap = new OverrideSizeMap();
        setHasOverrideSize(true);
        gOverrideSizeMap->set(this, s);
    }
}

int RenderBox::overrideWidth() const
{
    return hasOverrideSize() ? overrideSize() : width();
}

int RenderBox::overrideHeight() const
{
    return hasOverrideSize() ? overrideSize() : height();
}

int RenderBox::calcBorderBoxWidth(int width) const
{
    int bordersPlusPadding = borderLeft() + borderRight() + paddingLeft() + paddingRight();
    if (style()->boxSizing() == CONTENT_BOX)
        return width + bordersPlusPadding;
    return max(width, bordersPlusPadding);
}

int RenderBox::calcBorderBoxHeight(int height) const
{
    int bordersPlusPadding = borderTop() + borderBottom() + paddingTop() + paddingBottom();
    if (style()->boxSizing() == CONTENT_BOX)
        return height + bordersPlusPadding;
    return max(height, bordersPlusPadding);
}

int RenderBox::calcContentBoxWidth(int width) const
{
    if (style()->boxSizing() == BORDER_BOX)
        width -= (borderLeft() + borderRight() + paddingLeft() + paddingRight());
    return max(0, width);
}

int RenderBox::calcContentBoxHeight(int height) const
{
    if (style()->boxSizing() == BORDER_BOX)
        height -= (borderTop() + borderBottom() + paddingTop() + paddingBottom());
    return max(0, height);
}

// Hit Testing
bool RenderBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int xPos, int yPos, int tx, int ty, HitTestAction action)
{
    tx += x();
    ty += y();

    // Check kids first.
    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        // FIXME: We have to skip over inline flows, since they can show up inside table rows
        // at the moment (a demoted inline <form> for example). If we ever implement a
        // table-specific hit-test method (which we should do for performance reasons anyway),
        // then we can remove this check.
        if (!child->hasLayer() && !child->isRenderInline() && child->nodeAtPoint(request, result, xPos, yPos, tx, ty, action)) {
            updateHitTestResult(result, IntPoint(xPos - tx, yPos - ty));
            return true;
        }
    }

    // Check our bounds next. For this purpose always assume that we can only be hit in the
    // foreground phase (which is true for replaced elements like images).
    if (visibleToHitTesting() && action == HitTestForeground && IntRect(tx, ty, width(), height()).contains(xPos, yPos)) {
        updateHitTestResult(result, IntPoint(xPos - tx, yPos - ty));
        return true;
    }

    return false;
}

// --------------------- painting stuff -------------------------------

void RenderBox::paint(PaintInfo& paintInfo, int tx, int ty)
{
    tx += x();
    ty += y();

    // default implementation. Just pass paint through to the children
    PaintInfo childInfo(paintInfo);
    childInfo.paintingRoot = paintingRootForChildren(paintInfo);
    for (RenderObject* child = firstChild(); child; child = child->nextSibling())
        child->paint(childInfo, tx, ty);
}

void RenderBox::paintRootBoxDecorations(PaintInfo& paintInfo, int tx, int ty)
{
    const FillLayer* bgLayer = style()->backgroundLayers();
    Color bgColor = style()->backgroundColor();
    if (!style()->hasBackground() && element() && element()->hasTagName(HTMLNames::htmlTag)) {
        // Locate the <body> element using the DOM.  This is easier than trying
        // to crawl around a render tree with potential :before/:after content and
        // anonymous blocks created by inline <body> tags etc.  We can locate the <body>
        // render object very easily via the DOM.
        HTMLElement* body = document()->body();
        RenderObject* bodyObject = (body && body->hasLocalName(bodyTag)) ? body->renderer() : 0;
        if (bodyObject) {
            bgLayer = bodyObject->style()->backgroundLayers();
            bgColor = bodyObject->style()->backgroundColor();
        }
    }

    int w = width();
    int h = height();

    int rw;
    int rh;
    if (view()->frameView()) {
        rw = view()->frameView()->contentsWidth();
        rh = view()->frameView()->contentsHeight();
    } else {
        rw = view()->width();
        rh = view()->height();
    }

    // CSS2 14.2:
    // The background of the box generated by the root element covers the entire canvas including
    // its margins.
    int bx = tx - marginLeft();
    int by = ty - marginTop();
    int bw = max(w + marginLeft() + marginRight() + borderLeft() + borderRight(), rw);
    int bh = max(h + marginTop() + marginBottom() + borderTop() + borderBottom(), rh);

    int my = max(by, paintInfo.rect.y());

    paintFillLayers(paintInfo, bgColor, bgLayer, my, paintInfo.rect.height(), bx, by, bw, bh);

    if (style()->hasBorder() && style()->display() != INLINE)
        paintBorder(paintInfo.context, tx, ty, w, h, style());
}

void RenderBox::paintBoxDecorations(PaintInfo& paintInfo, int tx, int ty)
{
    if (!shouldPaintWithinRoot(paintInfo))
        return;

    if (isRoot()) {
        paintRootBoxDecorations(paintInfo, tx, ty);
        return;
    }

    int w = width();
    int h = height();

    // border-fit can adjust where we paint our border and background.  If set, we snugly fit our line box descendants.  (The iChat
    // balloon layout is an example of this).
    borderFitAdjust(tx, w);

    int my = max(ty, paintInfo.rect.y());
    int mh;
    if (ty < paintInfo.rect.y())
        mh = max(0, h - (paintInfo.rect.y() - ty));
    else
        mh = min(paintInfo.rect.height(), h);

    // FIXME: Should eventually give the theme control over whether the box shadow should paint, since controls could have
    // custom shadows of their own.
    paintBoxShadow(paintInfo.context, tx, ty, w, h, style());

    // If we have a native theme appearance, paint that before painting our background.
    // The theme will tell us whether or not we should also paint the CSS background.
    bool themePainted = style()->hasAppearance() && !theme()->paint(this, paintInfo, IntRect(tx, ty, w, h));
    if (!themePainted) {
        // The <body> only paints its background if the root element has defined a background
        // independent of the body.  Go through the DOM to get to the root element's render object,
        // since the root could be inline and wrapped in an anonymous block.
        if (!isBody() || document()->documentElement()->renderer()->style()->hasBackground())
            paintFillLayers(paintInfo, style()->backgroundColor(), style()->backgroundLayers(), my, mh, tx, ty, w, h);
        if (style()->hasAppearance())
            theme()->paintDecorations(this, paintInfo, IntRect(tx, ty, w, h));
    }

    // The theme will tell us whether or not we should also paint the CSS border.
    if ((!style()->hasAppearance() || (!themePainted && theme()->paintBorderOnly(this, paintInfo, IntRect(tx, ty, w, h)))) && style()->hasBorder())
        paintBorder(paintInfo.context, tx, ty, w, h, style());
}

void RenderBox::paintMask(PaintInfo& paintInfo, int tx, int ty)
{
    if (!shouldPaintWithinRoot(paintInfo) || style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    int w = width();
    int h = height();

    // border-fit can adjust where we paint our border and background.  If set, we snugly fit our line box descendants.  (The iChat
    // balloon layout is an example of this).
    borderFitAdjust(tx, w);

    int my = max(ty, paintInfo.rect.y());
    int mh;
    if (ty < paintInfo.rect.y())
        mh = max(0, h - (paintInfo.rect.y() - ty));
    else
        mh = min(paintInfo.rect.height(), h);

    paintMaskImages(paintInfo, my, mh, tx, ty, w, h);
}

void RenderBox::paintMaskImages(const PaintInfo& paintInfo, int my, int mh, int tx, int ty, int w, int h)
{
    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    StyleImage* maskBoxImage = style()->maskBoxImage().image();
    if ((maskBoxImage && style()->maskLayers()->hasImage()) || style()->maskLayers()->next())
        // We have to use an extra image buffer to hold the mask. Multiple mask images need
        // to composite together using source-over so that they can then combine into a single unified mask that
        // can be composited with the content using destination-in.  SVG images need to be able to set compositing modes
        // as they draw images contained inside their sub-document, so we paint all our images into a separate buffer
        // and composite that buffer as the mask.
        pushTransparencyLayer = true;
    
    CompositeOperator compositeOp = CompositeDestinationIn;
    if (pushTransparencyLayer) {
        paintInfo.context->setCompositeOperation(CompositeDestinationIn);
        paintInfo.context->beginTransparencyLayer(1.0f);
        compositeOp = CompositeSourceOver;
    }

    paintFillLayers(paintInfo, Color(), style()->maskLayers(), my, mh, tx, ty, w, h, compositeOp);
    paintNinePieceImage(paintInfo.context, tx, ty, w, h, style(), style()->maskBoxImage(), compositeOp);
    
    if (pushTransparencyLayer)
        paintInfo.context->endTransparencyLayer();
}

IntRect RenderBox::maskClipRect()
{
    IntRect bbox = borderBoxRect();
    if (style()->maskBoxImage().image())
        return bbox;
    
    IntRect result;
    for (const FillLayer* maskLayer = style()->maskLayers(); maskLayer; maskLayer = maskLayer->next()) {
        if (maskLayer->image()) {
            IntRect maskRect;
            IntPoint phase;
            IntSize tileSize;
            calculateBackgroundImageGeometry(maskLayer, bbox.x(), bbox.y(), bbox.width(), bbox.height(), maskRect, phase, tileSize);
            result.unite(maskRect);
        }
    }
    return result;
}

void RenderBox::paintFillLayers(const PaintInfo& paintInfo, const Color& c, const FillLayer* fillLayer,
                                int clipY, int clipH, int tx, int ty, int width, int height, CompositeOperator op)
{
    if (!fillLayer)
        return;

    paintFillLayers(paintInfo, c, fillLayer->next(), clipY, clipH, tx, ty, width, height, op);
    paintFillLayer(paintInfo, c, fillLayer, clipY, clipH, tx, ty, width, height, op);
}

void RenderBox::paintFillLayer(const PaintInfo& paintInfo, const Color& c, const FillLayer* fillLayer,
                               int clipY, int clipH, int tx, int ty, int width, int height, CompositeOperator op)
{
    paintFillLayerExtended(paintInfo, c, fillLayer, clipY, clipH, tx, ty, width, height, 0, op);
}

IntSize RenderBox::calculateBackgroundSize(const FillLayer* bgLayer, int scaledWidth, int scaledHeight) const
{
    StyleImage* bg = bgLayer->image();
    bg->setImageContainerSize(IntSize(scaledWidth, scaledHeight)); // Use the box established by background-origin.

    if (bgLayer->isSizeSet()) {
        int w = scaledWidth;
        int h = scaledHeight;
        Length bgWidth = bgLayer->size().width();
        Length bgHeight = bgLayer->size().height();

        if (bgWidth.isFixed())
            w = bgWidth.value();
        else if (bgWidth.isPercent())
            w = bgWidth.calcValue(scaledWidth);
        
        if (bgHeight.isFixed())
            h = bgHeight.value();
        else if (bgHeight.isPercent())
            h = bgHeight.calcValue(scaledHeight);
        
        // If one of the values is auto we have to use the appropriate
        // scale to maintain our aspect ratio.
        if (bgWidth.isAuto() && !bgHeight.isAuto())
            w = bg->imageSize(this, style()->effectiveZoom()).width() * h / bg->imageSize(this, style()->effectiveZoom()).height();        
        else if (!bgWidth.isAuto() && bgHeight.isAuto())
            h = bg->imageSize(this, style()->effectiveZoom()).height() * w / bg->imageSize(this, style()->effectiveZoom()).width();
        else if (bgWidth.isAuto() && bgHeight.isAuto()) {
            // If both width and height are auto, we just want to use the image's
            // intrinsic size.
            w = bg->imageSize(this, style()->effectiveZoom()).width();
            h = bg->imageSize(this, style()->effectiveZoom()).height();
        }
        
        return IntSize(max(1, w), max(1, h));
    } else
        return bg->imageSize(this, style()->effectiveZoom());
}

void RenderBox::imageChanged(WrappedImagePtr image, const IntRect*)
{
    if (!parent())
        return;

    if (isRenderInline() || style()->borderImage().image() && style()->borderImage().image()->data() == image ||
        style()->maskBoxImage().image() && style()->maskBoxImage().image()->data() == image) {
        repaint();
        return;
    }

    bool didFullRepaint = repaintLayerRectsForImage(image, style()->backgroundLayers(), true);
    if (!didFullRepaint) {
        repaintLayerRectsForImage(image, style()->maskLayers(), false);
    }
}

bool RenderBox::repaintLayerRectsForImage(WrappedImagePtr image, const FillLayer* layers, bool drawingBackground)
{
    IntRect rendererRect;
    RenderBox* layerRenderer = 0;

    for (const FillLayer* curLayer = layers; curLayer; curLayer = curLayer->next()) {
        if (curLayer->image() && image == curLayer->image()->data() && curLayer->image()->canRender(style()->effectiveZoom())) {
            // Now that we know this image is being used, compute the renderer and the rect
            // if we haven't already
            if (!layerRenderer) {
                bool drawingRootBackground = drawingBackground && (isRoot() || (isBody() && !document()->documentElement()->renderer()->style()->hasBackground()));
                if (drawingRootBackground) {
                    layerRenderer = view();

                    int rw;
                    int rh;

                    if (FrameView* frameView = static_cast<RenderView*>(layerRenderer)->frameView()) {
                        rw = frameView->contentsWidth();
                        rh = frameView->contentsHeight();
                    } else {
                        rw = layerRenderer->width();
                        rh = layerRenderer->height();
                    }
                    rendererRect = IntRect(-layerRenderer->marginLeft(),
                        -layerRenderer->marginTop(),
                        max(layerRenderer->width() + layerRenderer->marginLeft() + layerRenderer->marginRight() + layerRenderer->borderLeft() + layerRenderer->borderRight(), rw),
                        max(layerRenderer->height() + layerRenderer->marginTop() + layerRenderer->marginBottom() + layerRenderer->borderTop() + layerRenderer->borderBottom(), rh));
                } else {
                    layerRenderer = this;
                    rendererRect = borderBoxRect();
                }
            }

            IntRect repaintRect;
            IntPoint phase;
            IntSize tileSize;
            layerRenderer->calculateBackgroundImageGeometry(curLayer, rendererRect.x(), rendererRect.y(), rendererRect.width(), rendererRect.height(), repaintRect, phase, tileSize);
            layerRenderer->repaintRectangle(repaintRect);
            if (repaintRect == rendererRect)
                return true;
        }
    }
    return false;
}

void RenderBox::calculateBackgroundImageGeometry(const FillLayer* bgLayer, int tx, int ty, int w, int h, IntRect& destRect, IntPoint& phase, IntSize& tileSize)
{
    int pw;
    int ph;
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
    int cx;
    int cy;
    int rw = 0;
    int rh = 0;

    // CSS2 chapter 14.2.1

    if (bgLayer->attachment()) {
        // Scroll
        if (bgLayer->origin() != BorderFillBox) {
            left = borderLeft();
            right = borderRight();
            top = borderTop();
            bottom = borderBottom();
            if (bgLayer->origin() == ContentFillBox) {
                left += paddingLeft();
                right += paddingRight();
                top += paddingTop();
                bottom += paddingBottom();
            }
        }
        
        // The background of the box generated by the root element covers the entire canvas including
        // its margins.  Since those were added in already, we have to factor them out when computing the
        // box used by background-origin/size/position.
        if (isRoot()) {
            rw = width() - left - right;
            rh = height() - top - bottom; 
            left += marginLeft();
            right += marginRight();
            top += marginTop();
            bottom += marginBottom();
        }
        cx = tx;
        cy = ty;
        pw = w - left - right;
        ph = h - top - bottom;
    } else {
        // Fixed
        IntRect vr = viewRect();
        cx = vr.x();
        cy = vr.y();
        pw = vr.width();
        ph = vr.height();
    }

    int sx = 0;
    int sy = 0;
    int cw;
    int ch;

    IntSize scaledImageSize;
    if (isRoot() && bgLayer->attachment())
        scaledImageSize = calculateBackgroundSize(bgLayer, rw, rh);
    else
        scaledImageSize = calculateBackgroundSize(bgLayer, pw, ph);
        
    int scaledImageWidth = scaledImageSize.width();
    int scaledImageHeight = scaledImageSize.height();

    EFillRepeat backgroundRepeat = bgLayer->repeat();
    
    int xPosition;
    if (isRoot() && bgLayer->attachment())
        xPosition = bgLayer->xPosition().calcMinValue(rw - scaledImageWidth, true);
    else
        xPosition = bgLayer->xPosition().calcMinValue(pw - scaledImageWidth, true);
    if (backgroundRepeat == RepeatFill || backgroundRepeat == RepeatXFill) {
        cw = pw + left + right;
        sx = scaledImageWidth ? scaledImageWidth - (xPosition + left) % scaledImageWidth : 0;
    } else {
        cx += max(xPosition + left, 0);
        sx = -min(xPosition + left, 0);
        cw = scaledImageWidth + min(xPosition + left, 0);
    }
    
    int yPosition;
    if (isRoot() && bgLayer->attachment())
        yPosition = bgLayer->yPosition().calcMinValue(rh - scaledImageHeight, true);
    else 
        yPosition = bgLayer->yPosition().calcMinValue(ph - scaledImageHeight, true);
    if (backgroundRepeat == RepeatFill || backgroundRepeat == RepeatYFill) {
        ch = ph + top + bottom;
        sy = scaledImageHeight ? scaledImageHeight - (yPosition + top) % scaledImageHeight : 0;
    } else {
        cy += max(yPosition + top, 0);
        sy = -min(yPosition + top, 0);
        ch = scaledImageHeight + min(yPosition + top, 0);
    }

    if (!bgLayer->attachment()) {
        sx += max(tx - cx, 0);
        sy += max(ty - cy, 0);
    }

    destRect = IntRect(cx, cy, cw, ch);
    destRect.intersect(IntRect(tx, ty, w, h));
    phase = IntPoint(sx, sy);
    tileSize = IntSize(scaledImageWidth, scaledImageHeight);
}

void RenderBox::paintFillLayerExtended(const PaintInfo& paintInfo, const Color& c, const FillLayer* bgLayer, int clipY, int clipH,
                                       int tx, int ty, int w, int h, InlineFlowBox* box, CompositeOperator op)
{
    GraphicsContext* context = paintInfo.context;
    bool includeLeftEdge = box ? box->includeLeftEdge() : true;
    bool includeRightEdge = box ? box->includeRightEdge() : true;
    int bLeft = includeLeftEdge ? borderLeft() : 0;
    int bRight = includeRightEdge ? borderRight() : 0;
    int pLeft = includeLeftEdge ? paddingLeft() : 0;
    int pRight = includeRightEdge ? paddingRight() : 0;

    bool clippedToBorderRadius = false;
    if (style()->hasBorderRadius() && (includeLeftEdge || includeRightEdge)) {
        context->save();
        context->addRoundedRectClip(IntRect(tx, ty, w, h),
            includeLeftEdge ? style()->borderTopLeftRadius() : IntSize(),
            includeRightEdge ? style()->borderTopRightRadius() : IntSize(),
            includeLeftEdge ? style()->borderBottomLeftRadius() : IntSize(),
            includeRightEdge ? style()->borderBottomRightRadius() : IntSize());
        clippedToBorderRadius = true;
    }

    if (bgLayer->clip() == PaddingFillBox || bgLayer->clip() == ContentFillBox) {
        // Clip to the padding or content boxes as necessary.
        bool includePadding = bgLayer->clip() == ContentFillBox;
        int x = tx + bLeft + (includePadding ? pLeft : 0);
        int y = ty + borderTop() + (includePadding ? paddingTop() : 0);
        int width = w - bLeft - bRight - (includePadding ? pLeft + pRight : 0);
        int height = h - borderTop() - borderBottom() - (includePadding ? paddingTop() + paddingBottom() : 0);
        context->save();
        context->clip(IntRect(x, y, width, height));
    } else if (bgLayer->clip() == TextFillBox) {
        // We have to draw our text into a mask that can then be used to clip background drawing.
        // First figure out how big the mask has to be.  It should be no bigger than what we need
        // to actually render, so we should intersect the dirty rect with the border box of the background.
        IntRect maskRect(tx, ty, w, h);
        maskRect.intersect(paintInfo.rect);
        
        // Now create the mask.
        auto_ptr<ImageBuffer> maskImage = ImageBuffer::create(maskRect.size(), false);
        if (!maskImage.get())
            return;
        
        GraphicsContext* maskImageContext = maskImage->context();
        maskImageContext->translate(-maskRect.x(), -maskRect.y());
        
        // Now add the text to the clip.  We do this by painting using a special paint phase that signals to
        // InlineTextBoxes that they should just add their contents to the clip.
        PaintInfo info(maskImageContext, maskRect, PaintPhaseTextClip, true, 0, 0);
        if (box)
            box->paint(info, tx - box->xPos(), ty - box->yPos());
        else
            paint(info, tx, ty);
            
        // The mask has been created.  Now we just need to clip to it.
        context->save();
        context->clipToImageBuffer(maskRect, maskImage.get());
    }
    
    StyleImage* bg = bgLayer->image();
    bool shouldPaintBackgroundImage = bg && bg->canRender(style()->effectiveZoom());
    Color bgColor = c;

    // When this style flag is set, change existing background colors and images to a solid white background.
    // If there's no bg color or image, leave it untouched to avoid affecting transparency.
    // We don't try to avoid loading the background images, because this style flag is only set
    // when printing, and at that point we've already loaded the background images anyway. (To avoid
    // loading the background images we'd have to do this check when applying styles rather than
    // while rendering.)
    if (style()->forceBackgroundsToWhite()) {
        // Note that we can't reuse this variable below because the bgColor might be changed
        bool shouldPaintBackgroundColor = !bgLayer->next() && bgColor.isValid() && bgColor.alpha() > 0;
        if (shouldPaintBackgroundImage || shouldPaintBackgroundColor) {
            bgColor = Color::white;
            shouldPaintBackgroundImage = false;
        }
    }

    // Only fill with a base color (e.g., white) if we're the root document, since iframes/frames with
    // no background in the child document should show the parent's background.
    bool isTransparent = false;
    if (!bgLayer->next() && isRoot() && !(bgColor.isValid() && bgColor.alpha() > 0) && view()->frameView()) {
        Node* elt = document()->ownerElement();
        if (elt) {
            if (!elt->hasTagName(frameTag)) {
                // Locate the <body> element using the DOM.  This is easier than trying
                // to crawl around a render tree with potential :before/:after content and
                // anonymous blocks created by inline <body> tags etc.  We can locate the <body>
                // render object very easily via the DOM.
                HTMLElement* body = document()->body();
                isTransparent = !body || !body->hasLocalName(framesetTag); // Can't scroll a frameset document anyway.
            }
        } else
            isTransparent = view()->frameView()->isTransparent();

        // FIXME: This needs to be dynamic.  We should be able to go back to blitting if we ever stop being transparent.
        if (isTransparent)
            view()->frameView()->setUseSlowRepaints(); // The parent must show behind the child.
    }

    // Paint the color first underneath all images.
    if (!bgLayer->next()) {
        IntRect rect(tx, clipY, w, clipH);
        // If we have an alpha and we are painting the root element, go ahead and blend with the base background color.
        if (isRoot() && (!bgColor.isValid() || bgColor.alpha() < 0xFF) && !isTransparent) {
            Color baseColor = view()->frameView()->baseBackgroundColor();
            if (baseColor.alpha() > 0) {
                context->save();
                context->setCompositeOperation(CompositeCopy);
                context->fillRect(rect, baseColor);
                context->restore();
            } else
                context->clearRect(rect);
        }

        if (bgColor.isValid() && bgColor.alpha() > 0)
            context->fillRect(rect, bgColor);
    }

    // no progressive loading of the background image
    if (shouldPaintBackgroundImage) {
        IntRect destRect;
        IntPoint phase;
        IntSize tileSize;

        calculateBackgroundImageGeometry(bgLayer, tx, ty, w, h, destRect, phase, tileSize);
        if (!destRect.isEmpty()) {
            CompositeOperator compositeOp = op == CompositeSourceOver ? bgLayer->composite() : op;
            context->drawTiledImage(bg->image(this, tileSize), destRect, phase, tileSize, compositeOp);
        }
    }

    if (bgLayer->clip() != BorderFillBox)
        // Undo the background clip
        context->restore();

    if (clippedToBorderRadius)
        // Undo the border radius clip
        context->restore();
}

#if PLATFORM(MAC)

void RenderBox::paintCustomHighlight(int tx, int ty, const AtomicString& type, bool behindText)
{
    Frame* frame = document()->frame();
    if (!frame)
        return;
    Page* page = frame->page();
    if (!page)
        return;

    InlineBox* boxWrap = inlineBoxWrapper();
    RootInlineBox* r = boxWrap ? boxWrap->root() : 0;
    if (r) {
        FloatRect rootRect(tx + r->xPos(), ty + r->selectionTop(), r->width(), r->selectionHeight());
        FloatRect imageRect(tx + x(), rootRect.y(), width(), rootRect.height());
        page->chrome()->client()->paintCustomHighlight(node(), type, imageRect, rootRect, behindText, false);
    } else {
        FloatRect imageRect(tx + x(), ty + y(), width(), height());
        page->chrome()->client()->paintCustomHighlight(node(), type, imageRect, imageRect, behindText, false);
    }
}

#endif

IntRect RenderBox::getOverflowClipRect(int tx, int ty)
{
    // FIXME: When overflow-clip (CSS3) is implemented, we'll obtain the property
    // here.

    int bLeft = borderLeft();
    int bTop = borderTop();

    int clipX = tx + bLeft;
    int clipY = ty + bTop;
    int clipWidth = width() - bLeft - borderRight();
    int clipHeight = height() - bTop - borderBottom();

    // Subtract out scrollbars if we have them.
    if (m_layer) {
        clipWidth -= m_layer->verticalScrollbarWidth();
        clipHeight -= m_layer->horizontalScrollbarHeight();
    }

    return IntRect(clipX, clipY, clipWidth, clipHeight);
}

IntRect RenderBox::getClipRect(int tx, int ty)
{
    int clipX = tx;
    int clipY = ty;
    int clipWidth = width();
    int clipHeight = height();

    if (!style()->clipLeft().isAuto()) {
        int c = style()->clipLeft().calcValue(width());
        clipX += c;
        clipWidth -= c;
    }

    if (!style()->clipRight().isAuto())
        clipWidth -= width() - style()->clipRight().calcValue(width());

    if (!style()->clipTop().isAuto()) {
        int c = style()->clipTop().calcValue(height());
        clipY += c;
        clipHeight -= c;
    }

    if (!style()->clipBottom().isAuto())
        clipHeight -= height() - style()->clipBottom().calcValue(height());

    return IntRect(clipX, clipY, clipWidth, clipHeight);
}

int RenderBox::containingBlockWidth() const
{
    RenderBlock* cb = containingBlock();
    if (!cb)
        return 0;
    if (shrinkToAvoidFloats())
        return cb->lineWidth(y());
    return cb->availableWidth();
}

IntSize RenderBox::offsetForPositionedInContainer(RenderObject* container) const
{
    if (!container->isRelPositioned() || !container->isRenderInline())
        return IntSize();

    // When we have an enclosing relpositioned inline, we need to add in the offset of the first line
    // box from the rest of the content, but only in the cases where we know we're positioned
    // relative to the inline itself.

    IntSize offset;
    RenderFlow* flow = static_cast<RenderFlow*>(container);
    int sx;
    int sy;
    if (flow->firstLineBox()) {
        sx = flow->firstLineBox()->xPos();
        sy = flow->firstLineBox()->yPos();
    } else {
        sx = flow->staticX();
        sy = flow->staticY();
    }

    if (!hasStaticX())
        offset.setWidth(sx);
    // This is not terribly intuitive, but we have to match other browsers.  Despite being a block display type inside
    // an inline, we still keep our x locked to the left of the relative positioned inline.  Arguably the correct
    // behavior would be to go flush left to the block that contains the inline, but that isn't what other browsers
    // do.
    else if (!style()->isOriginalDisplayInlineType())
        // Avoid adding in the left border/padding of the containing block twice.  Subtract it out.
        offset.setWidth(sx - (containingBlock()->borderLeft() + containingBlock()->paddingLeft()));

    if (!hasStaticY())
        offset.setHeight(sy);

    return offset;
}

FloatPoint RenderBox::localToAbsolute(FloatPoint localPoint, bool fixed, bool useTransforms) const
{
    if (RenderView* v = view()) {
        if (v->layoutStateEnabled()) {
            LayoutState* layoutState = v->layoutState();
            IntSize offset = layoutState->m_offset;
            offset.expand(x(), y());
            localPoint += offset;
            if (style()->position() == RelativePosition && m_layer)
                localPoint += m_layer->relativePositionOffset();
            return localPoint;
        }
    }

    if (style()->position() == FixedPosition)
        fixed = true;

    RenderObject* o = container();
    if (o) {
        if (useTransforms && m_layer && m_layer->transform()) {
            fixed = false;  // Elements with transforms act as a containing block for fixed position descendants
            localPoint = m_layer->transform()->mapPoint(localPoint);
        }

        localPoint += offsetFromContainer(o);

        return o->localToAbsolute(localPoint, fixed, useTransforms);
    }
    
    return FloatPoint();
}

FloatPoint RenderBox::absoluteToLocal(FloatPoint containerPoint, bool fixed, bool useTransforms) const
{
    // We don't expect absoluteToLocal() to be called during layout (yet)
    ASSERT(!view() || !view()->layoutStateEnabled());
    
    if (style()->position() == FixedPosition)
        fixed = true;

    if (useTransforms && m_layer && m_layer->transform())
        fixed = false;
    
    RenderObject* o = container();
    if (o) {
        FloatPoint localPoint = o->absoluteToLocal(containerPoint, fixed, useTransforms);
        localPoint -= offsetFromContainer(o);
        if (useTransforms && m_layer && m_layer->transform())
            localPoint = m_layer->transform()->inverse().mapPoint(localPoint);
        return localPoint;
    }
    
    return FloatPoint();
}

FloatQuad RenderBox::localToContainerQuad(const FloatQuad& localQuad, RenderBox* repaintContainer, bool fixed) const
{
    if (repaintContainer == this)
        return localQuad;

    if (style()->position() == FixedPosition)
        fixed = true;

    RenderObject* o = container();
    if (o) {
        FloatQuad quad = localQuad;
        if (m_layer && m_layer->transform()) {
            fixed = false;  // Elements with transforms act as a containing block for fixed position descendants
            quad = m_layer->transform()->mapQuad(quad);
        }
        quad += offsetFromContainer(o);
        return o->localToContainerQuad(quad, repaintContainer, fixed);
    }
    
    return FloatQuad();
}

IntSize RenderBox::offsetFromContainer(RenderObject* o) const
{
    ASSERT(o == container());

    IntSize offset;    
    if (isRelPositioned())
        offset += relativePositionOffset();

    if (!isInline() || isReplaced()) {
        RenderBlock* cb;
        if (o->isBlockFlow() && style()->position() != AbsolutePosition && style()->position() != FixedPosition
                && (cb = static_cast<RenderBlock*>(o))->hasColumns()) {
            IntRect rect(x(), y(), 1, 1);
            cb->adjustRectForColumns(rect);
            offset.expand(rect.x(), rect.y());
        } else
            offset.expand(x(), y());
    }

    if (o->hasOverflowClip())
        offset -= toRenderBox(o)->layer()->scrolledContentOffset();

    if (style()->position() == AbsolutePosition)
        offset += offsetForPositionedInContainer(o);

    return offset;
}

void RenderBox::dirtyLineBoxes(bool fullLayout, bool /*isRootLineBox*/)
{
    if (m_inlineBoxWrapper) {
        if (fullLayout) {
            m_inlineBoxWrapper->destroy(renderArena());
            m_inlineBoxWrapper = 0;
        } else
            m_inlineBoxWrapper->dirtyLineBoxes();
    }
}

void RenderBox::position(InlineBox* box)
{
    if (isPositioned()) {
        // Cache the x position only if we were an INLINE type originally.
        bool wasInline = style()->isOriginalDisplayInlineType();
        if (wasInline && hasStaticX()) {
            // The value is cached in the xPos of the box.  We only need this value if
            // our object was inline originally, since otherwise it would have ended up underneath
            // the inlines.
            setStaticX(box->xPos());
            setChildNeedsLayout(true, false); // Just go ahead and mark the positioned object as needing layout, so it will update its position properly.
        } else if (!wasInline && hasStaticY()) {
            // Our object was a block originally, so we make our normal flow position be
            // just below the line box (as though all the inlines that came before us got
            // wrapped in an anonymous block, which is what would have happened had we been
            // in flow).  This value was cached in the yPos() of the box.
            setStaticY(box->yPos());
            setChildNeedsLayout(true, false); // Just go ahead and mark the positioned object as needing layout, so it will update its position properly.
        }

        // Nuke the box.
        box->remove();
        box->destroy(renderArena());
    } else if (isReplaced()) {
        setLocation(box->xPos(), box->yPos());
        m_inlineBoxWrapper = box;
    }
}

void RenderBox::deleteLineBoxWrapper()
{
    if (m_inlineBoxWrapper) {
        if (!documentBeingDestroyed())
            m_inlineBoxWrapper->remove();
        m_inlineBoxWrapper->destroy(renderArena());
        m_inlineBoxWrapper = 0;
    }
}

IntRect RenderBox::clippedOverflowRectForRepaint(RenderBox* repaintContainer)
{
    if (style()->visibility() != VISIBLE && !enclosingLayer()->hasVisibleContent())
        return IntRect();

    IntRect r = overflowRect(false);

    RenderView* v = view();
    if (v) {
        // FIXME: layoutDelta needs to be applied in parts before/after transforms and
        // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
        r.move(v->layoutDelta());
    }
    
    if (style()) {
        if (style()->hasAppearance())
            // The theme may wish to inflate the rect used when repainting.
            theme()->adjustRepaintRect(this, r);

        // We have to use maximalOutlineSize() because a child might have an outline
        // that projects outside of our overflowRect.
        if (v) {
            ASSERT(style()->outlineSize() <= v->maximalOutlineSize());
            r.inflate(v->maximalOutlineSize());
        }
    }
    computeRectForRepaint(repaintContainer, r);
    return r;
}

void RenderBox::computeRectForRepaint(RenderBox* repaintContainer, IntRect& rect, bool fixed)
{
    if (RenderView* v = view()) {
        // LayoutState is only valid for root-relative repainting
        if (v->layoutStateEnabled() && !repaintContainer) {
            LayoutState* layoutState = v->layoutState();
            if (style()->position() == RelativePosition && m_layer)
                rect.move(m_layer->relativePositionOffset());

            rect.move(x(), y());
            rect.move(layoutState->m_offset);
            if (layoutState->m_clipped)
                rect.intersect(layoutState->m_clipRect);
            return;
        }
    }

    if (hasReflection())
        rect.unite(reflectedRect(rect));

    if (repaintContainer == this)
        return;

    RenderObject* o = container();
    if (!o)
        return;

    IntPoint topLeft = rect.location();
    topLeft.move(x(), y());

    if (style()->position() == FixedPosition)
        fixed = true;

    if (o->isBlockFlow() && style()->position() != AbsolutePosition && style()->position() != FixedPosition) {
        RenderBlock* cb = static_cast<RenderBlock*>(o);
        if (cb->hasColumns()) {
            IntRect repaintRect(topLeft, rect.size());
            cb->adjustRectForColumns(repaintRect);
            topLeft = repaintRect.location();
            rect = repaintRect;
        }
    }

    // We are now in our parent container's coordinate space.  Apply our transform to obtain a bounding box
    // in the parent's coordinate space that encloses us.
    if (m_layer && m_layer->transform()) {
        fixed = false;
        rect = m_layer->transform()->mapRect(rect);
        // FIXME: this clobbers topLeft adjustment done for multicol above
        topLeft = rect.location();
        topLeft.move(x(), y());
    }

    if (style()->position() == AbsolutePosition)
        topLeft += offsetForPositionedInContainer(o);
    else if (style()->position() == RelativePosition && m_layer) {
        // Apply the relative position offset when invalidating a rectangle.  The layer
        // is translated, but the render box isn't, so we need to do this to get the
        // right dirty rect.  Since this is called from RenderObject::setStyle, the relative position
        // flag on the RenderObject has been cleared, so use the one on the style().
        topLeft += m_layer->relativePositionOffset();
    }
    
    // FIXME: We ignore the lightweight clipping rect that controls use, since if |o| is in mid-layout,
    // its controlClipRect will be wrong. For overflow clip we use the values cached by the layer.
    if (o->hasOverflowClip()) {
        RenderBox* containerBox = toRenderBox(o);

        // o->height() is inaccurate if we're in the middle of a layout of |o|, so use the
        // layer's size instead.  Even if the layer's size is wrong, the layer itself will repaint
        // anyway if its size does change.
        topLeft -= containerBox->layer()->scrolledContentOffset(); // For overflow:auto/scroll/hidden.

        IntRect repaintRect(topLeft, rect.size());
        IntRect boxRect(0, 0, containerBox->layer()->width(), containerBox->layer()->height());
        rect = intersection(repaintRect, boxRect);
        if (rect.isEmpty())
            return;
    } else
        rect.setLocation(topLeft);
    
    o->computeRectForRepaint(repaintContainer, rect, fixed);
}

void RenderBox::repaintDuringLayoutIfMoved(const IntRect& rect)
{
    int newX = x();
    int newY = y();
    int newWidth = width();
    int newHeight = height();
    if (rect.x() != newX || rect.y() != newY) {
        // The child moved.  Invalidate the object's old and new positions.  We have to do this
        // since the object may not have gotten a layout.
        m_frameRect = rect;
        repaint();
        repaintOverhangingFloats(true);
        m_frameRect = IntRect(newX, newY, newWidth, newHeight);
        repaint();
        repaintOverhangingFloats(true);
    }
}

int RenderBox::relativePositionOffsetX() const
{
    if (!style()->left().isAuto()) {
        if (!style()->right().isAuto() && containingBlock()->style()->direction() == RTL)
            return -style()->right().calcValue(containingBlockWidth());
        return style()->left().calcValue(containingBlockWidth());
    }
    if (!style()->right().isAuto())
        return -style()->right().calcValue(containingBlockWidth());
    return 0;
}

int RenderBox::relativePositionOffsetY() const
{
    if (!style()->top().isAuto()) {
        if (!style()->top().isPercent() || containingBlock()->style()->height().isFixed())
            return style()->top().calcValue(containingBlockHeight());
    } else if (!style()->bottom().isAuto()) {
        if (!style()->bottom().isPercent() || containingBlock()->style()->height().isFixed())
            return -style()->bottom().calcValue(containingBlockHeight());
    }
    return 0;
}

void RenderBox::calcWidth()
{
    if (isPositioned()) {
        calcAbsoluteHorizontal();
        return;
    }

    // If layout is limited to a subtree, the subtree root's width does not change.
    if (node() && view()->frameView() && view()->frameView()->layoutRoot(true) == this)
        return;

    // The parent box is flexing us, so it has increased or decreased our
    // width.  Use the width from the style context.
    if (hasOverrideSize() &&  parent()->style()->boxOrient() == HORIZONTAL
            && parent()->isFlexibleBox() && parent()->isFlexingChildren()) {
        setWidth(overrideSize());
        return;
    }

    bool inVerticalBox = parent()->isFlexibleBox() && (parent()->style()->boxOrient() == VERTICAL);
    bool stretching = (parent()->style()->boxAlign() == BSTRETCH);
    bool treatAsReplaced = shouldCalculateSizeAsReplaced() && (!inVerticalBox || !stretching);

    Length w = (treatAsReplaced) ? Length(calcReplacedWidth(), Fixed) : style()->width();

    RenderBlock* cb = containingBlock();
    int containerWidth = max(0, containingBlockWidth());

    Length marginLeft = style()->marginLeft();
    Length marginRight = style()->marginRight();

    if (isInline() && !isInlineBlockOrInlineTable()) {
        // just calculate margins
        m_marginLeft = marginLeft.calcMinValue(containerWidth);
        m_marginRight = marginRight.calcMinValue(containerWidth);
        if (treatAsReplaced)
            setWidth(max(w.value() + borderLeft() + borderRight() + paddingLeft() + paddingRight(), minPrefWidth()));

        return;
    }

    // Width calculations
    if (treatAsReplaced)
        setWidth(w.value() + borderLeft() + borderRight() + paddingLeft() + paddingRight());
    else {
        // Calculate Width
        setWidth(calcWidthUsing(Width, containerWidth));

        // Calculate MaxWidth
        if (!style()->maxWidth().isUndefined()) {
            int maxW = calcWidthUsing(MaxWidth, containerWidth);
            if (width() > maxW) {
                setWidth(maxW);
                w = style()->maxWidth();
            }
        }

        // Calculate MinWidth
        int minW = calcWidthUsing(MinWidth, containerWidth);
        if (width() < minW) {
            setWidth(minW);
            w = style()->minWidth();
        }
    }

    if (stretchesToMinIntrinsicWidth()) {
        setWidth(max(width(), minPrefWidth()));
        w = Length(width(), Fixed);
    }

    // Margin calculations
    if (w.isAuto()) {
        m_marginLeft = marginLeft.calcMinValue(containerWidth);
        m_marginRight = marginRight.calcMinValue(containerWidth);
    } else {
        m_marginLeft = 0;
        m_marginRight = 0;
        calcHorizontalMargins(marginLeft, marginRight, containerWidth);
    }

    if (containerWidth && containerWidth != (width() + m_marginLeft + m_marginRight)
            && !isFloating() && !isInline() && !cb->isFlexibleBox()) {
        if (cb->style()->direction() == LTR)
            m_marginRight = containerWidth - width() - m_marginLeft;
        else
            m_marginLeft = containerWidth - width() - m_marginRight;
    }
}

int RenderBox::calcWidthUsing(WidthType widthType, int cw)
{
    int widthResult = width();
    Length w;
    if (widthType == Width)
        w = style()->width();
    else if (widthType == MinWidth)
        w = style()->minWidth();
    else
        w = style()->maxWidth();

    if (w.isIntrinsicOrAuto()) {
        int marginLeft = style()->marginLeft().calcMinValue(cw);
        int marginRight = style()->marginRight().calcMinValue(cw);
        if (cw)
            widthResult = cw - marginLeft - marginRight;

        if (sizesToIntrinsicWidth(widthType)) {
            widthResult = max(widthResult, minPrefWidth());
            widthResult = min(widthResult, maxPrefWidth());
        }
    } else
        widthResult = calcBorderBoxWidth(w.calcValue(cw));

    return widthResult;
}

bool RenderBox::sizesToIntrinsicWidth(WidthType widthType) const
{
    // Marquees in WinIE are like a mixture of blocks and inline-blocks.  They size as though they're blocks,
    // but they allow text to sit on the same line as the marquee.
    if (isFloating() || (isInlineBlockOrInlineTable() && !isHTMLMarquee()))
        return true;

    // This code may look a bit strange.  Basically width:intrinsic should clamp the size when testing both
    // min-width and width.  max-width is only clamped if it is also intrinsic.
    Length width = (widthType == MaxWidth) ? style()->maxWidth() : style()->width();
    if (width.type() == Intrinsic)
        return true;

    // Children of a horizontal marquee do not fill the container by default.
    // FIXME: Need to deal with MAUTO value properly.  It could be vertical.
    if (parent()->style()->overflowX() == OMARQUEE) {
        EMarqueeDirection dir = parent()->style()->marqueeDirection();
        if (dir == MAUTO || dir == MFORWARD || dir == MBACKWARD || dir == MLEFT || dir == MRIGHT)
            return true;
    }

    // Flexible horizontal boxes lay out children at their intrinsic widths.  Also vertical boxes
    // that don't stretch their kids lay out their children at their intrinsic widths.
    if (parent()->isFlexibleBox()
            && (parent()->style()->boxOrient() == HORIZONTAL || parent()->style()->boxAlign() != BSTRETCH))
        return true;

    return false;
}

void RenderBox::calcHorizontalMargins(const Length& marginLeft, const Length& marginRight, int containerWidth)
{
    if (isFloating() || isInline()) {
        // Inline blocks/tables and floats don't have their margins increased.
        m_marginLeft = marginLeft.calcMinValue(containerWidth);
        m_marginRight = marginRight.calcMinValue(containerWidth);
        return;
    }

    if ((marginLeft.isAuto() && marginRight.isAuto() && width() < containerWidth)
            || (!marginLeft.isAuto() && !marginRight.isAuto() && containingBlock()->style()->textAlign() == WEBKIT_CENTER)) {
        m_marginLeft = max(0, (containerWidth - width()) / 2);
        m_marginRight = containerWidth - width() - m_marginLeft;
    } else if ((marginRight.isAuto() && width() < containerWidth)
            || (!marginLeft.isAuto() && containingBlock()->style()->direction() == RTL && containingBlock()->style()->textAlign() == WEBKIT_LEFT)) {
        m_marginLeft = marginLeft.calcValue(containerWidth);
        m_marginRight = containerWidth - width() - m_marginLeft;
    } else if ((marginLeft.isAuto() && width() < containerWidth)
            || (!marginRight.isAuto() && containingBlock()->style()->direction() == LTR && containingBlock()->style()->textAlign() == WEBKIT_RIGHT)) {
        m_marginRight = marginRight.calcValue(containerWidth);
        m_marginLeft = containerWidth - width() - m_marginRight;
    } else {
        // This makes auto margins 0 if we failed a width() < containerWidth test above (css2.1, 10.3.3).
        m_marginLeft = marginLeft.calcMinValue(containerWidth);
        m_marginRight = marginRight.calcMinValue(containerWidth);
    }
}

void RenderBox::calcHeight()
{
    // Cell height is managed by the table and inline non-replaced elements do not support a height property.
    if (isTableCell() || (isInline() && !isReplaced()))
        return;

    if (isPositioned())
        calcAbsoluteVertical();
    else {
        calcVerticalMargins();

        // For tables, calculate margins only.
        if (isTable())
            return;

        Length h;
        bool inHorizontalBox = parent()->isFlexibleBox() && parent()->style()->boxOrient() == HORIZONTAL;
        bool stretching = parent()->style()->boxAlign() == BSTRETCH;
        bool treatAsReplaced = shouldCalculateSizeAsReplaced() && (!inHorizontalBox || !stretching);
        bool checkMinMaxHeight = false;

        // The parent box is flexing us, so it has increased or decreased our height.  We have to
        // grab our cached flexible height.
        if (hasOverrideSize() && parent()->isFlexibleBox() && parent()->style()->boxOrient() == VERTICAL
                && parent()->isFlexingChildren())
            h = Length(overrideSize() - borderTop() - borderBottom() - paddingTop() - paddingBottom(), Fixed);
        else if (treatAsReplaced)
            h = Length(calcReplacedHeight(), Fixed);
        else {
            h = style()->height();
            checkMinMaxHeight = true;
        }

        // Block children of horizontal flexible boxes fill the height of the box.
        if (h.isAuto() && parent()->isFlexibleBox() && parent()->style()->boxOrient() == HORIZONTAL
                && parent()->isStretchingChildren()) {
            h = Length(parentBox()->contentHeight() - marginTop() - marginBottom() -
                       borderTop() - paddingTop() - borderBottom() - paddingBottom(), Fixed);
            checkMinMaxHeight = false;
        }

        int heightResult;
        if (checkMinMaxHeight) {
            heightResult = calcHeightUsing(style()->height());
            if (heightResult == -1)
                heightResult = height();
            int minH = calcHeightUsing(style()->minHeight()); // Leave as -1 if unset.
            int maxH = style()->maxHeight().isUndefined() ? heightResult : calcHeightUsing(style()->maxHeight());
            if (maxH == -1)
                maxH = heightResult;
            heightResult = min(maxH, heightResult);
            heightResult = max(minH, heightResult);
        } else
            // The only times we don't check min/max height are when a fixed length has
            // been given as an override.  Just use that.  The value has already been adjusted
            // for box-sizing.
            heightResult = h.value() + borderTop() + borderBottom() + paddingTop() + paddingBottom();

            setHeight(heightResult);
        }

    // WinIE quirk: The <html> block always fills the entire canvas in quirks mode.  The <body> always fills the
    // <html> block in quirks mode.  Only apply this quirk if the block is normal flow and no height
    // is specified.
    if (stretchesToViewHeight() && !document()->printing()) {
        int margins = collapsedMarginTop() + collapsedMarginBottom();
        int visHeight = view()->viewHeight();
        if (isRoot())
            setHeight(max(height(), visHeight - margins));
        else {
            int marginsBordersPadding = margins + parentBox()->marginTop() + parentBox()->marginBottom()
                + parentBox()->borderTop() + parentBox()->borderBottom()
                + parentBox()->paddingTop() + parentBox()->paddingBottom();
            setHeight(max(height(), visHeight - marginsBordersPadding));
        }
    }
}

int RenderBox::calcHeightUsing(const Length& h)
{
    int height = -1;
    if (!h.isAuto()) {
        if (h.isFixed())
            height = h.value();
        else if (h.isPercent())
            height = calcPercentageHeight(h);
        if (height != -1) {
            height = calcBorderBoxHeight(height);
            return height;
        }
    }
    return height;
}

int RenderBox::calcPercentageHeight(const Length& height)
{
    int result = -1;
    bool includeBorderPadding = isTable();
    RenderBlock* cb = containingBlock();
    if (style()->htmlHacks()) {
        // In quirks mode, blocks with auto height are skipped, and we keep looking for an enclosing
        // block that may have a specified height and then use it.  In strict mode, this violates the
        // specification, which states that percentage heights just revert to auto if the containing
        // block has an auto height.
        while (!cb->isRenderView() && !cb->isBody() && !cb->isTableCell() && !cb->isPositioned() && cb->style()->height().isAuto()) {
            cb = cb->containingBlock();
            cb->addPercentHeightDescendant(this);
        }
    }

    // A positioned element that specified both top/bottom or that specifies height should be treated as though it has a height
    // explicitly specified that can be used for any percentage computations.
    bool isPositionedWithSpecifiedHeight = cb->isPositioned() && (!cb->style()->height().isAuto() || (!cb->style()->top().isAuto() && !cb->style()->bottom().isAuto()));

    // Table cells violate what the CSS spec says to do with heights.  Basically we
    // don't care if the cell specified a height or not.  We just always make ourselves
    // be a percentage of the cell's current content height.
    if (cb->isTableCell()) {
        result = cb->overrideSize();
        if (result == -1) {
            // Normally we would let the cell size intrinsically, but scrolling overflow has to be
            // treated differently, since WinIE lets scrolled overflow regions shrink as needed.
            // While we can't get all cases right, we can at least detect when the cell has a specified
            // height or when the table has a specified height.  In these cases we want to initially have
            // no size and allow the flexing of the table or the cell to its specified height to cause us
            // to grow to fill the space.  This could end up being wrong in some cases, but it is
            // preferable to the alternative (sizing intrinsically and making the row end up too big).
            RenderTableCell* cell = static_cast<RenderTableCell*>(cb);
            if (scrollsOverflowY() && (!cell->style()->height().isAuto() || !cell->table()->style()->height().isAuto()))
                return 0;
            return -1;
        }
        includeBorderPadding = true;
    }
    // Otherwise we only use our percentage height if our containing block had a specified
    // height.
    else if (cb->style()->height().isFixed())
        result = cb->calcContentBoxHeight(cb->style()->height().value());
    else if (cb->style()->height().isPercent() && !isPositionedWithSpecifiedHeight) {
        // We need to recur and compute the percentage height for our containing block.
        result = cb->calcPercentageHeight(cb->style()->height());
        if (result != -1)
            result = cb->calcContentBoxHeight(result);
    } else if (cb->isRenderView() || (cb->isBody() && style()->htmlHacks()) || isPositionedWithSpecifiedHeight) {
        // Don't allow this to affect the block' height() member variable, since this
        // can get called while the block is still laying out its kids.
        int oldHeight = cb->height();
        cb->calcHeight();
        result = cb->contentHeight();
        cb->setHeight(oldHeight);
    } else if (cb->isRoot() && isPositioned())
        // Match the positioned objects behavior, which is that positioned objects will fill their viewport
        // always.  Note we could only hit this case by recurring into calcPercentageHeight on a positioned containing block.
        result = cb->calcContentBoxHeight(cb->availableHeight());

    if (result != -1) {
        result = height.calcValue(result);
        if (includeBorderPadding) {
            // It is necessary to use the border-box to match WinIE's broken
            // box model.  This is essential for sizing inside
            // table cells using percentage heights.
            result -= (borderTop() + paddingTop() + borderBottom() + paddingBottom());
            result = max(0, result);
        }
    }
    return result;
}

int RenderBox::calcReplacedWidth(bool includeMaxWidth) const
{
    int width = calcReplacedWidthUsing(style()->width());
    int minW = calcReplacedWidthUsing(style()->minWidth());
    int maxW = !includeMaxWidth || style()->maxWidth().isUndefined() ? width : calcReplacedWidthUsing(style()->maxWidth());

    return max(minW, min(width, maxW));
}

int RenderBox::calcReplacedWidthUsing(Length width) const
{
    switch (width.type()) {
        case Fixed:
            return calcContentBoxWidth(width.value());
        case Percent: {
            const int cw = isPositioned() ? containingBlockWidthForPositioned(container()) : containingBlockWidth();
            if (cw > 0)
                return calcContentBoxWidth(width.calcMinValue(cw));
        }
        // fall through
        default:
            return intrinsicSize().width();
     }
 }

int RenderBox::calcReplacedHeight() const
{
    int height = calcReplacedHeightUsing(style()->height());
    int minH = calcReplacedHeightUsing(style()->minHeight());
    int maxH = style()->maxHeight().isUndefined() ? height : calcReplacedHeightUsing(style()->maxHeight());

    return max(minH, min(height, maxH));
}

int RenderBox::calcReplacedHeightUsing(Length height) const
{
    switch (height.type()) {
        case Fixed:
            return calcContentBoxHeight(height.value());
        case Percent:
        {
            RenderObject* cb = isPositioned() ? container() : containingBlock();
            while (cb->isAnonymous()) {
                cb = cb->containingBlock();
                static_cast<RenderBlock*>(cb)->addPercentHeightDescendant(const_cast<RenderBox*>(this));
            }

            if (cb->isPositioned() && cb->style()->height().isAuto() && !(cb->style()->top().isAuto() || cb->style()->bottom().isAuto())) {
                ASSERT(cb->isRenderBlock());
                RenderBlock* block = static_cast<RenderBlock*>(cb);
                int oldHeight = block->height();
                block->calcHeight();
                int newHeight = block->calcContentBoxHeight(block->contentHeight());
                block->setHeight(oldHeight);
                return calcContentBoxHeight(height.calcValue(newHeight));
            }
            
            int availableHeight = isPositioned() ? containingBlockHeightForPositioned(cb) : toRenderBox(cb)->availableHeight();

            // It is necessary to use the border-box to match WinIE's broken
            // box model.  This is essential for sizing inside
            // table cells using percentage heights.
            if (cb->isTableCell() && (cb->style()->height().isAuto() || cb->style()->height().isPercent())) {
                // Don't let table cells squeeze percent-height replaced elements
                // <http://bugs.webkit.org/show_bug.cgi?id=15359>
                availableHeight = max(availableHeight, intrinsicSize().height());
                return height.calcValue(availableHeight - (borderTop() + borderBottom()
                    + paddingTop() + paddingBottom()));
            }

            return calcContentBoxHeight(height.calcValue(availableHeight));
        }
        default:
            return intrinsicSize().height();
    }
}

int RenderBox::availableHeight() const
{
    return availableHeightUsing(style()->height());
}

int RenderBox::availableHeightUsing(const Length& h) const
{
    if (h.isFixed())
        return calcContentBoxHeight(h.value());

    if (isRenderView())
        return static_cast<const RenderView*>(this)->frameView()->visibleHeight();

    // We need to stop here, since we don't want to increase the height of the table
    // artificially.  We're going to rely on this cell getting expanded to some new
    // height, and then when we lay out again we'll use the calculation below.
    if (isTableCell() && (h.isAuto() || h.isPercent()))
        return overrideSize() - (borderLeft() + borderRight() + paddingLeft() + paddingRight());

    if (h.isPercent())
       return calcContentBoxHeight(h.calcValue(containingBlock()->availableHeight()));

    if (isRenderBlock() && isPositioned() && style()->height().isAuto() && !(style()->top().isAuto() || style()->bottom().isAuto())) {
        RenderBlock* block = const_cast<RenderBlock*>(static_cast<const RenderBlock*>(this));
        int oldHeight = block->height();
        block->calcHeight();
        int newHeight = block->calcContentBoxHeight(block->contentHeight());
        block->setHeight(oldHeight);
        return calcContentBoxHeight(newHeight);
    }

    return containingBlock()->availableHeight();
}

void RenderBox::calcVerticalMargins()
{
    if (isTableCell()) {
        m_marginTop = 0;
        m_marginBottom = 0;
        return;
    }

    // margins are calculated with respect to the _width_ of
    // the containing block (8.3)
    int cw = containingBlock()->contentWidth();

    m_marginTop = style()->marginTop().calcMinValue(cw);
    m_marginBottom = style()->marginBottom().calcMinValue(cw);
}

int RenderBox::staticX() const
{
    return m_layer ? m_layer->staticX() : 0;
}

int RenderBox::staticY() const
{
    return m_layer ? m_layer->staticY() : 0;
}

void RenderBox::setStaticX(int staticX)
{
    ASSERT(isPositioned() || isRelPositioned());
    m_layer->setStaticX(staticX);
}

void RenderBox::setStaticY(int staticY)
{
    ASSERT(isPositioned() || isRelPositioned());
    
    if (staticY == m_layer->staticY())
        return;
    
    m_layer->setStaticY(staticY);
    setChildNeedsLayout(true, false);
}

int RenderBox::containingBlockWidthForPositioned(const RenderObject* containingBlock) const
{
    if (containingBlock->isRenderInline()) {
        ASSERT(containingBlock->isRelPositioned());

        const RenderFlow* flow = static_cast<const RenderFlow*>(containingBlock);
        InlineFlowBox* first = flow->firstLineBox();
        InlineFlowBox* last = flow->lastLineBox();

        // If the containing block is empty, return a width of 0.
        if (!first || !last)
            return 0;

        int fromLeft;
        int fromRight;
        if (containingBlock->style()->direction() == LTR) {
            fromLeft = first->xPos() + first->borderLeft();
            fromRight = last->xPos() + last->width() - last->borderRight();
        } else {
            fromRight = first->xPos() + first->width() - first->borderRight();
            fromLeft = last->xPos() + last->borderLeft();
        }

        return max(0, (fromRight - fromLeft));
    }

    const RenderBox* containingBlockBox = toRenderBox(containingBlock);
    return containingBlockBox->width() - containingBlockBox->borderLeft() - containingBlockBox->borderRight() - containingBlockBox->verticalScrollbarWidth();
}

int RenderBox::containingBlockHeightForPositioned(const RenderObject* containingBlock) const
{
    const RenderBox* containingBlockBox = toRenderBox(containingBlock);
    
    int heightResult;
    if (containingBlock->isRenderInline()) {
        ASSERT(containingBlock->isRelPositioned());
        heightResult = static_cast<const RenderInline*>(containingBlock)->linesBoundingBox().height();
    } else
        heightResult = containingBlockBox->height();
    
    return heightResult - containingBlockBox->borderTop() - containingBlockBox->borderBottom();
}

void RenderBox::calcAbsoluteHorizontal()
{
    if (isReplaced()) {
        calcAbsoluteHorizontalReplaced();
        return;
    }

    // QUESTIONS
    // FIXME 1: Which RenderObject's 'direction' property should used: the
    // containing block (cb) as the spec seems to imply, the parent (parent()) as
    // was previously done in calculating the static distances, or ourself, which
    // was also previously done for deciding what to override when you had
    // over-constrained margins?  Also note that the container block is used
    // in similar situations in other parts of the RenderBox class (see calcWidth()
    // and calcHorizontalMargins()). For now we are using the parent for quirks
    // mode and the containing block for strict mode.

    // FIXME 2: Should we still deal with these the cases of 'left' or 'right' having
    // the type 'static' in determining whether to calculate the static distance?
    // NOTE: 'static' is not a legal value for 'left' or 'right' as of CSS 2.1.

    // FIXME 3: Can perhaps optimize out cases when max-width/min-width are greater
    // than or less than the computed width().  Be careful of box-sizing and
    // percentage issues.

    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.3.7 "Absolutely positioned, non-replaced elements"
    // <http://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width>
    // (block-style-comments in this function and in calcAbsoluteHorizontalValues()
    // correspond to text from the spec)


    // We don't use containingBlock(), since we may be positioned by an enclosing
    // relative positioned inline.
    const RenderBox* containerBlock = toRenderBox(container());
    
    const int containerWidth = containingBlockWidthForPositioned(containerBlock);

    // To match WinIE, in quirks mode use the parent's 'direction' property
    // instead of the the container block's.
    TextDirection containerDirection = (style()->htmlHacks()) ? parent()->style()->direction() : containerBlock->style()->direction();

    const int bordersPlusPadding = borderLeft() + borderRight() + paddingLeft() + paddingRight();
    const Length marginLeft = style()->marginLeft();
    const Length marginRight = style()->marginRight();
    Length left = style()->left();
    Length right = style()->right();

    /*---------------------------------------------------------------------------*\
     * For the purposes of this section and the next, the term "static position"
     * (of an element) refers, roughly, to the position an element would have had
     * in the normal flow. More precisely:
     *
     * * The static position for 'left' is the distance from the left edge of the
     *   containing block to the left margin edge of a hypothetical box that would
     *   have been the first box of the element if its 'position' property had
     *   been 'static' and 'float' had been 'none'. The value is negative if the
     *   hypothetical box is to the left of the containing block.
     * * The static position for 'right' is the distance from the right edge of the
     *   containing block to the right margin edge of the same hypothetical box as
     *   above. The value is positive if the hypothetical box is to the left of the
     *   containing block's edge.
     *
     * But rather than actually calculating the dimensions of that hypothetical box,
     * user agents are free to make a guess at its probable position.
     *
     * For the purposes of calculating the static position, the containing block of
     * fixed positioned elements is the initial containing block instead of the
     * viewport, and all scrollable boxes should be assumed to be scrolled to their
     * origin.
    \*---------------------------------------------------------------------------*/

    // see FIXME 2
    // Calculate the static distance if needed.
    if (left.isAuto() && right.isAuto()) {
        if (containerDirection == LTR) {
            // 'staticX' should already have been set through layout of the parent.
            int staticPosition = staticX() - containerBlock->borderLeft();
            for (RenderBox* po = parentBox(); po && po != containerBlock; po = po->parentBox())
                staticPosition += po->x();
            left.setValue(Fixed, staticPosition);
        } else {
            RenderBox* po = parentBox();
            // 'staticX' should already have been set through layout of the parent.
            int staticPosition = staticX() + containerWidth + containerBlock->borderRight() - po->width();
            for (; po && po != containerBlock; po = po->parentBox())
                staticPosition -= po->x();
            right.setValue(Fixed, staticPosition);
        }
    }

    // Calculate constraint equation values for 'width' case.
    int widthResult;
    int xResult;
    calcAbsoluteHorizontalValues(style()->width(), containerBlock, containerDirection,
                                 containerWidth, bordersPlusPadding,
                                 left, right, marginLeft, marginRight,
                                 widthResult, m_marginLeft, m_marginRight, xResult);
    setWidth(widthResult);
    setX(xResult);

    // Calculate constraint equation values for 'max-width' case.
    if (!style()->maxWidth().isUndefined()) {
        int maxWidth;
        int maxMarginLeft;
        int maxMarginRight;
        int maxXPos;

        calcAbsoluteHorizontalValues(style()->maxWidth(), containerBlock, containerDirection,
                                     containerWidth, bordersPlusPadding,
                                     left, right, marginLeft, marginRight,
                                     maxWidth, maxMarginLeft, maxMarginRight, maxXPos);

        if (width() > maxWidth) {
            setWidth(maxWidth);
            m_marginLeft = maxMarginLeft;
            m_marginRight = maxMarginRight;
            m_frameRect.setX(maxXPos);
        }
    }

    // Calculate constraint equation values for 'min-width' case.
    if (!style()->minWidth().isZero()) {
        int minWidth;
        int minMarginLeft;
        int minMarginRight;
        int minXPos;

        calcAbsoluteHorizontalValues(style()->minWidth(), containerBlock, containerDirection,
                                     containerWidth, bordersPlusPadding,
                                     left, right, marginLeft, marginRight,
                                     minWidth, minMarginLeft, minMarginRight, minXPos);

        if (width() < minWidth) {
            setWidth(minWidth);
            m_marginLeft = minMarginLeft;
            m_marginRight = minMarginRight;
            m_frameRect.setX(minXPos);
        }
    }

    if (stretchesToMinIntrinsicWidth() && width() < minPrefWidth() - bordersPlusPadding) {
        calcAbsoluteHorizontalValues(Length(minPrefWidth() - bordersPlusPadding, Fixed), containerBlock, containerDirection,
                                     containerWidth, bordersPlusPadding,
                                     left, right, marginLeft, marginRight,
                                     widthResult, m_marginLeft, m_marginRight, xResult);
        setWidth(widthResult);
        setX(xResult);
    }

    // Put width() into correct form.
    setWidth(width() + bordersPlusPadding);
}

void RenderBox::calcAbsoluteHorizontalValues(Length width, const RenderBox* containerBlock, TextDirection containerDirection,
                                             const int containerWidth, const int bordersPlusPadding,
                                             const Length left, const Length right, const Length marginLeft, const Length marginRight,
                                             int& widthValue, int& marginLeftValue, int& marginRightValue, int& xPos)
{
    // 'left' and 'right' cannot both be 'auto' because one would of been
    // converted to the static postion already
    ASSERT(!(left.isAuto() && right.isAuto()));

    int leftValue = 0;

    bool widthIsAuto = width.isIntrinsicOrAuto();
    bool leftIsAuto = left.isAuto();
    bool rightIsAuto = right.isAuto();

    if (!leftIsAuto && !widthIsAuto && !rightIsAuto) {
        /*-----------------------------------------------------------------------*\
         * If none of the three is 'auto': If both 'margin-left' and 'margin-
         * right' are 'auto', solve the equation under the extra constraint that
         * the two margins get equal values, unless this would make them negative,
         * in which case when direction of the containing block is 'ltr' ('rtl'),
         * set 'margin-left' ('margin-right') to zero and solve for 'margin-right'
         * ('margin-left'). If one of 'margin-left' or 'margin-right' is 'auto',
         * solve the equation for that value. If the values are over-constrained,
         * ignore the value for 'left' (in case the 'direction' property of the
         * containing block is 'rtl') or 'right' (in case 'direction' is 'ltr')
         * and solve for that value.
        \*-----------------------------------------------------------------------*/
        // NOTE:  It is not necessary to solve for 'right' in the over constrained
        // case because the value is not used for any further calculations.

        leftValue = left.calcValue(containerWidth);
        widthValue = calcContentBoxWidth(width.calcValue(containerWidth));

        const int availableSpace = containerWidth - (leftValue + widthValue + right.calcValue(containerWidth) + bordersPlusPadding);

        // Margins are now the only unknown
        if (marginLeft.isAuto() && marginRight.isAuto()) {
            // Both margins auto, solve for equality
            if (availableSpace >= 0) {
                marginLeftValue = availableSpace / 2; // split the diference
                marginRightValue = availableSpace - marginLeftValue;  // account for odd valued differences
            } else {
                // see FIXME 1
                if (containerDirection == LTR) {
                    marginLeftValue = 0;
                    marginRightValue = availableSpace; // will be negative
                } else {
                    marginLeftValue = availableSpace; // will be negative
                    marginRightValue = 0;
                }
            }
        } else if (marginLeft.isAuto()) {
            // Solve for left margin
            marginRightValue = marginRight.calcValue(containerWidth);
            marginLeftValue = availableSpace - marginRightValue;
        } else if (marginRight.isAuto()) {
            // Solve for right margin
            marginLeftValue = marginLeft.calcValue(containerWidth);
            marginRightValue = availableSpace - marginLeftValue;
        } else {
            // Over-constrained, solve for left if direction is RTL
            marginLeftValue = marginLeft.calcValue(containerWidth);
            marginRightValue = marginRight.calcValue(containerWidth);

            // see FIXME 1 -- used to be "this->style()->direction()"
            if (containerDirection == RTL)
                leftValue = (availableSpace + leftValue) - marginLeftValue - marginRightValue;
        }
    } else {
        /*--------------------------------------------------------------------*\
         * Otherwise, set 'auto' values for 'margin-left' and 'margin-right'
         * to 0, and pick the one of the following six rules that applies.
         *
         * 1. 'left' and 'width' are 'auto' and 'right' is not 'auto', then the
         *    width is shrink-to-fit. Then solve for 'left'
         *
         *              OMIT RULE 2 AS IT SHOULD NEVER BE HIT
         * ------------------------------------------------------------------
         * 2. 'left' and 'right' are 'auto' and 'width' is not 'auto', then if
         *    the 'direction' property of the containing block is 'ltr' set
         *    'left' to the static position, otherwise set 'right' to the
         *    static position. Then solve for 'left' (if 'direction is 'rtl')
         *    or 'right' (if 'direction' is 'ltr').
         * ------------------------------------------------------------------
         *
         * 3. 'width' and 'right' are 'auto' and 'left' is not 'auto', then the
         *    width is shrink-to-fit . Then solve for 'right'
         * 4. 'left' is 'auto', 'width' and 'right' are not 'auto', then solve
         *    for 'left'
         * 5. 'width' is 'auto', 'left' and 'right' are not 'auto', then solve
         *    for 'width'
         * 6. 'right' is 'auto', 'left' and 'width' are not 'auto', then solve
         *    for 'right'
         *
         * Calculation of the shrink-to-fit width is similar to calculating the
         * width of a table cell using the automatic table layout algorithm.
         * Roughly: calculate the preferred width by formatting the content
         * without breaking lines other than where explicit line breaks occur,
         * and also calculate the preferred minimum width, e.g., by trying all
         * possible line breaks. CSS 2.1 does not define the exact algorithm.
         * Thirdly, calculate the available width: this is found by solving
         * for 'width' after setting 'left' (in case 1) or 'right' (in case 3)
         * to 0.
         *
         * Then the shrink-to-fit width is:
         * min(max(preferred minimum width, available width), preferred width).
        \*--------------------------------------------------------------------*/
        // NOTE: For rules 3 and 6 it is not necessary to solve for 'right'
        // because the value is not used for any further calculations.

        // Calculate margins, 'auto' margins are ignored.
        marginLeftValue = marginLeft.calcMinValue(containerWidth);
        marginRightValue = marginRight.calcMinValue(containerWidth);

        const int availableSpace = containerWidth - (marginLeftValue + marginRightValue + bordersPlusPadding);

        // FIXME: Is there a faster way to find the correct case?
        // Use rule/case that applies.
        if (leftIsAuto && widthIsAuto && !rightIsAuto) {
            // RULE 1: (use shrink-to-fit for width, and solve of left)
            int rightValue = right.calcValue(containerWidth);

            // FIXME: would it be better to have shrink-to-fit in one step?
            int preferredWidth = maxPrefWidth() - bordersPlusPadding;
            int preferredMinWidth = minPrefWidth() - bordersPlusPadding;
            int availableWidth = availableSpace - rightValue;
            widthValue = min(max(preferredMinWidth, availableWidth), preferredWidth);
            leftValue = availableSpace - (widthValue + rightValue);
        } else if (!leftIsAuto && widthIsAuto && rightIsAuto) {
            // RULE 3: (use shrink-to-fit for width, and no need solve of right)
            leftValue = left.calcValue(containerWidth);

            // FIXME: would it be better to have shrink-to-fit in one step?
            int preferredWidth = maxPrefWidth() - bordersPlusPadding;
            int preferredMinWidth = minPrefWidth() - bordersPlusPadding;
            int availableWidth = availableSpace - leftValue;
            widthValue = min(max(preferredMinWidth, availableWidth), preferredWidth);
        } else if (leftIsAuto && !width.isAuto() && !rightIsAuto) {
            // RULE 4: (solve for left)
            widthValue = calcContentBoxWidth(width.calcValue(containerWidth));
            leftValue = availableSpace - (widthValue + right.calcValue(containerWidth));
        } else if (!leftIsAuto && widthIsAuto && !rightIsAuto) {
            // RULE 5: (solve for width)
            leftValue = left.calcValue(containerWidth);
            widthValue = availableSpace - (leftValue + right.calcValue(containerWidth));
        } else if (!leftIsAuto&& !widthIsAuto && rightIsAuto) {
            // RULE 6: (no need solve for right)
            leftValue = left.calcValue(containerWidth);
            widthValue = calcContentBoxWidth(width.calcValue(containerWidth));
        }
    }

    // Use computed values to calculate the horizontal position.

    // FIXME: This hack is needed to calculate the xPos for a 'rtl' relatively
    // positioned, inline containing block because right now, it is using the xPos
    // of the first line box when really it should use the last line box.  When
    // this is fixed elsewhere, this block should be removed.
    if (containerBlock->isInline() && containerBlock->style()->direction() == RTL) {
        const RenderFlow* flow = static_cast<const RenderFlow*>(containerBlock);
        InlineFlowBox* firstLine = flow->firstLineBox();
        InlineFlowBox* lastLine = flow->lastLineBox();
        if (firstLine && lastLine && firstLine != lastLine) {
            xPos = leftValue + marginLeftValue + lastLine->borderLeft() + (lastLine->xPos() - firstLine->xPos());
            return;
        }
    }

    xPos = leftValue + marginLeftValue + containerBlock->borderLeft();
}

void RenderBox::calcAbsoluteVertical()
{
    if (isReplaced()) {
        calcAbsoluteVerticalReplaced();
        return;
    }

    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.6.4 "Absolutely positioned, non-replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-non-replaced-height>
    // (block-style-comments in this function and in calcAbsoluteVerticalValues()
    // correspond to text from the spec)


    // We don't use containingBlock(), since we may be positioned by an enclosing relpositioned inline.
    const RenderBox* containerBlock = toRenderBox(container());

    const int containerHeight = containingBlockHeightForPositioned(containerBlock);

    const int bordersPlusPadding = borderTop() + borderBottom() + paddingTop() + paddingBottom();
    const Length marginTop = style()->marginTop();
    const Length marginBottom = style()->marginBottom();
    Length top = style()->top();
    Length bottom = style()->bottom();

    /*---------------------------------------------------------------------------*\
     * For the purposes of this section and the next, the term "static position"
     * (of an element) refers, roughly, to the position an element would have had
     * in the normal flow. More precisely, the static position for 'top' is the
     * distance from the top edge of the containing block to the top margin edge
     * of a hypothetical box that would have been the first box of the element if
     * its 'position' property had been 'static' and 'float' had been 'none'. The
     * value is negative if the hypothetical box is above the containing block.
     *
     * But rather than actually calculating the dimensions of that hypothetical
     * box, user agents are free to make a guess at its probable position.
     *
     * For the purposes of calculating the static position, the containing block
     * of fixed positioned elements is the initial containing block instead of
     * the viewport.
    \*---------------------------------------------------------------------------*/

    // see FIXME 2
    // Calculate the static distance if needed.
    if (top.isAuto() && bottom.isAuto()) {
        // staticY should already have been set through layout of the parent()
        int staticTop = staticY() - containerBlock->borderTop();
        for (RenderBox* po = parentBox(); po && po != containerBlock; po = po->parentBox()) {
            if (!po->isTableRow())
                staticTop += po->y();
        }
        top.setValue(Fixed, staticTop);
    }


    int h; // Needed to compute overflow.
    int y;

    // Calculate constraint equation values for 'height' case.
    calcAbsoluteVerticalValues(style()->height(), containerBlock, containerHeight, bordersPlusPadding,
                               top, bottom, marginTop, marginBottom,
                               h, m_marginTop, m_marginBottom, y);
    setY(y);

    // Avoid doing any work in the common case (where the values of min-height and max-height are their defaults).
    // see FIXME 3

    // Calculate constraint equation values for 'max-height' case.
    if (!style()->maxHeight().isUndefined()) {
        int maxHeight;
        int maxMarginTop;
        int maxMarginBottom;
        int maxYPos;

        calcAbsoluteVerticalValues(style()->maxHeight(), containerBlock, containerHeight, bordersPlusPadding,
                                   top, bottom, marginTop, marginBottom,
                                   maxHeight, maxMarginTop, maxMarginBottom, maxYPos);

        if (h > maxHeight) {
            h = maxHeight;
            m_marginTop = maxMarginTop;
            m_marginBottom = maxMarginBottom;
            m_frameRect.setY(maxYPos);
        }
    }

    // Calculate constraint equation values for 'min-height' case.
    if (!style()->minHeight().isZero()) {
        int minHeight;
        int minMarginTop;
        int minMarginBottom;
        int minYPos;

        calcAbsoluteVerticalValues(style()->minHeight(), containerBlock, containerHeight, bordersPlusPadding,
                                   top, bottom, marginTop, marginBottom,
                                   minHeight, minMarginTop, minMarginBottom, minYPos);

        if (h < minHeight) {
            h = minHeight;
            m_marginTop = minMarginTop;
            m_marginBottom = minMarginBottom;
            m_frameRect.setY(minYPos);
        }
    }

    // Set final height value.
    setHeight(h + bordersPlusPadding);
}

void RenderBox::calcAbsoluteVerticalValues(Length h, const RenderBox* containerBlock,
                                           const int containerHeight, const int bordersPlusPadding,
                                           const Length top, const Length bottom, const Length marginTop, const Length marginBottom,
                                           int& heightValue, int& marginTopValue, int& marginBottomValue, int& yPos)
{
    // 'top' and 'bottom' cannot both be 'auto' because 'top would of been
    // converted to the static position in calcAbsoluteVertical()
    ASSERT(!(top.isAuto() && bottom.isAuto()));

    int contentHeight = height() - bordersPlusPadding;

    int topValue = 0;

    bool heightIsAuto = h.isAuto();
    bool topIsAuto = top.isAuto();
    bool bottomIsAuto = bottom.isAuto();

    // Height is never unsolved for tables.
    if (isTable()) {
        h.setValue(Fixed, contentHeight);
        heightIsAuto = false;
    }

    if (!topIsAuto && !heightIsAuto && !bottomIsAuto) {
        /*-----------------------------------------------------------------------*\
         * If none of the three are 'auto': If both 'margin-top' and 'margin-
         * bottom' are 'auto', solve the equation under the extra constraint that
         * the two margins get equal values. If one of 'margin-top' or 'margin-
         * bottom' is 'auto', solve the equation for that value. If the values
         * are over-constrained, ignore the value for 'bottom' and solve for that
         * value.
        \*-----------------------------------------------------------------------*/
        // NOTE:  It is not necessary to solve for 'bottom' in the over constrained
        // case because the value is not used for any further calculations.

        heightValue = calcContentBoxHeight(h.calcValue(containerHeight));
        topValue = top.calcValue(containerHeight);

        const int availableSpace = containerHeight - (topValue + heightValue + bottom.calcValue(containerHeight) + bordersPlusPadding);

        // Margins are now the only unknown
        if (marginTop.isAuto() && marginBottom.isAuto()) {
            // Both margins auto, solve for equality
            // NOTE: This may result in negative values.
            marginTopValue = availableSpace / 2; // split the diference
            marginBottomValue = availableSpace - marginTopValue; // account for odd valued differences
        } else if (marginTop.isAuto()) {
            // Solve for top margin
            marginBottomValue = marginBottom.calcValue(containerHeight);
            marginTopValue = availableSpace - marginBottomValue;
        } else if (marginBottom.isAuto()) {
            // Solve for bottom margin
            marginTopValue = marginTop.calcValue(containerHeight);
            marginBottomValue = availableSpace - marginTopValue;
        } else {
            // Over-constrained, (no need solve for bottom)
            marginTopValue = marginTop.calcValue(containerHeight);
            marginBottomValue = marginBottom.calcValue(containerHeight);
        }
    } else {
        /*--------------------------------------------------------------------*\
         * Otherwise, set 'auto' values for 'margin-top' and 'margin-bottom'
         * to 0, and pick the one of the following six rules that applies.
         *
         * 1. 'top' and 'height' are 'auto' and 'bottom' is not 'auto', then
         *    the height is based on the content, and solve for 'top'.
         *
         *              OMIT RULE 2 AS IT SHOULD NEVER BE HIT
         * ------------------------------------------------------------------
         * 2. 'top' and 'bottom' are 'auto' and 'height' is not 'auto', then
         *    set 'top' to the static position, and solve for 'bottom'.
         * ------------------------------------------------------------------
         *
         * 3. 'height' and 'bottom' are 'auto' and 'top' is not 'auto', then
         *    the height is based on the content, and solve for 'bottom'.
         * 4. 'top' is 'auto', 'height' and 'bottom' are not 'auto', and
         *    solve for 'top'.
         * 5. 'height' is 'auto', 'top' and 'bottom' are not 'auto', and
         *    solve for 'height'.
         * 6. 'bottom' is 'auto', 'top' and 'height' are not 'auto', and
         *    solve for 'bottom'.
        \*--------------------------------------------------------------------*/
        // NOTE: For rules 3 and 6 it is not necessary to solve for 'bottom'
        // because the value is not used for any further calculations.

        // Calculate margins, 'auto' margins are ignored.
        marginTopValue = marginTop.calcMinValue(containerHeight);
        marginBottomValue = marginBottom.calcMinValue(containerHeight);

        const int availableSpace = containerHeight - (marginTopValue + marginBottomValue + bordersPlusPadding);

        // Use rule/case that applies.
        if (topIsAuto && heightIsAuto && !bottomIsAuto) {
            // RULE 1: (height is content based, solve of top)
            heightValue = contentHeight;
            topValue = availableSpace - (heightValue + bottom.calcValue(containerHeight));
        } else if (!topIsAuto && heightIsAuto && bottomIsAuto) {
            // RULE 3: (height is content based, no need solve of bottom)
            topValue = top.calcValue(containerHeight);
            heightValue = contentHeight;
        } else if (topIsAuto && !heightIsAuto && !bottomIsAuto) {
            // RULE 4: (solve of top)
            heightValue = calcContentBoxHeight(h.calcValue(containerHeight));
            topValue = availableSpace - (heightValue + bottom.calcValue(containerHeight));
        } else if (!topIsAuto && heightIsAuto && !bottomIsAuto) {
            // RULE 5: (solve of height)
            topValue = top.calcValue(containerHeight);
            heightValue = max(0, availableSpace - (topValue + bottom.calcValue(containerHeight)));
        } else if (!topIsAuto && !heightIsAuto && bottomIsAuto) {
            // RULE 6: (no need solve of bottom)
            heightValue = calcContentBoxHeight(h.calcValue(containerHeight));
            topValue = top.calcValue(containerHeight);
        }
    }

    // Use computed values to calculate the vertical position.
    yPos = topValue + marginTopValue + containerBlock->borderTop();
}

void RenderBox::calcAbsoluteHorizontalReplaced()
{
    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.3.8 "Absolutly positioned, replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-replaced-width>
    // (block-style-comments in this function correspond to text from the spec and
    // the numbers correspond to numbers in spec)

    // We don't use containingBlock(), since we may be positioned by an enclosing
    // relative positioned inline.
    const RenderBox* containerBlock = toRenderBox(container());

    const int containerWidth = containingBlockWidthForPositioned(containerBlock);

    // To match WinIE, in quirks mode use the parent's 'direction' property
    // instead of the the container block's.
    TextDirection containerDirection = (style()->htmlHacks()) ? parent()->style()->direction() : containerBlock->style()->direction();

    // Variables to solve.
    Length left = style()->left();
    Length right = style()->right();
    Length marginLeft = style()->marginLeft();
    Length marginRight = style()->marginRight();


    /*-----------------------------------------------------------------------*\
     * 1. The used value of 'width' is determined as for inline replaced
     *    elements.
    \*-----------------------------------------------------------------------*/
    // NOTE: This value of width is FINAL in that the min/max width calculations
    // are dealt with in calcReplacedWidth().  This means that the steps to produce
    // correct max/min in the non-replaced version, are not necessary.
    setWidth(calcReplacedWidth() + borderLeft() + borderRight() + paddingLeft() + paddingRight());
    const int availableSpace = containerWidth - width();

    /*-----------------------------------------------------------------------*\
     * 2. If both 'left' and 'right' have the value 'auto', then if 'direction'
     *    of the containing block is 'ltr', set 'left' to the static position;
     *    else if 'direction' is 'rtl', set 'right' to the static position.
    \*-----------------------------------------------------------------------*/
    // see FIXME 2
    if (left.isAuto() && right.isAuto()) {
        // see FIXME 1
        if (containerDirection == LTR) {
            // 'staticX' should already have been set through layout of the parent.
            int staticPosition = staticX() - containerBlock->borderLeft();
            for (RenderBox* po = parentBox(); po && po != containerBlock; po = po->parentBox())
                staticPosition += po->x();
            left.setValue(Fixed, staticPosition);
        } else {
            RenderBox* po = parentBox();
            // 'staticX' should already have been set through layout of the parent.
            int staticPosition = staticX() + containerWidth + containerBlock->borderRight() - po->width();
            for (; po && po != containerBlock; po = po->parentBox())
                staticPosition -= po->x();
            right.setValue(Fixed, staticPosition);
        }
    }

    /*-----------------------------------------------------------------------*\
     * 3. If 'left' or 'right' are 'auto', replace any 'auto' on 'margin-left'
     *    or 'margin-right' with '0'.
    \*-----------------------------------------------------------------------*/
    if (left.isAuto() || right.isAuto()) {
        if (marginLeft.isAuto())
            marginLeft.setValue(Fixed, 0);
        if (marginRight.isAuto())
            marginRight.setValue(Fixed, 0);
    }

    /*-----------------------------------------------------------------------*\
     * 4. If at this point both 'margin-left' and 'margin-right' are still
     *    'auto', solve the equation under the extra constraint that the two
     *    margins must get equal values, unless this would make them negative,
     *    in which case when the direction of the containing block is 'ltr'
     *    ('rtl'), set 'margin-left' ('margin-right') to zero and solve for
     *    'margin-right' ('margin-left').
    \*-----------------------------------------------------------------------*/
    int leftValue = 0;
    int rightValue = 0;

    if (marginLeft.isAuto() && marginRight.isAuto()) {
        // 'left' and 'right' cannot be 'auto' due to step 3
        ASSERT(!(left.isAuto() && right.isAuto()));

        leftValue = left.calcValue(containerWidth);
        rightValue = right.calcValue(containerWidth);

        int difference = availableSpace - (leftValue + rightValue);
        if (difference > 0) {
            m_marginLeft = difference / 2; // split the diference
            m_marginRight = difference - m_marginLeft; // account for odd valued differences
        } else {
            // see FIXME 1
            if (containerDirection == LTR) {
                m_marginLeft = 0;
                m_marginRight = difference;  // will be negative
            } else {
                m_marginLeft = difference;  // will be negative
                m_marginRight = 0;
            }
        }

    /*-----------------------------------------------------------------------*\
     * 5. If at this point there is an 'auto' left, solve the equation for
     *    that value.
    \*-----------------------------------------------------------------------*/
    } else if (left.isAuto()) {
        m_marginLeft = marginLeft.calcValue(containerWidth);
        m_marginRight = marginRight.calcValue(containerWidth);
        rightValue = right.calcValue(containerWidth);

        // Solve for 'left'
        leftValue = availableSpace - (rightValue + m_marginLeft + m_marginRight);
    } else if (right.isAuto()) {
        m_marginLeft = marginLeft.calcValue(containerWidth);
        m_marginRight = marginRight.calcValue(containerWidth);
        leftValue = left.calcValue(containerWidth);

        // Solve for 'right'
        rightValue = availableSpace - (leftValue + m_marginLeft + m_marginRight);
    } else if (marginLeft.isAuto()) {
        m_marginRight = marginRight.calcValue(containerWidth);
        leftValue = left.calcValue(containerWidth);
        rightValue = right.calcValue(containerWidth);

        // Solve for 'margin-left'
        m_marginLeft = availableSpace - (leftValue + rightValue + m_marginRight);
    } else if (marginRight.isAuto()) {
        m_marginLeft = marginLeft.calcValue(containerWidth);
        leftValue = left.calcValue(containerWidth);
        rightValue = right.calcValue(containerWidth);

        // Solve for 'margin-right'
        m_marginRight = availableSpace - (leftValue + rightValue + m_marginLeft);
    } else {
        // Nothing is 'auto', just calculate the values.
        m_marginLeft = marginLeft.calcValue(containerWidth);
        m_marginRight = marginRight.calcValue(containerWidth);
        rightValue = right.calcValue(containerWidth);
        leftValue = left.calcValue(containerWidth);
    }

    /*-----------------------------------------------------------------------*\
     * 6. If at this point the values are over-constrained, ignore the value
     *    for either 'left' (in case the 'direction' property of the
     *    containing block is 'rtl') or 'right' (in case 'direction' is
     *    'ltr') and solve for that value.
    \*-----------------------------------------------------------------------*/
    // NOTE:  It is not necessary to solve for 'right' when the direction is
    // LTR because the value is not used.
    int totalWidth = width() + leftValue + rightValue +  m_marginLeft + m_marginRight;
    if (totalWidth > containerWidth && (containerDirection == RTL))
        leftValue = containerWidth - (totalWidth - leftValue);

    // Use computed values to calculate the horizontal position.

    // FIXME: This hack is needed to calculate the xPos for a 'rtl' relatively
    // positioned, inline containing block because right now, it is using the xPos
    // of the first line box when really it should use the last line box.  When
    // this is fixed elsewhere, this block should be removed.
    if (containerBlock->isInline() && containerBlock->style()->direction() == RTL) {
        const RenderFlow* flow = static_cast<const RenderFlow*>(containerBlock);
        InlineFlowBox* firstLine = flow->firstLineBox();
        InlineFlowBox* lastLine = flow->lastLineBox();
        if (firstLine && lastLine && firstLine != lastLine) {
            m_frameRect.setX(leftValue + m_marginLeft + lastLine->borderLeft() + (lastLine->xPos() - firstLine->xPos()));
            return;
        }
    }

    m_frameRect.setX(leftValue + m_marginLeft + containerBlock->borderLeft());
}

void RenderBox::calcAbsoluteVerticalReplaced()
{
    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.6.5 "Absolutly positioned, replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-replaced-height>
    // (block-style-comments in this function correspond to text from the spec and
    // the numbers correspond to numbers in spec)

    // We don't use containingBlock(), since we may be positioned by an enclosing relpositioned inline.
    const RenderBox* containerBlock = toRenderBox(container());

    const int containerHeight = containingBlockHeightForPositioned(containerBlock);

    // Variables to solve.
    Length top = style()->top();
    Length bottom = style()->bottom();
    Length marginTop = style()->marginTop();
    Length marginBottom = style()->marginBottom();


    /*-----------------------------------------------------------------------*\
     * 1. The used value of 'height' is determined as for inline replaced
     *    elements.
    \*-----------------------------------------------------------------------*/
    // NOTE: This value of height is FINAL in that the min/max height calculations
    // are dealt with in calcReplacedHeight().  This means that the steps to produce
    // correct max/min in the non-replaced version, are not necessary.
    setHeight(calcReplacedHeight() + borderTop() + borderBottom() + paddingTop() + paddingBottom());
    const int availableSpace = containerHeight - height();

    /*-----------------------------------------------------------------------*\
     * 2. If both 'top' and 'bottom' have the value 'auto', replace 'top'
     *    with the element's static position.
    \*-----------------------------------------------------------------------*/
    // see FIXME 2
    if (top.isAuto() && bottom.isAuto()) {
        // staticY should already have been set through layout of the parent().
        int staticTop = staticY() - containerBlock->borderTop();
        for (RenderBox* po = parentBox(); po && po != containerBlock; po = po->parentBox()) {
            if (!po->isTableRow())
                staticTop += po->y();
        }
        top.setValue(Fixed, staticTop);
    }

    /*-----------------------------------------------------------------------*\
     * 3. If 'bottom' is 'auto', replace any 'auto' on 'margin-top' or
     *    'margin-bottom' with '0'.
    \*-----------------------------------------------------------------------*/
    // FIXME: The spec. says that this step should only be taken when bottom is
    // auto, but if only top is auto, this makes step 4 impossible.
    if (top.isAuto() || bottom.isAuto()) {
        if (marginTop.isAuto())
            marginTop.setValue(Fixed, 0);
        if (marginBottom.isAuto())
            marginBottom.setValue(Fixed, 0);
    }

    /*-----------------------------------------------------------------------*\
     * 4. If at this point both 'margin-top' and 'margin-bottom' are still
     *    'auto', solve the equation under the extra constraint that the two
     *    margins must get equal values.
    \*-----------------------------------------------------------------------*/
    int topValue = 0;
    int bottomValue = 0;

    if (marginTop.isAuto() && marginBottom.isAuto()) {
        // 'top' and 'bottom' cannot be 'auto' due to step 2 and 3 combinded.
        ASSERT(!(top.isAuto() || bottom.isAuto()));

        topValue = top.calcValue(containerHeight);
        bottomValue = bottom.calcValue(containerHeight);

        int difference = availableSpace - (topValue + bottomValue);
        // NOTE: This may result in negative values.
        m_marginTop =  difference / 2; // split the difference
        m_marginBottom = difference - m_marginTop; // account for odd valued differences

    /*-----------------------------------------------------------------------*\
     * 5. If at this point there is only one 'auto' left, solve the equation
     *    for that value.
    \*-----------------------------------------------------------------------*/
    } else if (top.isAuto()) {
        m_marginTop = marginTop.calcValue(containerHeight);
        m_marginBottom = marginBottom.calcValue(containerHeight);
        bottomValue = bottom.calcValue(containerHeight);

        // Solve for 'top'
        topValue = availableSpace - (bottomValue + m_marginTop + m_marginBottom);
    } else if (bottom.isAuto()) {
        m_marginTop = marginTop.calcValue(containerHeight);
        m_marginBottom = marginBottom.calcValue(containerHeight);
        topValue = top.calcValue(containerHeight);

        // Solve for 'bottom'
        // NOTE: It is not necessary to solve for 'bottom' because we don't ever
        // use the value.
    } else if (marginTop.isAuto()) {
        m_marginBottom = marginBottom.calcValue(containerHeight);
        topValue = top.calcValue(containerHeight);
        bottomValue = bottom.calcValue(containerHeight);

        // Solve for 'margin-top'
        m_marginTop = availableSpace - (topValue + bottomValue + m_marginBottom);
    } else if (marginBottom.isAuto()) {
        m_marginTop = marginTop.calcValue(containerHeight);
        topValue = top.calcValue(containerHeight);
        bottomValue = bottom.calcValue(containerHeight);

        // Solve for 'margin-bottom'
        m_marginBottom = availableSpace - (topValue + bottomValue + m_marginTop);
    } else {
        // Nothing is 'auto', just calculate the values.
        m_marginTop = marginTop.calcValue(containerHeight);
        m_marginBottom = marginBottom.calcValue(containerHeight);
        topValue = top.calcValue(containerHeight);
        // NOTE: It is not necessary to solve for 'bottom' because we don't ever
        // use the value.
     }

    /*-----------------------------------------------------------------------*\
     * 6. If at this point the values are over-constrained, ignore the value
     *    for 'bottom' and solve for that value.
    \*-----------------------------------------------------------------------*/
    // NOTE: It is not necessary to do this step because we don't end up using
    // the value of 'bottom' regardless of whether the values are over-constrained
    // or not.

    // Use computed values to calculate the vertical position.
    m_frameRect.setY(topValue + m_marginTop + containerBlock->borderTop());
}

IntRect RenderBox::localCaretRect(InlineBox* box, int caretOffset, int* extraWidthToEndOfLine)
{
    // VisiblePositions at offsets inside containers either a) refer to the positions before/after
    // those containers (tables and select elements) or b) refer to the position inside an empty block.
    // They never refer to children.
    // FIXME: Paint the carets inside empty blocks differently than the carets before/after elements.

    // FIXME: What about border and padding?
    const int caretWidth = 1;
    IntRect rect(x(), y(), caretWidth, height());
    TextDirection direction = box ? box->direction() : style()->direction();

    if ((!caretOffset) ^ (direction == LTR))
        rect.move(IntSize(width() - caretWidth, 0));

    if (box) {
        RootInlineBox* rootBox = box->root();
        int top = rootBox->topOverflow();
        rect.setY(top);
        rect.setHeight(rootBox->bottomOverflow() - top);
    }

    // If height of box is smaller than font height, use the latter one,
    // otherwise the caret might become invisible.
    //
    // Also, if the box is not a replaced element, always use the font height.
    // This prevents the "big caret" bug described in:
    // <rdar://problem/3777804> Deleting all content in a document can result in giant tall-as-window insertion point
    //
    // FIXME: ignoring :first-line, missing good reason to take care of
    int fontHeight = style()->font().height();
    if (fontHeight > rect.height() || !isReplaced() && !isTable())
        rect.setHeight(fontHeight);

    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = x() + width() - rect.right();

    // Move to local coords
    rect.move(-x(), -y());
    return rect;
}

int RenderBox::lowestPosition(bool /*includeOverflowInterior*/, bool includeSelf) const
{
    if (!includeSelf || !width())
        return 0;
    int bottom = height();
    if (isRelPositioned())
        bottom += relativePositionOffsetY();
    return bottom;
}

int RenderBox::rightmostPosition(bool /*includeOverflowInterior*/, bool includeSelf) const
{
    if (!includeSelf || !height())
        return 0;
    int right = width();
    if (isRelPositioned())
        right += relativePositionOffsetX();
    return right;
}

int RenderBox::leftmostPosition(bool /*includeOverflowInterior*/, bool includeSelf) const
{
    if (!includeSelf || !height())
        return width();
    int left = 0;
    if (isRelPositioned())
        left += relativePositionOffsetX();
    return left;
}

#if ENABLE(SVG)

TransformationMatrix RenderBox::localTransform() const
{
    return TransformationMatrix(1, 0, 0, 1, x(), y());
}

#endif

} // namespace WebCore
