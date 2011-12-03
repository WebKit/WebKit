/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "htmlediting.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderArena.h"
#include "RenderBoxRegionInfo.h"
#include "RenderFlowThread.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderPart.h"
#include "RenderRegion.h"
#include "RenderTableCell.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "ScrollbarTheme.h"
#include "TransformState.h"
#include <algorithm>
#include <math.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

// Used by flexible boxes when flexing this element and by table cells.
typedef WTF::HashMap<const RenderBox*, LayoutUnit> OverrideSizeMap;
static OverrideSizeMap* gOverrideHeightMap = 0;
static OverrideSizeMap* gOverrideWidthMap = 0;

bool RenderBox::s_hadOverflowClip = false;

RenderBox::RenderBox(Node* node)
    : RenderBoxModelObject(node)
    , m_marginLeft(0)
    , m_marginRight(0)
    , m_marginTop(0)
    , m_marginBottom(0)
    , m_minPreferredLogicalWidth(-1)
    , m_maxPreferredLogicalWidth(-1)
    , m_inlineBoxWrapper(0)
{
    setIsBox();
}

RenderBox::~RenderBox()
{
}

LayoutUnit RenderBox::marginBefore() const
{
    switch (style()->writingMode()) {
    case TopToBottomWritingMode:
        return m_marginTop;
    case BottomToTopWritingMode:
        return m_marginBottom;
    case LeftToRightWritingMode:
        return m_marginLeft;
    case RightToLeftWritingMode:
        return m_marginRight;
    }
    ASSERT_NOT_REACHED();
    return m_marginTop;
}

LayoutUnit RenderBox::marginAfter() const
{
    switch (style()->writingMode()) {
    case TopToBottomWritingMode:
        return m_marginBottom;
    case BottomToTopWritingMode:
        return m_marginTop;
    case LeftToRightWritingMode:
        return m_marginRight;
    case RightToLeftWritingMode:
        return m_marginLeft;
    }
    ASSERT_NOT_REACHED();
    return m_marginBottom;
}

LayoutUnit RenderBox::marginStart() const
{
    if (isHorizontalWritingMode())
        return style()->isLeftToRightDirection() ? m_marginLeft : m_marginRight;
    return style()->isLeftToRightDirection() ? m_marginTop : m_marginBottom;
}

LayoutUnit RenderBox::marginEnd() const
{
    if (isHorizontalWritingMode())
        return style()->isLeftToRightDirection() ? m_marginRight : m_marginLeft;
    return style()->isLeftToRightDirection() ? m_marginBottom : m_marginTop;
}

void RenderBox::setMarginStart(LayoutUnit margin)
{
    if (isHorizontalWritingMode()) {
        if (style()->isLeftToRightDirection())
            m_marginLeft = margin;
        else
            m_marginRight = margin;
    } else {
        if (style()->isLeftToRightDirection())
            m_marginTop = margin;
        else
            m_marginBottom = margin;
    }
}

void RenderBox::setMarginEnd(LayoutUnit margin)
{
    if (isHorizontalWritingMode()) {
        if (style()->isLeftToRightDirection())
            m_marginRight = margin;
        else
            m_marginLeft = margin;
    } else {
        if (style()->isLeftToRightDirection())
            m_marginBottom = margin;
        else
            m_marginTop = margin;
    }
}

void RenderBox::setMarginBefore(LayoutUnit margin)
{
    switch (style()->writingMode()) {
    case TopToBottomWritingMode:
        m_marginTop = margin;
        break;
    case BottomToTopWritingMode:
        m_marginBottom = margin;
        break;
    case LeftToRightWritingMode:
        m_marginLeft = margin;
        break;
    case RightToLeftWritingMode:
        m_marginRight = margin;
        break;
    }
}

void RenderBox::setMarginAfter(LayoutUnit margin)
{
    switch (style()->writingMode()) {
    case TopToBottomWritingMode:
        m_marginBottom = margin;
        break;
    case BottomToTopWritingMode:
        m_marginTop = margin;
        break;
    case LeftToRightWritingMode:
        m_marginRight = margin;
        break;
    case RightToLeftWritingMode:
        m_marginLeft = margin;
        break;
    }
}

LayoutRect RenderBox::borderBoxRectInRegion(RenderRegion* region, LayoutUnit offsetFromTopOfFirstPage, RenderBoxRegionInfoFlags cacheFlag) const
{
    if (!region)
        return borderBoxRect();
    
    // Compute the logical width and placement in this region.
    RenderBoxRegionInfo* boxInfo = renderBoxRegionInfo(region, offsetFromTopOfFirstPage, cacheFlag);
    if (!boxInfo)
        return borderBoxRect();

    // We have cached insets.
    LayoutUnit logicalWidth = boxInfo->logicalWidth();
    LayoutUnit logicalLeft = boxInfo->logicalLeft();
        
    // Now apply the parent inset since it is cumulative whenever anything in the containing block chain shifts.
    // FIXME: Doesn't work right with perpendicular writing modes.
    const RenderBlock* currentBox = containingBlock();
    offsetFromTopOfFirstPage -= logicalTop();
    RenderBoxRegionInfo* currentBoxInfo = currentBox->renderBoxRegionInfo(region, offsetFromTopOfFirstPage);
    while (currentBoxInfo && currentBoxInfo->isShifted()) {
        if (currentBox->style()->direction() == LTR)
            logicalLeft += currentBoxInfo->logicalLeft();
        else
            logicalLeft -= (currentBox->logicalWidth() - currentBoxInfo->logicalWidth()) - currentBoxInfo->logicalLeft();
        offsetFromTopOfFirstPage -= logicalTop();
        currentBox = currentBox->containingBlock();
        region = currentBox->clampToStartAndEndRegions(region);
        currentBoxInfo = currentBox->renderBoxRegionInfo(region, offsetFromTopOfFirstPage);
    }
    
    if (cacheFlag == DoNotCacheRenderBoxRegionInfo)
        delete boxInfo;

    if (isHorizontalWritingMode())
        return LayoutRect(logicalLeft, 0, logicalWidth, height());
    return LayoutRect(0, logicalLeft, width(), logicalWidth);
}

void RenderBox::clearRenderBoxRegionInfo()
{
    if (!inRenderFlowThread() || isRenderFlowThread())
        return;

    RenderFlowThread* flowThread = enclosingRenderFlowThread();
    if (!flowThread->hasValidRegions())
        return;

    flowThread->removeRenderBoxRegionInfo(this);
}

void RenderBox::willBeDestroyed()
{
    clearOverrideSize();

    if (style() && (style()->logicalHeight().isPercent() || style()->logicalMinHeight().isPercent() || style()->logicalMaxHeight().isPercent()))
        RenderBlock::removePercentHeightDescendant(this);

    // If this renderer is owning renderer for the frameview's custom scrollbars,
    // we need to clear it from the scrollbar. See webkit bug 64737.
    if (style() && style()->hasPseudoStyle(SCROLLBAR) && frame() && frame()->view())
        frame()->view()->clearOwningRendererForCustomScrollbars(this);

    // If the following assertion fails, logicalHeight()/logicalMinHeight()/
    // logicalMaxHeight() values are changed from a percent value to a non-percent
    // value during laying out. It causes a use-after-free bug.
    ASSERT(!RenderBlock::hasPercentHeightDescendant(this));

    RenderBoxModelObject::willBeDestroyed();
}

void RenderBox::removeFloatingOrPositionedChildFromBlockLists()
{
    ASSERT(isFloatingOrPositioned());

    if (documentBeingDestroyed())
        return;

    if (isFloating()) {
        RenderBlock* parentBlock = 0;
        for (RenderObject* curr = parent(); curr && !curr->isRenderView(); curr = curr->parent()) {
            if (curr->isRenderBlock()) {
                RenderBlock* currBlock = toRenderBlock(curr);
                if (!parentBlock || currBlock->containsFloat(this))
                    parentBlock = currBlock;
            }
        }

        if (parentBlock) {
            RenderObject* parent = parentBlock->parent();
            if (parent && parent->isDeprecatedFlexibleBox())
                parentBlock = toRenderBlock(parent);

            parentBlock->markAllDescendantsWithFloatsForLayout(this, false);
        }
    }

    if (isPositioned()) {
        for (RenderObject* curr = parent(); curr; curr = curr->parent()) {
            if (curr->isRenderBlock())
                toRenderBlock(curr)->removePositionedObject(this);
        }
    }
}

void RenderBox::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    s_hadOverflowClip = hasOverflowClip();

    if (style()) {
        // The background of the root element or the body element could propagate up to
        // the canvas.  Just dirty the entire canvas when our style changes substantially.
        if (diff >= StyleDifferenceRepaint && node() &&
                (node()->hasTagName(htmlTag) || node()->hasTagName(bodyTag)))
            view()->repaint();
        
        // When a layout hint happens and an object's position style changes, we have to do a layout
        // to dirty the render tree using the old position value now.
        if (diff == StyleDifferenceLayout && parent() && style()->position() != newStyle->position()) {
            markContainingBlocksForLayout();
            if (style()->position() == StaticPosition)
                repaint();
            else if (newStyle->position() == AbsolutePosition || newStyle->position() == FixedPosition)
                parent()->setChildNeedsLayout(true);
            if (isFloating() && !isPositioned() && (newStyle->position() == AbsolutePosition || newStyle->position() == FixedPosition))
                removeFloatingOrPositionedChildFromBlockLists();
        }
    } else if (newStyle && isBody())
        view()->repaint();

    if (FrameView *frameView = view()->frameView()) {
        bool newStyleIsFixed = newStyle && newStyle->position() == FixedPosition;
        bool oldStyleIsFixed = style() && style()->position() == FixedPosition;
        if (newStyleIsFixed != oldStyleIsFixed) {
            if (newStyleIsFixed)
                frameView->addFixedObject();
            else
                frameView->removeFixedObject();
        }
    }

    RenderBoxModelObject::styleWillChange(diff, newStyle);
}

void RenderBox::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBoxModelObject::styleDidChange(diff, oldStyle);

    if (needsLayout() && oldStyle) {
        if (oldStyle && (oldStyle->logicalHeight().isPercent() || oldStyle->logicalMinHeight().isPercent() || oldStyle->logicalMaxHeight().isPercent()))
            RenderBlock::removePercentHeightDescendant(this);

        // Normally we can do optimized positioning layout for absolute/fixed positioned objects. There is one special case, however, which is
        // when the positioned object's margin-before is changed. In this case the parent has to get a layout in order to run margin collapsing
        // to determine the new static position.
        if (isPositioned() && style()->hasStaticBlockPosition(isHorizontalWritingMode()) && oldStyle->marginBefore() != style()->marginBefore()
            && parent() && !parent()->normalChildNeedsLayout())
            parent()->setChildNeedsLayout(true);
    }

    // If our zoom factor changes and we have a defined scrollLeft/Top, we need to adjust that value into the
    // new zoomed coordinate space.
    if (hasOverflowClip() && oldStyle && style() && oldStyle->effectiveZoom() != style()->effectiveZoom()) {
        if (int left = layer()->scrollXOffset()) {
            left = (left / oldStyle->effectiveZoom()) * style()->effectiveZoom();
            layer()->scrollToXOffset(left);
        }
        if (int top = layer()->scrollYOffset()) {
            top = (top / oldStyle->effectiveZoom()) * style()->effectiveZoom();
            layer()->scrollToYOffset(top);
        }
    }

    bool isBodyRenderer = isBody();
    bool isRootRenderer = isRoot();

    // Set the text color if we're the body.
    if (isBodyRenderer)
        document()->setTextColor(style()->visitedDependentColor(CSSPropertyColor));

    if (isRootRenderer || isBodyRenderer) {
        // Propagate the new writing mode and direction up to the RenderView.
        RenderView* viewRenderer = view();
        RenderStyle* viewStyle = viewRenderer->style();
        if (viewStyle->direction() != style()->direction() && (isRootRenderer || !document()->directionSetOnDocumentElement())) {
            viewStyle->setDirection(style()->direction());
            if (isBodyRenderer)
                document()->documentElement()->renderer()->style()->setDirection(style()->direction());
            setNeedsLayoutAndPrefWidthsRecalc();
        }

        if (viewStyle->writingMode() != style()->writingMode() && (isRootRenderer || !document()->writingModeSetOnDocumentElement())) {
            viewStyle->setWritingMode(style()->writingMode());
            viewRenderer->setHorizontalWritingMode(style()->isHorizontalWritingMode());
            if (isBodyRenderer) {
                document()->documentElement()->renderer()->style()->setWritingMode(style()->writingMode());
                document()->documentElement()->renderer()->setHorizontalWritingMode(style()->isHorizontalWritingMode());
            }
            setNeedsLayoutAndPrefWidthsRecalc();
        }

        frame()->view()->recalculateScrollbarOverlayStyle();
    }
}

void RenderBox::updateBoxModelInfoFromStyle()
{
    RenderBoxModelObject::updateBoxModelInfoFromStyle();

    bool isRootObject = isRoot();
    bool isViewObject = isRenderView();

    // The root and the RenderView always paint their backgrounds/borders.
    if (isRootObject || isViewObject)
        setHasBoxDecorations(true);

    setPositioned(style()->position() == AbsolutePosition || style()->position() == FixedPosition);
    setFloating(style()->isFloating() && (!isPositioned() || style()->floating() == PositionedFloat));

    // We also handle <body> and <html>, whose overflow applies to the viewport.
    if (style()->overflowX() != OVISIBLE && !isRootObject && (isRenderBlock() || isTableRow() || isTableSection())) {
        bool boxHasOverflowClip = true;
        if (isBody()) {
            // Overflow on the body can propagate to the viewport under the following conditions.
            // (1) The root element is <html>.
            // (2) We are the primary <body> (can be checked by looking at document.body).
            // (3) The root element has visible overflow.
            if (document()->documentElement()->hasTagName(htmlTag) &&
                document()->body() == node() &&
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

    setHasTransform(style()->hasTransformRelatedProperty());
    setHasReflection(style()->boxReflect());
}

void RenderBox::layout()
{
    ASSERT(needsLayout());

    RenderObject* child = firstChild();
    if (!child) {
        setNeedsLayout(false);
        return;
    }

    LayoutStateMaintainer statePusher(view(), this, locationOffset(), style()->isFlippedBlocksWritingMode());
    while (child) {
        child->layoutIfNeeded();
        ASSERT(!child->needsLayout());
        child = child->nextSibling();
    }
    statePusher.pop();
    setNeedsLayout(false);
}

// More IE extensions.  clientWidth and clientHeight represent the interior of an object
// excluding border and scrollbar.
LayoutUnit RenderBox::clientWidth() const
{
    return width() - borderLeft() - borderRight() - verticalScrollbarWidth();
}

LayoutUnit RenderBox::clientHeight() const
{
    return height() - borderTop() - borderBottom() - horizontalScrollbarHeight();
}

int RenderBox::scrollWidth() const
{
    if (hasOverflowClip())
        return layer()->scrollWidth();
    // For objects with visible overflow, this matches IE.
    // FIXME: Need to work right with writing modes.
    if (style()->isLeftToRightDirection())
        return max(clientWidth(), maxXLayoutOverflow() - borderLeft());
    return clientWidth() - min(0, minXLayoutOverflow() - borderLeft());
}

int RenderBox::scrollHeight() const
{
    if (hasOverflowClip())
        return layer()->scrollHeight();
    // For objects with visible overflow, this matches IE.
    // FIXME: Need to work right with writing modes.
    return max(clientHeight(), maxYLayoutOverflow() - borderTop());
}

int RenderBox::scrollLeft() const
{
    return hasOverflowClip() ? layer()->scrollXOffset() : 0;
}

int RenderBox::scrollTop() const
{
    return hasOverflowClip() ? layer()->scrollYOffset() : 0;
}

void RenderBox::setScrollLeft(int newLeft)
{
    if (hasOverflowClip())
        layer()->scrollToXOffset(newLeft, RenderLayer::ScrollOffsetClamped);
}

void RenderBox::setScrollTop(int newTop)
{
    if (hasOverflowClip())
        layer()->scrollToYOffset(newTop, RenderLayer::ScrollOffsetClamped);
}

void RenderBox::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    rects.append(LayoutRect(accumulatedOffset, size()));
}

void RenderBox::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    quads.append(localToAbsoluteQuad(FloatRect(0, 0, width(), height()), false, wasFixed));
}

void RenderBox::updateLayerTransform()
{
    // Transform-origin depends on box size, so we need to update the layer transform after layout.
    if (hasLayer())
        layer()->updateTransform();
}

LayoutRect RenderBox::absoluteContentBox() const
{
    LayoutRect rect = contentBoxRect();
    FloatPoint absPos = localToAbsolute(FloatPoint());
    rect.move(absPos.x(), absPos.y());
    return rect;
}

FloatQuad RenderBox::absoluteContentQuad() const
{
    LayoutRect rect = contentBoxRect();
    return localToAbsoluteQuad(FloatRect(rect));
}

LayoutRect RenderBox::outlineBoundsForRepaint(RenderBoxModelObject* repaintContainer, LayoutPoint* cachedOffsetToRepaintContainer) const
{
    LayoutRect box = borderBoundingBox();
    adjustRectForOutlineAndShadow(box);

    FloatQuad containerRelativeQuad = FloatRect(box);
    if (cachedOffsetToRepaintContainer)
        containerRelativeQuad.move(cachedOffsetToRepaintContainer->x(), cachedOffsetToRepaintContainer->y());
    else
        containerRelativeQuad = localToContainerQuad(containerRelativeQuad, repaintContainer);

    box = containerRelativeQuad.enclosingBoundingBox();

    // FIXME: layoutDelta needs to be applied in parts before/after transforms and
    // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
    box.move(view()->layoutDelta());

    return box;
}

void RenderBox::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset)
{
    if (!size().isEmpty())
        rects.append(LayoutRect(additionalOffset, size()));
}

LayoutRect RenderBox::reflectionBox() const
{
    LayoutRect result;
    if (!style()->boxReflect())
        return result;
    LayoutRect box = borderBoxRect();
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

LayoutRect RenderBox::reflectedRect(const LayoutRect& r) const
{
    if (!style()->boxReflect())
        return LayoutRect();

    LayoutRect box = borderBoxRect();
    LayoutRect result = r;
    switch (style()->boxReflect()->direction()) {
        case ReflectionBelow:
            result.setY(box.maxY() + reflectionOffset() + (box.maxY() - r.maxY()));
            break;
        case ReflectionAbove:
            result.setY(box.y() - reflectionOffset() - box.height() + (box.maxY() - r.maxY()));
            break;
        case ReflectionLeft:
            result.setX(box.x() - reflectionOffset() - box.width() + (box.maxX() - r.maxX()));
            break;
        case ReflectionRight:
            result.setX(box.maxX() + reflectionOffset() + (box.maxX() - r.maxX()));
            break;
    }
    return result;
}

bool RenderBox::shouldLayoutFixedElementRelativeToFrame(Frame* frame, FrameView* frameView) const
{
    return style() && style()->position() == FixedPosition && container()->isRenderView() && frame && frameView && frameView->shouldLayoutFixedElementsRelativeToFrame();
}

bool RenderBox::includeVerticalScrollbarSize() const
{
    return hasOverflowClip() && !layer()->hasOverlayScrollbars()
        && (style()->overflowY() == OSCROLL || style()->overflowY() == OAUTO);
}

bool RenderBox::includeHorizontalScrollbarSize() const
{
    return hasOverflowClip() && !layer()->hasOverlayScrollbars()
        && (style()->overflowX() == OSCROLL || style()->overflowX() == OAUTO);
}

int RenderBox::verticalScrollbarWidth() const
{
    return includeVerticalScrollbarSize() ? layer()->verticalScrollbarWidth() : 0;
}

int RenderBox::horizontalScrollbarHeight() const
{
    return includeHorizontalScrollbarSize() ? layer()->horizontalScrollbarHeight() : 0;
}

bool RenderBox::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier, Node** stopNode)
{
    RenderLayer* l = layer();
    if (l && l->scroll(direction, granularity, multiplier)) {
        if (stopNode)
            *stopNode = node();
        return true;
    }

    if (stopNode && *stopNode && *stopNode == node())
        return true;

    RenderBlock* b = containingBlock();
    if (b && !b->isRenderView())
        return b->scroll(direction, granularity, multiplier, stopNode);
    return false;
}

bool RenderBox::logicalScroll(ScrollLogicalDirection direction, ScrollGranularity granularity, float multiplier, Node** stopNode)
{
    bool scrolled = false;
    
    RenderLayer* l = layer();
    if (l) {
#if PLATFORM(MAC)
        // On Mac only we reset the inline direction position when doing a document scroll (e.g., hitting Home/End).
        if (granularity == ScrollByDocument)
            scrolled = l->scroll(logicalToPhysical(ScrollInlineDirectionBackward, isHorizontalWritingMode(), style()->isFlippedBlocksWritingMode()), ScrollByDocument, multiplier);
#endif
        if (l->scroll(logicalToPhysical(direction, isHorizontalWritingMode(), style()->isFlippedBlocksWritingMode()), granularity, multiplier))
            scrolled = true;
        
        if (scrolled) {
            if (stopNode)
                *stopNode = node();
            return true;
        }
    }

    if (stopNode && *stopNode && *stopNode == node())
        return true;

    RenderBlock* b = containingBlock();
    if (b && !b->isRenderView())
        return b->logicalScroll(direction, granularity, multiplier, stopNode);
    return false;
}

bool RenderBox::canBeScrolledAndHasScrollableArea() const
{
    return canBeProgramaticallyScrolled() && (scrollHeight() != clientHeight() || scrollWidth() != clientWidth());
}
    
bool RenderBox::canBeProgramaticallyScrolled() const
{
    return (hasOverflowClip() && (scrollsOverflow() || (node() && node()->rendererIsEditable()))) || (node() && node()->isDocumentNode());
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

bool RenderBox::needsPreferredWidthsRecalculation() const
{
    return style()->paddingStart().isPercent() || style()->paddingEnd().isPercent();
}

LayoutUnit RenderBox::minPreferredLogicalWidth() const
{
    if (preferredLogicalWidthsDirty())
        const_cast<RenderBox*>(this)->computePreferredLogicalWidths();
        
    return m_minPreferredLogicalWidth;
}

LayoutUnit RenderBox::maxPreferredLogicalWidth() const
{
    if (preferredLogicalWidthsDirty())
        const_cast<RenderBox*>(this)->computePreferredLogicalWidths();
        
    return m_maxPreferredLogicalWidth;
}

bool RenderBox::hasOverrideHeight() const
{
    return gOverrideHeightMap && gOverrideHeightMap->contains(this);
}

bool RenderBox::hasOverrideWidth() const
{
    return gOverrideWidthMap && gOverrideWidthMap->contains(this);
}

void RenderBox::setOverrideHeight(LayoutUnit height)
{
    if (!gOverrideHeightMap)
        gOverrideHeightMap = new OverrideSizeMap();
    gOverrideHeightMap->set(this, height);
}

void RenderBox::setOverrideWidth(LayoutUnit width)
{
    if (!gOverrideWidthMap)
        gOverrideWidthMap = new OverrideSizeMap();
    gOverrideWidthMap->set(this, width);
}

void RenderBox::clearOverrideSize()
{
    if (hasOverrideHeight())
        gOverrideHeightMap->remove(this);
    if (hasOverrideWidth())
        gOverrideWidthMap->remove(this);
}

LayoutUnit RenderBox::overrideWidth() const
{
    return hasOverrideWidth() ? gOverrideWidthMap->get(this) : width();
}

LayoutUnit RenderBox::overrideHeight() const
{
    return hasOverrideHeight() ? gOverrideHeightMap->get(this) : height();
}

LayoutUnit RenderBox::computeBorderBoxLogicalWidth(LayoutUnit width) const
{
    LayoutUnit bordersPlusPadding = borderAndPaddingLogicalWidth();
    if (style()->boxSizing() == CONTENT_BOX)
        return width + bordersPlusPadding;
    return max(width, bordersPlusPadding);
}

LayoutUnit RenderBox::computeBorderBoxLogicalHeight(LayoutUnit height) const
{
    LayoutUnit bordersPlusPadding = borderAndPaddingLogicalHeight();
    if (style()->boxSizing() == CONTENT_BOX)
        return height + bordersPlusPadding;
    return max(height, bordersPlusPadding);
}

LayoutUnit RenderBox::computeContentBoxLogicalWidth(LayoutUnit width) const
{
    if (style()->boxSizing() == BORDER_BOX)
        width -= borderAndPaddingLogicalWidth();
    return max<LayoutUnit>(0, width);
}

LayoutUnit RenderBox::computeContentBoxLogicalHeight(LayoutUnit height) const
{
    if (style()->boxSizing() == BORDER_BOX)
        height -= borderAndPaddingLogicalHeight();
    return max<LayoutUnit>(0, height);
}

// Hit Testing
bool RenderBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    LayoutPoint adjustedLocation = accumulatedOffset + location();

    // Check kids first.
    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (!child->hasLayer() && child->nodeAtPoint(request, result, pointInContainer, adjustedLocation, action)) {
            updateHitTestResult(result, pointInContainer - toLayoutSize(adjustedLocation));
            return true;
        }
    }

    // Check our bounds next. For this purpose always assume that we can only be hit in the
    // foreground phase (which is true for replaced elements like images).
    LayoutRect boundsRect = borderBoxRectInRegion(result.region());
    boundsRect.moveBy(adjustedLocation);
    if (visibleToHitTesting() && action == HitTestForeground && boundsRect.intersects(result.rectForPoint(pointInContainer))) {
        updateHitTestResult(result, pointInContainer - toLayoutSize(adjustedLocation));
        if (!result.addNodeToRectBasedTestResult(node(), pointInContainer, boundsRect))
            return true;
    }

    return false;
}

// --------------------- painting stuff -------------------------------

void RenderBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + location();
    // default implementation. Just pass paint through to the children
    PaintInfo childInfo(paintInfo);
    childInfo.updatePaintingRootForChildren(this);
    for (RenderObject* child = firstChild(); child; child = child->nextSibling())
        child->paint(childInfo, adjustedPaintOffset);
}

void RenderBox::paintRootBoxFillLayers(const PaintInfo& paintInfo)
{
    const FillLayer* bgLayer = style()->backgroundLayers();
    Color bgColor = style()->visitedDependentColor(CSSPropertyBackgroundColor);
    RenderObject* bodyObject = 0;
    if (!hasBackground() && node() && node()->hasTagName(HTMLNames::htmlTag)) {
        // Locate the <body> element using the DOM.  This is easier than trying
        // to crawl around a render tree with potential :before/:after content and
        // anonymous blocks created by inline <body> tags etc.  We can locate the <body>
        // render object very easily via the DOM.
        HTMLElement* body = document()->body();
        bodyObject = (body && body->hasLocalName(bodyTag)) ? body->renderer() : 0;
        if (bodyObject) {
            bgLayer = bodyObject->style()->backgroundLayers();
            bgColor = bodyObject->style()->visitedDependentColor(CSSPropertyBackgroundColor);
        }
    }

    paintFillLayers(paintInfo, bgColor, bgLayer, view()->backgroundRect(this), BackgroundBleedNone, CompositeSourceOver, bodyObject);
}

BackgroundBleedAvoidance RenderBox::determineBackgroundBleedAvoidance(GraphicsContext* context) const
{
    if (context->paintingDisabled())
        return BackgroundBleedNone;

    const RenderStyle* style = this->style();

    if (!style->hasBackground() || !style->hasBorder() || !style->hasBorderRadius() || borderImageIsLoadedAndCanBeRendered())
        return BackgroundBleedNone;

    AffineTransform ctm = context->getCTM();
    FloatSize contextScaling(static_cast<float>(ctm.xScale()), static_cast<float>(ctm.yScale()));
    if (borderObscuresBackgroundEdge(contextScaling))
        return BackgroundBleedShrinkBackground;
    
    // FIXME: there is one more strategy possible, for opaque backgrounds and
    // translucent borders. In that case we could avoid using a transparency layer,
    // and paint the border first, and then paint the background clipped to the
    // inside of the border.

    return BackgroundBleedUseTransparencyLayer;
}

void RenderBox::paintBoxDecorations(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(this))
        return;

    LayoutRect paintRect = borderBoxRectInRegion(paintInfo.renderRegion);
    paintRect.moveBy(paintOffset);

    // border-fit can adjust where we paint our border and background.  If set, we snugly fit our line box descendants.  (The iChat
    // balloon layout is an example of this).
    borderFitAdjust(paintRect);

    // FIXME: Should eventually give the theme control over whether the box shadow should paint, since controls could have
    // custom shadows of their own.
    paintBoxShadow(paintInfo, paintRect, style(), Normal);

    BackgroundBleedAvoidance bleedAvoidance = determineBackgroundBleedAvoidance(paintInfo.context);

    GraphicsContextStateSaver stateSaver(*paintInfo.context, false);
    if (bleedAvoidance == BackgroundBleedUseTransparencyLayer) {
        // To avoid the background color bleeding out behind the border, we'll render background and border
        // into a transparency layer, and then clip that in one go (which requires setting up the clip before
        // beginning the layer).
        RoundedRect border = style()->getRoundedBorderFor(paintRect);
        stateSaver.save();
        paintInfo.context->addRoundedRectClip(border);
        paintInfo.context->beginTransparencyLayer(1);
    }
    
    // If we have a native theme appearance, paint that before painting our background.
    // The theme will tell us whether or not we should also paint the CSS background.
    bool themePainted = style()->hasAppearance() && !theme()->paint(this, paintInfo, paintRect);
    if (!themePainted) {
        paintBackground(paintInfo, paintRect, bleedAvoidance);

        if (style()->hasAppearance())
            theme()->paintDecorations(this, paintInfo, paintRect);
    }
    paintBoxShadow(paintInfo, paintRect, style(), Inset);

    // The theme will tell us whether or not we should also paint the CSS border.
    if ((!style()->hasAppearance() || (!themePainted && theme()->paintBorderOnly(this, paintInfo, paintRect))) && style()->hasBorder())
        paintBorder(paintInfo, paintRect, style(), bleedAvoidance);

    if (bleedAvoidance == BackgroundBleedUseTransparencyLayer)
        paintInfo.context->endTransparencyLayer();
}

void RenderBox::paintBackground(const PaintInfo& paintInfo, const LayoutRect& paintRect, BackgroundBleedAvoidance bleedAvoidance)
{
    if (isRoot())
        paintRootBoxFillLayers(paintInfo);
    else if (!isBody() || document()->documentElement()->renderer()->hasBackground()) {
        // The <body> only paints its background if the root element has defined a background
        // independent of the body.
        if (!backgroundIsObscured())
            paintFillLayers(paintInfo, style()->visitedDependentColor(CSSPropertyBackgroundColor), style()->backgroundLayers(), paintRect, bleedAvoidance);
    }
}

void RenderBox::paintMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(this) || style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask || paintInfo.context->paintingDisabled())
        return;

    LayoutRect paintRect = LayoutRect(paintOffset, size());

    // border-fit can adjust where we paint our border and background.  If set, we snugly fit our line box descendants.  (The iChat
    // balloon layout is an example of this).
    borderFitAdjust(paintRect);

    paintMaskImages(paintInfo, paintRect);
}

void RenderBox::paintMaskImages(const PaintInfo& paintInfo, const LayoutRect& paintRect)
{
    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    bool compositedMask = hasLayer() && layer()->hasCompositedMask();
    CompositeOperator compositeOp = CompositeSourceOver;

    bool allMaskImagesLoaded = true;
    
    if (!compositedMask) {
        // If the context has a rotation, scale or skew, then use a transparency layer to avoid
        // pixel cruft around the edge of the mask.
        const AffineTransform& currentCTM = paintInfo.context->getCTM();
        pushTransparencyLayer = !currentCTM.isIdentityOrTranslationOrFlipped();

        StyleImage* maskBoxImage = style()->maskBoxImage().image();
        const FillLayer* maskLayers = style()->maskLayers();

        // Don't render a masked element until all the mask images have loaded, to prevent a flash of unmasked content.
        if (maskBoxImage)
            allMaskImagesLoaded &= maskBoxImage->isLoaded();

        if (maskLayers)
            allMaskImagesLoaded &= maskLayers->imagesAreLoaded();

        // Before all images have loaded, just use an empty transparency layer as the mask.
        if (!allMaskImagesLoaded)
            pushTransparencyLayer = true;

        bool hasMaskLayerWithImage = maskLayers->hasImage();
        if (maskBoxImage && hasMaskLayerWithImage) {
            // We have a mask-box-image and mask-image, so need to composite them together before using the result as a mask.
            pushTransparencyLayer = true;
        } else if (hasMaskLayerWithImage) {
            // We have to use an extra image buffer to hold the mask. Multiple mask images need
            // to composite together using source-over so that they can then combine into a single unified mask that
            // can be composited with the content using destination-in.  SVG images need to be able to set compositing modes
            // as they draw images contained inside their sub-document, so we paint all our images into a separate buffer
            // and composite that buffer as the mask.
            // We have to check that the mask images to be rendered contain at least one image that can be actually used in rendering
            // before pushing the transparency layer.
            for (const FillLayer* fillLayer = maskLayers->next(); fillLayer; fillLayer = fillLayer->next()) {
                if (fillLayer->image() && fillLayer->image()->canRender(this, style()->effectiveZoom())) {
                    pushTransparencyLayer = true;
                    // We found one image that can be used in rendering, exit the loop
                    break;
                }
            }
        }
        
        compositeOp = CompositeDestinationIn;
        if (pushTransparencyLayer) {
            paintInfo.context->setCompositeOperation(CompositeDestinationIn);
            paintInfo.context->beginTransparencyLayer(1.0f);
            compositeOp = CompositeSourceOver;
        }
    }

    if (allMaskImagesLoaded) {
        paintFillLayers(paintInfo, Color(), style()->maskLayers(), paintRect, BackgroundBleedNone, compositeOp);
        paintNinePieceImage(paintInfo.context, paintRect, style(), style()->maskBoxImage(), compositeOp);
    }
    
    if (pushTransparencyLayer)
        paintInfo.context->endTransparencyLayer();
}

LayoutRect RenderBox::maskClipRect()
{
    const NinePieceImage& maskBoxImage = style()->maskBoxImage();
    if (maskBoxImage.image()) {
        LayoutRect borderImageRect = borderBoxRect();
        
        // Apply outsets to the border box.
        LayoutUnit topOutset;
        LayoutUnit rightOutset;
        LayoutUnit bottomOutset;
        LayoutUnit leftOutset;
        style()->getMaskBoxImageOutsets(topOutset, rightOutset, bottomOutset, leftOutset);
         
        borderImageRect.setX(borderImageRect.x() - leftOutset);
        borderImageRect.setY(borderImageRect.y() - topOutset);
        borderImageRect.setWidth(borderImageRect.width() + leftOutset + rightOutset);
        borderImageRect.setHeight(borderImageRect.height() + topOutset + bottomOutset);

        return borderImageRect;
    }
    
    LayoutRect result;
    LayoutRect borderBox = borderBoxRect();
    for (const FillLayer* maskLayer = style()->maskLayers(); maskLayer; maskLayer = maskLayer->next()) {
        if (maskLayer->image()) {
            BackgroundImageGeometry geometry;
            calculateBackgroundImageGeometry(maskLayer, borderBox, geometry);
            result.unite(geometry.destRect());
        }
    }
    return result;
}

void RenderBox::paintFillLayers(const PaintInfo& paintInfo, const Color& c, const FillLayer* fillLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, CompositeOperator op, RenderObject* backgroundObject)
{
    if (!fillLayer)
        return;

    paintFillLayers(paintInfo, c, fillLayer->next(), rect, bleedAvoidance, op, backgroundObject);
    paintFillLayer(paintInfo, c, fillLayer, rect, bleedAvoidance, op, backgroundObject);
}

void RenderBox::paintFillLayer(const PaintInfo& paintInfo, const Color& c, const FillLayer* fillLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, CompositeOperator op, RenderObject* backgroundObject)
{
    paintFillLayerExtended(paintInfo, c, fillLayer, rect, bleedAvoidance, 0, IntSize(), op, backgroundObject);
}

#if USE(ACCELERATED_COMPOSITING)
static bool layersUseImage(WrappedImagePtr image, const FillLayer* layers)
{
    for (const FillLayer* curLayer = layers; curLayer; curLayer = curLayer->next()) {
        if (curLayer->image() && image == curLayer->image()->data())
            return true;
    }

    return false;
}
#endif

void RenderBox::imageChanged(WrappedImagePtr image, const IntRect*)
{
    if (!parent())
        return;

    if ((style()->borderImage().image() && style()->borderImage().image()->data() == image) ||
        (style()->maskBoxImage().image() && style()->maskBoxImage().image()->data() == image)) {
        repaint();
        return;
    }

    bool didFullRepaint = repaintLayerRectsForImage(image, style()->backgroundLayers(), true);
    if (!didFullRepaint)
        repaintLayerRectsForImage(image, style()->maskLayers(), false);


#if USE(ACCELERATED_COMPOSITING)
    if (hasLayer() && layer()->hasCompositedMask() && layersUseImage(image, style()->maskLayers()))
        layer()->contentChanged(RenderLayer::MaskImageChanged);
#endif
}

bool RenderBox::repaintLayerRectsForImage(WrappedImagePtr image, const FillLayer* layers, bool drawingBackground)
{
    LayoutRect rendererRect;
    RenderBox* layerRenderer = 0;

    for (const FillLayer* curLayer = layers; curLayer; curLayer = curLayer->next()) {
        if (curLayer->image() && image == curLayer->image()->data() && curLayer->image()->canRender(this, style()->effectiveZoom())) {
            // Now that we know this image is being used, compute the renderer and the rect
            // if we haven't already
            if (!layerRenderer) {
                bool drawingRootBackground = drawingBackground && (isRoot() || (isBody() && !document()->documentElement()->renderer()->hasBackground()));
                if (drawingRootBackground) {
                    layerRenderer = view();

                    LayoutUnit rw;
                    LayoutUnit rh;

                    if (FrameView* frameView = toRenderView(layerRenderer)->frameView()) {
                        rw = frameView->contentsWidth();
                        rh = frameView->contentsHeight();
                    } else {
                        rw = layerRenderer->width();
                        rh = layerRenderer->height();
                    }
                    rendererRect = LayoutRect(-layerRenderer->marginLeft(),
                        -layerRenderer->marginTop(),
                        max(layerRenderer->width() + layerRenderer->marginLeft() + layerRenderer->marginRight() + layerRenderer->borderLeft() + layerRenderer->borderRight(), rw),
                        max(layerRenderer->height() + layerRenderer->marginTop() + layerRenderer->marginBottom() + layerRenderer->borderTop() + layerRenderer->borderBottom(), rh));
                } else {
                    layerRenderer = this;
                    rendererRect = borderBoxRect();
                }
            }

            BackgroundImageGeometry geometry;
            layerRenderer->calculateBackgroundImageGeometry(curLayer, rendererRect, geometry);
            layerRenderer->repaintRectangle(geometry.destRect());
            if (geometry.destRect() == rendererRect)
                return true;
        }
    }
    return false;
}

#if PLATFORM(MAC)

void RenderBox::paintCustomHighlight(const LayoutPoint& paintOffset, const AtomicString& type, bool behindText)
{
    Frame* frame = this->frame();
    if (!frame)
        return;
    Page* page = frame->page();
    if (!page)
        return;

    InlineBox* boxWrap = inlineBoxWrapper();
    RootInlineBox* r = boxWrap ? boxWrap->root() : 0;
    if (r) {
        FloatRect rootRect(paintOffset.x() + r->x(), paintOffset.y() + r->selectionTop(), r->logicalWidth(), r->selectionHeight());
        FloatRect imageRect(paintOffset.x() + x(), rootRect.y(), width(), rootRect.height());
        page->chrome()->client()->paintCustomHighlight(node(), type, imageRect, rootRect, behindText, false);
    } else {
        FloatRect imageRect(paintOffset.x() + x(), paintOffset.y() + y(), width(), height());
        page->chrome()->client()->paintCustomHighlight(node(), type, imageRect, imageRect, behindText, false);
    }
}

#endif

bool RenderBox::pushContentsClip(PaintInfo& paintInfo, const LayoutPoint& accumulatedOffset)
{
    if (paintInfo.phase == PaintPhaseBlockBackground || paintInfo.phase == PaintPhaseSelfOutline || paintInfo.phase == PaintPhaseMask)
        return false;
        
    bool isControlClip = hasControlClip();
    bool isOverflowClip = hasOverflowClip() && !layer()->isSelfPaintingLayer();
    
    if (!isControlClip && !isOverflowClip)
        return false;
    
    if (paintInfo.phase == PaintPhaseOutline)
        paintInfo.phase = PaintPhaseChildOutlines;
    else if (paintInfo.phase == PaintPhaseChildBlockBackground) {
        paintInfo.phase = PaintPhaseBlockBackground;
        paintObject(paintInfo, accumulatedOffset);
        paintInfo.phase = PaintPhaseChildBlockBackgrounds;
    }
    IntRect clipRect(isControlClip ? controlClipRect(accumulatedOffset) : overflowClipRect(accumulatedOffset, paintInfo.renderRegion));
    paintInfo.context->save();
    if (style()->hasBorderRadius())
        paintInfo.context->addRoundedRectClip(style()->getRoundedInnerBorderFor(LayoutRect(accumulatedOffset, size())));
    paintInfo.context->clip(clipRect);
    return true;
}

void RenderBox::popContentsClip(PaintInfo& paintInfo, PaintPhase originalPhase, const LayoutPoint& accumulatedOffset)
{
    ASSERT(hasControlClip() || (hasOverflowClip() && !layer()->isSelfPaintingLayer()));

    paintInfo.context->restore();
    if (originalPhase == PaintPhaseOutline) {
        paintInfo.phase = PaintPhaseSelfOutline;
        paintObject(paintInfo, accumulatedOffset);
        paintInfo.phase = originalPhase;
    } else if (originalPhase == PaintPhaseChildBlockBackground)
        paintInfo.phase = originalPhase;
}

LayoutRect RenderBox::overflowClipRect(const LayoutPoint& location, RenderRegion* region, OverlayScrollbarSizeRelevancy relevancy)
{
    // FIXME: When overflow-clip (CSS3) is implemented, we'll obtain the property
    // here.
    LayoutRect clipRect = borderBoxRectInRegion(region);
    clipRect.setLocation(location + clipRect.location() + LayoutSize(borderLeft(), borderTop()));
    clipRect.setSize(clipRect.size() - LayoutSize(borderLeft() + borderRight(), borderTop() + borderBottom()));

    // Subtract out scrollbars if we have them.
    if (layer())
        clipRect.contract(layer()->verticalScrollbarWidth(relevancy), layer()->horizontalScrollbarHeight(relevancy));

    return clipRect;
}

LayoutRect RenderBox::clipRect(const LayoutPoint& location, RenderRegion* region)
{
    LayoutRect borderBoxRect = borderBoxRectInRegion(region);
    LayoutRect clipRect = LayoutRect(borderBoxRect.location() + location, borderBoxRect.size());

    if (!style()->clipLeft().isAuto()) {
        LayoutUnit c = style()->clipLeft().calcValue(borderBoxRect.width());
        clipRect.move(c, 0);
        clipRect.contract(c, 0);
    }

    // We don't use the region-specific border box's width and height since clip offsets are (stupidly) specified
    // from the left and top edges. Therefore it's better to avoid constraining to smaller widths and heights.

    if (!style()->clipRight().isAuto())
        clipRect.contract(width() - style()->clipRight().calcValue(width()), 0);

    if (!style()->clipTop().isAuto()) {
        LayoutUnit c = style()->clipTop().calcValue(borderBoxRect.height());
        clipRect.move(0, c);
        clipRect.contract(0, c);
    }

    if (!style()->clipBottom().isAuto())
        clipRect.contract(0, height() - style()->clipBottom().calcValue(height()));

    return clipRect;
}

LayoutUnit RenderBox::containingBlockLogicalWidthForContent() const
{
    RenderBlock* cb = containingBlock();
    if (shrinkToAvoidFloats() && !inRenderFlowThread())
        return cb->availableLogicalWidthForLine(logicalTop(), false);
    return cb->availableLogicalWidth();
}

LayoutUnit RenderBox::containingBlockLogicalWidthForContentInRegion(RenderRegion* region, LayoutUnit offsetFromLogicalTopOfFirstPage) const
{
    if (!region)
        return containingBlockLogicalWidthForContent();

    RenderBlock* cb = containingBlock();
    RenderRegion* containingBlockRegion = cb->clampToStartAndEndRegions(region);
    if (shrinkToAvoidFloats()) {
        LayoutUnit offsetFromLogicalTopOfRegion = region->offsetFromLogicalTopOfFirstPage() - offsetFromLogicalTopOfFirstPage;
        return cb->availableLogicalWidthForLine(max(logicalTop(), logicalTop() + offsetFromLogicalTopOfRegion), false, containingBlockRegion, offsetFromLogicalTopOfFirstPage - logicalTop());
    }
    LayoutUnit result = cb->availableLogicalWidth();
    RenderBoxRegionInfo* boxInfo = cb->renderBoxRegionInfo(containingBlockRegion, offsetFromLogicalTopOfFirstPage - logicalTop());
    if (!boxInfo)
        return result;
    return max<LayoutUnit>(0, result - (cb->logicalWidth() - boxInfo->logicalWidth()));
}

LayoutUnit RenderBox::perpendicularContainingBlockLogicalHeight() const
{
    RenderBlock* cb = containingBlock();
    RenderStyle* containingBlockStyle = cb->style();
    Length logicalHeightLength = containingBlockStyle->logicalHeight();
    
    // FIXME: For now just support fixed heights.  Eventually should support percentage heights as well.
    if (!logicalHeightLength.isFixed()) {
        // Rather than making the child be completely unconstrained, WinIE uses the viewport width and height
        // as a constraint.  We do that for now as well even though it's likely being unconstrained is what the spec
        // will decide.
        return containingBlockStyle->isHorizontalWritingMode() ? view()->frameView()->visibleHeight() : view()->frameView()->visibleWidth();
    }
    
    // Use the content box logical height as specified by the style.
    return cb->computeContentBoxLogicalHeight(logicalHeightLength.value());
}

void RenderBox::mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool fixed, bool useTransforms, TransformState& transformState, bool* wasFixed) const
{
    if (repaintContainer == this)
        return;

    if (RenderView* v = view()) {
        if (v->layoutStateEnabled() && !repaintContainer) {
            LayoutState* layoutState = v->layoutState();
            LayoutSize offset = layoutState->m_paintOffset;
            offset.expand(x(), y());
            if (style()->position() == RelativePosition && layer())
                offset += layer()->relativePositionOffset();
            transformState.move(offset);
            return;
        }
    }

    bool containerSkipped;
    RenderObject* o = container(repaintContainer, &containerSkipped);
    if (!o)
        return;

    bool isFixedPos = style()->position() == FixedPosition;
    bool hasTransform = hasLayer() && layer()->transform();
    if (hasTransform) {
        // If this box has a transform, it acts as a fixed position container for fixed descendants,
        // and may itself also be fixed position. So propagate 'fixed' up only if this box is fixed position.
        fixed &= isFixedPos;
    } else
        fixed |= isFixedPos;
    if (wasFixed)
        *wasFixed = fixed;
    
    LayoutSize containerOffset = offsetFromContainer(o, roundedLayoutPoint(transformState.mappedPoint()));
    
    bool preserve3D = useTransforms && (o->style()->preserves3D() || style()->preserves3D());
    if (useTransforms && shouldUseTransformFromContainer(o)) {
        TransformationMatrix t;
        getTransformFromContainer(o, containerOffset, t);
        transformState.applyTransform(t, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    } else
        transformState.move(containerOffset.width(), containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);

    if (containerSkipped) {
        // There can't be a transform between repaintContainer and o, because transforms create containers, so it should be safe
        // to just subtract the delta between the repaintContainer and o.
        LayoutSize containerOffset = repaintContainer->offsetFromAncestorContainer(o);
        transformState.move(-containerOffset.width(), -containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
        return;
    }

    if (o->isRenderFlowThread()) {
        // Transform from render flow coordinates into region coordinates.
        RenderRegion* region = toRenderFlowThread(o)->mapFromFlowToRegion(transformState);
        if (region)
            region->mapLocalToContainer(region->containerForRepaint(), fixed, useTransforms, transformState, wasFixed);
        return;
    }

    o->mapLocalToContainer(repaintContainer, fixed, useTransforms, transformState, wasFixed);
}

void RenderBox::mapAbsoluteToLocalPoint(bool fixed, bool useTransforms, TransformState& transformState) const
{
    // We don't expect absoluteToLocal() to be called during layout (yet)
    ASSERT(!view() || !view()->layoutStateEnabled());
    
    bool isFixedPos = style()->position() == FixedPosition;
    bool hasTransform = hasLayer() && layer()->transform();
    if (hasTransform) {
        // If this box has a transform, it acts as a fixed position container for fixed descendants,
        // and may itself also be fixed position. So propagate 'fixed' up only if this box is fixed position.
        fixed &= isFixedPos;
    } else
        fixed |= isFixedPos;

    RenderBoxModelObject::mapAbsoluteToLocalPoint(fixed, useTransforms, transformState);
}

LayoutSize RenderBox::offsetFromContainer(RenderObject* o, const LayoutPoint& point) const
{
    ASSERT(o == container());

    LayoutSize offset;    
    if (isRelPositioned())
        offset += relativePositionOffset();

    if (!isInline() || isReplaced()) {
        if (style()->position() != AbsolutePosition && style()->position() != FixedPosition && o->hasColumns()) {
            RenderBlock* block = toRenderBlock(o);
            LayoutRect columnRect(frameRect());
            block->adjustStartEdgeForWritingModeIncludingColumns(columnRect);
            offset += toSize(columnRect.location());
            LayoutPoint columnPoint = block->flipForWritingModeIncludingColumns(point + offset);
            offset = toLayoutSize(block->flipForWritingModeIncludingColumns(toLayoutPoint(offset)));
            o->adjustForColumns(offset, columnPoint);
            offset = block->flipForWritingMode(offset);
        } else
            offset += topLeftLocationOffset();
    }

    if (o->hasOverflowClip())
        offset -= toRenderBox(o)->layer()->scrolledContentOffset();

    if (style()->position() == AbsolutePosition && o->isRelPositioned() && o->isRenderInline())
        offset += toRenderInline(o)->relativePositionedInlineOffset(this);

    return offset;
}

InlineBox* RenderBox::createInlineBox()
{
    return new (renderArena()) InlineBox(this);
}

void RenderBox::dirtyLineBoxes(bool fullLayout)
{
    if (m_inlineBoxWrapper) {
        if (fullLayout) {
            m_inlineBoxWrapper->destroy(renderArena());
            m_inlineBoxWrapper = 0;
        } else
            m_inlineBoxWrapper->dirtyLineBoxes();
    }
}

void RenderBox::positionLineBox(InlineBox* box)
{
    if (isPositioned()) {
        // Cache the x position only if we were an INLINE type originally.
        bool wasInline = style()->isOriginalDisplayInlineType();
        if (wasInline) {
            // The value is cached in the xPos of the box.  We only need this value if
            // our object was inline originally, since otherwise it would have ended up underneath
            // the inlines.
            RootInlineBox* root = box->root();
            root->block()->setStaticInlinePositionForChild(this, root->lineTopWithLeading(), lroundf(box->logicalLeft()));
            if (style()->hasStaticInlinePosition(box->isHorizontal()))
                setChildNeedsLayout(true, false); // Just go ahead and mark the positioned object as needing layout, so it will update its position properly.
        } else {
            // Our object was a block originally, so we make our normal flow position be
            // just below the line box (as though all the inlines that came before us got
            // wrapped in an anonymous block, which is what would have happened had we been
            // in flow).  This value was cached in the y() of the box.
            layer()->setStaticBlockPosition(box->logicalTop());
            if (style()->hasStaticBlockPosition(box->isHorizontal()))
                setChildNeedsLayout(true, false); // Just go ahead and mark the positioned object as needing layout, so it will update its position properly.
        }

        // Nuke the box.
        box->remove();
        box->destroy(renderArena());
    } else if (isReplaced()) {
        setLocation(roundedLayoutPoint(box->topLeft()));
        ASSERT(!m_inlineBoxWrapper);
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

LayoutRect RenderBox::clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer) const
{
    if (style()->visibility() != VISIBLE && !enclosingLayer()->hasVisibleContent())
        return LayoutRect();

    LayoutRect r = visualOverflowRect();

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

void RenderBox::computeRectForRepaint(RenderBoxModelObject* repaintContainer, LayoutRect& rect, bool fixed) const
{
    // The rect we compute at each step is shifted by our x/y offset in the parent container's coordinate space.
    // Only when we cross a writing mode boundary will we have to possibly flipForWritingMode (to convert into a more appropriate
    // offset corner for the enclosing container).  This allows for a fully RL or BT document to repaint
    // properly even during layout, since the rect remains flipped all the way until the end.
    //
    // RenderView::computeRectForRepaint then converts the rect to physical coordinates.  We also convert to
    // physical when we hit a repaintContainer boundary.  Therefore the final rect returned is always in the
    // physical coordinate space of the repaintContainer.
    if (RenderView* v = view()) {
        // LayoutState is only valid for root-relative, non-fixed position repainting
        if (v->layoutStateEnabled() && !repaintContainer && style()->position() != FixedPosition) {
            LayoutState* layoutState = v->layoutState();

            if (layer() && layer()->transform())
                rect = layer()->transform()->mapRect(rect);

            if (style()->position() == RelativePosition && layer())
                rect.move(layer()->relativePositionOffset());

            rect.moveBy(location());
            rect.move(layoutState->m_paintOffset);
            if (layoutState->m_clipped)
                rect.intersect(layoutState->m_clipRect);
            return;
        }
    }

    if (hasReflection())
        rect.unite(reflectedRect(rect));

    if (repaintContainer == this) {
        if (repaintContainer->style()->isFlippedBlocksWritingMode())
            flipForWritingMode(rect);
        return;
    }

    bool containerSkipped;
    RenderObject* o = container(repaintContainer, &containerSkipped);
    if (!o)
        return;

    if (isWritingModeRoot() && !isPositioned())
        flipForWritingMode(rect);
    LayoutPoint topLeft = rect.location();
    topLeft.move(x(), y());

    EPosition position = style()->position();

    // We are now in our parent container's coordinate space.  Apply our transform to obtain a bounding box
    // in the parent's coordinate space that encloses us.
    if (layer() && layer()->transform()) {
        fixed = position == FixedPosition;
        rect = layer()->transform()->mapRect(rect);
        topLeft = rect.location();
        topLeft.move(x(), y());
    } else if (position == FixedPosition)
        fixed = true;

    if (position == AbsolutePosition && o->isRelPositioned() && o->isRenderInline())
        topLeft += toRenderInline(o)->relativePositionedInlineOffset(this);
    else if (position == RelativePosition && layer()) {
        // Apply the relative position offset when invalidating a rectangle.  The layer
        // is translated, but the render box isn't, so we need to do this to get the
        // right dirty rect.  Since this is called from RenderObject::setStyle, the relative position
        // flag on the RenderObject has been cleared, so use the one on the style().
        topLeft += layer()->relativePositionOffset();
    }
    
    if (o->isBlockFlow() && position != AbsolutePosition && position != FixedPosition) {
        RenderBlock* cb = toRenderBlock(o);
        if (cb->hasColumns()) {
            LayoutRect repaintRect(topLeft, rect.size());
            cb->adjustRectForColumns(repaintRect);
            topLeft = repaintRect.location();
            rect = repaintRect;
        }
    }

    // FIXME: We ignore the lightweight clipping rect that controls use, since if |o| is in mid-layout,
    // its controlClipRect will be wrong. For overflow clip we use the values cached by the layer.
    if (o->hasOverflowClip()) {
        RenderBox* containerBox = toRenderBox(o);

        // o->height() is inaccurate if we're in the middle of a layout of |o|, so use the
        // layer's size instead.  Even if the layer's size is wrong, the layer itself will repaint
        // anyway if its size does change.
        topLeft -= containerBox->layer()->scrolledContentOffset(); // For overflow:auto/scroll/hidden.

        LayoutRect repaintRect(topLeft, rect.size());
        LayoutRect boxRect(LayoutPoint(), containerBox->layer()->size());
        rect = intersection(repaintRect, boxRect);
        if (rect.isEmpty())
            return;
    } else
        rect.setLocation(topLeft);

    if (containerSkipped) {
        // If the repaintContainer is below o, then we need to map the rect into repaintContainer's coordinates.
        LayoutSize containerOffset = repaintContainer->offsetFromAncestorContainer(o);
        rect.move(-containerOffset);
        return;
    }

    o->computeRectForRepaint(repaintContainer, rect, fixed);
}

void RenderBox::repaintDuringLayoutIfMoved(const LayoutRect& rect)
{
    LayoutUnit newX = x();
    LayoutUnit newY = y();
    LayoutUnit newWidth = width();
    LayoutUnit newHeight = height();
    if (rect.x() != newX || rect.y() != newY) {
        // The child moved.  Invalidate the object's old and new positions.  We have to do this
        // since the object may not have gotten a layout.
        m_frameRect = rect;
        repaint();
        repaintOverhangingFloats(true);
        m_frameRect = LayoutRect(newX, newY, newWidth, newHeight);
        repaint();
        repaintOverhangingFloats(true);
    }
}

void RenderBox::computeLogicalWidth()
{
    computeLogicalWidthInRegion();
}

void RenderBox::computeLogicalWidthInRegion(RenderRegion* region, LayoutUnit offsetFromLogicalTopOfFirstPage)
{
    if (isPositioned()) {
        // FIXME: This calculation is not patched for block-flow yet.
        // https://bugs.webkit.org/show_bug.cgi?id=46500
        computePositionedLogicalWidth(region, offsetFromLogicalTopOfFirstPage);
        return;
    }

    // If layout is limited to a subtree, the subtree root's logical width does not change.
    if (node() && view()->frameView() && view()->frameView()->layoutRoot(true) == this)
        return;

    // The parent box is flexing us, so it has increased or decreased our
    // width.  Use the width from the style context.
    // FIXME: Account for block-flow in flexible boxes.
    // https://bugs.webkit.org/show_bug.cgi?id=46418
    if (hasOverrideWidth() && parent()->isFlexibleBoxIncludingDeprecated()) {
        setLogicalWidth(overrideWidth());
        return;
    }

    // FIXME: Account for block-flow in flexible boxes.
    // https://bugs.webkit.org/show_bug.cgi?id=46418
    bool inVerticalBox = parent()->isDeprecatedFlexibleBox() && (parent()->style()->boxOrient() == VERTICAL);
    bool stretching = (parent()->style()->boxAlign() == BSTRETCH);
    bool treatAsReplaced = shouldComputeSizeAsReplaced() && (!inVerticalBox || !stretching);

    Length logicalWidthLength = (treatAsReplaced) ? Length(computeReplacedLogicalWidth(), Fixed) : style()->logicalWidth();

    RenderBlock* cb = containingBlock();
    LayoutUnit containerLogicalWidth = max<LayoutUnit>(0, containingBlockLogicalWidthForContentInRegion(region, offsetFromLogicalTopOfFirstPage));
    bool hasPerpendicularContainingBlock = cb->isHorizontalWritingMode() != isHorizontalWritingMode();
    LayoutUnit containerWidthInInlineDirection = containerLogicalWidth;
    if (hasPerpendicularContainingBlock)
        containerWidthInInlineDirection = perpendicularContainingBlockLogicalHeight();
    
    if (isInline() && !isInlineBlockOrInlineTable()) {
        // just calculate margins
        setMarginStart(style()->marginStart().calcMinValue(containerLogicalWidth));
        setMarginEnd(style()->marginEnd().calcMinValue(containerLogicalWidth));
        if (treatAsReplaced)
            setLogicalWidth(max<LayoutUnit>(logicalWidthLength.calcFloatValue(0) + borderAndPaddingLogicalWidth(), minPreferredLogicalWidth()));
        return;
    }

    // Width calculations
    if (treatAsReplaced)
        setLogicalWidth(logicalWidthLength.value() + borderAndPaddingLogicalWidth());
    else {
        // Calculate LogicalWidth
        setLogicalWidth(computeLogicalWidthUsing(LogicalWidth, containerWidthInInlineDirection));

        // Calculate MaxLogicalWidth
        if (!style()->logicalMaxWidth().isUndefined()) {
            LayoutUnit maxLogicalWidth = computeLogicalWidthUsing(MaxLogicalWidth, containerWidthInInlineDirection);
            if (logicalWidth() > maxLogicalWidth) {
                setLogicalWidth(maxLogicalWidth);
                logicalWidthLength = style()->logicalMaxWidth();
            }
        }

        // Calculate MinLogicalWidth
        LayoutUnit minLogicalWidth = computeLogicalWidthUsing(MinLogicalWidth, containerWidthInInlineDirection);
        if (logicalWidth() < minLogicalWidth) {
            setLogicalWidth(minLogicalWidth);
            logicalWidthLength = style()->logicalMinWidth();
        }
    }

    // Fieldsets are currently the only objects that stretch to their minimum width.
    if (stretchesToMinIntrinsicLogicalWidth()) {
        setLogicalWidth(max(logicalWidth(), minPreferredLogicalWidth()));
        logicalWidthLength = Length(logicalWidth(), Fixed);
    }

    // Margin calculations.
    if (logicalWidthLength.isAuto() || hasPerpendicularContainingBlock) {
        setMarginStart(style()->marginStart().calcMinValue(containerLogicalWidth));
        setMarginEnd(style()->marginEnd().calcMinValue(containerLogicalWidth));
    } else
        computeInlineDirectionMargins(cb, containerLogicalWidth, logicalWidth());

    if (!hasPerpendicularContainingBlock && containerLogicalWidth && containerLogicalWidth != (logicalWidth() + marginStart() + marginEnd())
            && !isFloating() && !isInline() && !cb->isFlexibleBoxIncludingDeprecated())
        cb->setMarginEndForChild(this, containerLogicalWidth - logicalWidth() - cb->marginStartForChild(this));
}

LayoutUnit RenderBox::computeLogicalWidthUsing(LogicalWidthType widthType, LayoutUnit availableLogicalWidth)
{
    LayoutUnit logicalWidthResult = logicalWidth();
    Length logicalWidth;
    if (widthType == LogicalWidth)
        logicalWidth = style()->logicalWidth();
    else if (widthType == MinLogicalWidth)
        logicalWidth = style()->logicalMinWidth();
    else
        logicalWidth = style()->logicalMaxWidth();

    if (logicalWidth.isIntrinsicOrAuto()) {
        LayoutUnit marginStart = style()->marginStart().calcMinValue(availableLogicalWidth);
        LayoutUnit marginEnd = style()->marginEnd().calcMinValue(availableLogicalWidth);
        logicalWidthResult = availableLogicalWidth - marginStart - marginEnd;

        if (sizesToIntrinsicLogicalWidth(widthType)) {
            logicalWidthResult = max(logicalWidthResult, minPreferredLogicalWidth());
            logicalWidthResult = min(logicalWidthResult, maxPreferredLogicalWidth());
        }
    } else // FIXME: If the containing block flow is perpendicular to our direction we need to use the available logical height instead.
        logicalWidthResult = computeBorderBoxLogicalWidth(logicalWidth.calcValue(availableLogicalWidth)); 

    return logicalWidthResult;
}

bool RenderBox::sizesToIntrinsicLogicalWidth(LogicalWidthType widthType) const
{
    // Marquees in WinIE are like a mixture of blocks and inline-blocks.  They size as though they're blocks,
    // but they allow text to sit on the same line as the marquee.
    if (isFloating() || (isInlineBlockOrInlineTable() && !isHTMLMarquee()))
        return true;

    // This code may look a bit strange.  Basically width:intrinsic should clamp the size when testing both
    // min-width and width.  max-width is only clamped if it is also intrinsic.
    Length logicalWidth = (widthType == MaxLogicalWidth) ? style()->logicalMaxWidth() : style()->logicalWidth();
    if (logicalWidth.type() == Intrinsic)
        return true;

    // Children of a horizontal marquee do not fill the container by default.
    // FIXME: Need to deal with MAUTO value properly.  It could be vertical.
    // FIXME: Think about block-flow here.  Need to find out how marquee direction relates to
    // block-flow (as well as how marquee overflow should relate to block flow).
    // https://bugs.webkit.org/show_bug.cgi?id=46472
    if (parent()->style()->overflowX() == OMARQUEE) {
        EMarqueeDirection dir = parent()->style()->marqueeDirection();
        if (dir == MAUTO || dir == MFORWARD || dir == MBACKWARD || dir == MLEFT || dir == MRIGHT)
            return true;
    }

    // Flexible box items should shrink wrap, so we lay them out at their intrinsic widths.
    // In the case of columns that have a stretch alignment, we go ahead and layout at the
    // stretched size to avoid an extra layout when applying alignment.
    if (parent()->isFlexibleBox() && (!parent()->style()->isColumnFlexFlow() || style()->flexAlign() != AlignStretch))
        return true;

    // Flexible horizontal boxes lay out children at their intrinsic widths.  Also vertical boxes
    // that don't stretch their kids lay out their children at their intrinsic widths.
    // FIXME: Think about block-flow here.
    // https://bugs.webkit.org/show_bug.cgi?id=46473
    if (parent()->isDeprecatedFlexibleBox()
            && (parent()->style()->boxOrient() == HORIZONTAL || parent()->style()->boxAlign() != BSTRETCH))
        return true;

    // Button, input, select, textarea, and legend treat
    // width value of 'auto' as 'intrinsic' unless it's in a
    // stretching vertical flexbox.
    // FIXME: Think about block-flow here.
    // https://bugs.webkit.org/show_bug.cgi?id=46473
    if (logicalWidth.type() == Auto && !(parent()->isDeprecatedFlexibleBox() && parent()->style()->boxOrient() == VERTICAL && parent()->style()->boxAlign() == BSTRETCH) && node() && (node()->hasTagName(inputTag) || node()->hasTagName(selectTag) || node()->hasTagName(buttonTag) || node()->hasTagName(textareaTag) || node()->hasTagName(legendTag)))
        return true;

    if (isHorizontalWritingMode() != containingBlock()->isHorizontalWritingMode())
        return true;

    return false;
}

void RenderBox::computeInlineDirectionMargins(RenderBlock* containingBlock, LayoutUnit containerWidth, LayoutUnit childWidth)
{
    const RenderStyle* containingBlockStyle = containingBlock->style();
    Length marginStartLength = style()->marginStartUsing(containingBlockStyle);
    Length marginEndLength = style()->marginEndUsing(containingBlockStyle);

    if (isFloating() || isInline()) {
        // Inline blocks/tables and floats don't have their margins increased.
        containingBlock->setMarginStartForChild(this, marginStartLength.calcMinValue(containerWidth));
        containingBlock->setMarginEndForChild(this, marginEndLength.calcMinValue(containerWidth));
        return;
    }

    // Case One: The object is being centered in the containing block's available logical width.
    if ((marginStartLength.isAuto() && marginEndLength.isAuto() && childWidth < containerWidth)
        || (!marginStartLength.isAuto() && !marginEndLength.isAuto() && containingBlock->style()->textAlign() == WEBKIT_CENTER)) {
        containingBlock->setMarginStartForChild(this, max<LayoutUnit>(0, (containerWidth - childWidth) / 2));
        containingBlock->setMarginEndForChild(this, containerWidth - childWidth - containingBlock->marginStartForChild(this));
        return;
    } 
    
    // Case Two: The object is being pushed to the start of the containing block's available logical width.
    if (marginEndLength.isAuto() && childWidth < containerWidth) {
        containingBlock->setMarginStartForChild(this, marginStartLength.calcValue(containerWidth));
        containingBlock->setMarginEndForChild(this, containerWidth - childWidth - containingBlock->marginStartForChild(this));
        return;
    } 
    
    // Case Three: The object is being pushed to the end of the containing block's available logical width.
    bool pushToEndFromTextAlign = !marginEndLength.isAuto() && ((!containingBlockStyle->isLeftToRightDirection() && containingBlockStyle->textAlign() == WEBKIT_LEFT)
        || (containingBlockStyle->isLeftToRightDirection() && containingBlockStyle->textAlign() == WEBKIT_RIGHT));
    if ((marginStartLength.isAuto() && childWidth < containerWidth) || pushToEndFromTextAlign) {
        containingBlock->setMarginEndForChild(this, marginEndLength.calcValue(containerWidth));
        containingBlock->setMarginStartForChild(this, containerWidth - childWidth - containingBlock->marginEndForChild(this));
        return;
    } 
    
    // Case Four: Either no auto margins, or our width is >= the container width (css2.1, 10.3.3).  In that case
    // auto margins will just turn into 0.
    containingBlock->setMarginStartForChild(this, marginStartLength.calcMinValue(containerWidth));
    containingBlock->setMarginEndForChild(this, marginEndLength.calcMinValue(containerWidth));
}

RenderBoxRegionInfo* RenderBox::renderBoxRegionInfo(RenderRegion* region, LayoutUnit offsetFromLogicalTopOfFirstPage, RenderBoxRegionInfoFlags cacheFlag) const
{
    // Make sure nobody is trying to call this with a null region.
    if (!region)
        return 0;

    // If we have computed our width in this region already, it will be cached, and we can
    // just return it.
    RenderBoxRegionInfo* boxInfo = region->renderBoxRegionInfo(this);
    if (boxInfo && cacheFlag == CacheRenderBoxRegionInfo)
        return boxInfo;

    // No cached value was found, so we have to compute our insets in this region.
    // FIXME: For now we limit this computation to normal RenderBlocks. Future patches will expand
    // support to cover all boxes.
    if (!inRenderFlowThread() || isFloating() || isReplaced() || isInline() || hasColumns()
        || isTableCell() || !isBlockFlow() || isRenderFlowThread())
        return 0;

    // FIXME: It's gross to cast away the const, but it would be a huge refactoring to
    // change all width computation to avoid updating any member variables, and it would be pretty lame to
    // make all the variables mutable as well.
    RenderFlowThread* flowThread = enclosingRenderFlowThread();
    if (flowThread->style()->writingMode() != style()->writingMode())
        return 0;

    LayoutUnit oldLogicalWidth = logicalWidth();
    LayoutUnit oldLogicalLeft = logicalLeft();
    LayoutUnit oldMarginStart = marginStart();
    LayoutUnit oldMarginEnd = marginEnd();

    RenderBox* mutableBox = const_cast<RenderBox*>(this);
    
    mutableBox->computeLogicalWidthInRegion(region, offsetFromLogicalTopOfFirstPage);

    // Now determine the insets based off where this object is supposed to be positioned.
    RenderBlock* cb = containingBlock();
    RenderRegion* clampedContainingBlockRegion = cb->clampToStartAndEndRegions(region);
    RenderBoxRegionInfo* containingBlockInfo = cb->renderBoxRegionInfo(clampedContainingBlockRegion,
        offsetFromLogicalTopOfFirstPage - logicalTop());
    LayoutUnit containingBlockLogicalWidth = cb->logicalWidth();
    LayoutUnit containingBlockLogicalWidthInRegion = containingBlockInfo ? containingBlockInfo->logicalWidth() : containingBlockLogicalWidth;
    
    LayoutUnit marginStartInRegion = marginStart();
    LayoutUnit startMarginDelta = marginStartInRegion - oldMarginStart;
    LayoutUnit logicalWidthInRegion = logicalWidth();
    LayoutUnit logicalLeftInRegion = logicalLeft();
    LayoutUnit widthDelta = logicalWidthInRegion - oldLogicalWidth;
    LayoutUnit logicalLeftDelta = isPositioned() ? logicalLeftInRegion - oldLogicalLeft : startMarginDelta;
    LayoutUnit logicalRightInRegion = containingBlockLogicalWidthInRegion - (logicalLeftInRegion + logicalWidthInRegion);
    LayoutUnit oldLogicalRight = containingBlockLogicalWidth - (oldLogicalLeft + oldLogicalWidth);
    LayoutUnit logicalRightDelta = isPositioned() ? logicalRightInRegion - oldLogicalRight : startMarginDelta;

    // Set our values back.
    mutableBox->setLogicalWidth(oldLogicalWidth);
    mutableBox->setLogicalLeft(oldLogicalLeft);
    mutableBox->setMarginStart(oldMarginStart);
    mutableBox->setMarginEnd(oldMarginEnd);

    LayoutUnit logicalLeftOffset = 0;
    
    if (!isPositioned() && avoidsFloats() && cb->containsFloats()) {
        LayoutUnit startPositionDelta = cb->computeStartPositionDeltaForChildAvoidingFloats(this, marginStartInRegion, logicalWidthInRegion,
            region, offsetFromLogicalTopOfFirstPage);
        if (cb->style()->isLeftToRightDirection())
            logicalLeftDelta += startPositionDelta;
        else
            logicalRightDelta += startPositionDelta;
    }

    if (cb->style()->isLeftToRightDirection())
        logicalLeftOffset += logicalLeftDelta;
    else
        logicalLeftOffset -= (widthDelta + logicalRightDelta);
    
    LayoutUnit logicalRightOffset = logicalWidth() - (logicalLeftOffset + logicalWidthInRegion);
    bool isShifted = (containingBlockInfo && containingBlockInfo->isShifted())
            || (style()->isLeftToRightDirection() && logicalLeftOffset)
            || (!style()->isLeftToRightDirection() && logicalRightOffset);

    // FIXME: Although it's unlikely, these boxes can go outside our bounds, and so we will need to incorporate them into overflow.
    if (cacheFlag == CacheRenderBoxRegionInfo)
        return region->setRenderBoxRegionInfo(this, logicalLeftOffset, logicalWidthInRegion, isShifted);
    return new RenderBoxRegionInfo(logicalLeftOffset, logicalWidthInRegion, isShifted);
}

void RenderBox::computeLogicalHeight()
{
    // Cell height is managed by the table and inline non-replaced elements do not support a height property.
    if (isTableCell() || (isInline() && !isReplaced()))
        return;

    Length h;
    if (isPositioned())
        computePositionedLogicalHeight();
    else {
        RenderBlock* cb = containingBlock();
        bool hasPerpendicularContainingBlock = cb->isHorizontalWritingMode() != isHorizontalWritingMode();
    
        if (!hasPerpendicularContainingBlock)
            computeBlockDirectionMargins(cb);

        // For tables, calculate margins only.
        if (isTable()) {
            if (hasPerpendicularContainingBlock)
                computeInlineDirectionMargins(cb, containingBlockLogicalWidthForContent(), logicalHeight());
            return;
        }

        // FIXME: Account for block-flow in flexible boxes.
        // https://bugs.webkit.org/show_bug.cgi?id=46418
        bool inHorizontalBox = parent()->isDeprecatedFlexibleBox() && parent()->style()->boxOrient() == HORIZONTAL;
        bool stretching = parent()->style()->boxAlign() == BSTRETCH;
        bool treatAsReplaced = shouldComputeSizeAsReplaced() && (!inHorizontalBox || !stretching);
        bool checkMinMaxHeight = false;

        // The parent box is flexing us, so it has increased or decreased our height.  We have to
        // grab our cached flexible height.
        // FIXME: Account for block-flow in flexible boxes.
        // https://bugs.webkit.org/show_bug.cgi?id=46418
        if (hasOverrideHeight() && parent()->isFlexibleBoxIncludingDeprecated())
            h = Length(overrideHeight() - borderAndPaddingLogicalHeight(), Fixed);
        else if (treatAsReplaced)
            h = Length(computeReplacedLogicalHeight(), Fixed);
        else {
            h = style()->logicalHeight();
            checkMinMaxHeight = true;
        }

        // Block children of horizontal flexible boxes fill the height of the box.
        // FIXME: Account for block-flow in flexible boxes.
        // https://bugs.webkit.org/show_bug.cgi?id=46418
        if (h.isAuto() && parent()->isDeprecatedFlexibleBox() && parent()->style()->boxOrient() == HORIZONTAL
                && parent()->isStretchingChildren()) {
            h = Length(parentBox()->contentLogicalHeight() - marginBefore() - marginAfter() - borderAndPaddingLogicalHeight(), Fixed);
            checkMinMaxHeight = false;
        }

        LayoutUnit heightResult;
        if (checkMinMaxHeight) {
            heightResult = computeLogicalHeightUsing(style()->logicalHeight());
            // FIXME: Use < 0 or roughlyEquals when we move to float, see https://bugs.webkit.org/show_bug.cgi?id=66148
            if (heightResult == -1)
                heightResult = logicalHeight();
            LayoutUnit minH = computeLogicalHeightUsing(style()->logicalMinHeight()); // Leave as -1 if unset.
            LayoutUnit maxH = style()->logicalMaxHeight().isUndefined() ? heightResult : computeLogicalHeightUsing(style()->logicalMaxHeight());
            if (maxH == -1)
                maxH = heightResult;
            heightResult = min(maxH, heightResult);
            heightResult = max(minH, heightResult);
        } else {
            // The only times we don't check min/max height are when a fixed length has
            // been given as an override.  Just use that.  The value has already been adjusted
            // for box-sizing.
            heightResult = h.value() + borderAndPaddingLogicalHeight();
        }

        setLogicalHeight(heightResult);
        
        if (hasPerpendicularContainingBlock)
            computeInlineDirectionMargins(cb, containingBlockLogicalWidthForContent(), heightResult);
    }

    // WinIE quirk: The <html> block always fills the entire canvas in quirks mode.  The <body> always fills the
    // <html> block in quirks mode.  Only apply this quirk if the block is normal flow and no height
    // is specified. When we're printing, we also need this quirk if the body or root has a percentage 
    // height since we don't set a height in RenderView when we're printing. So without this quirk, the 
    // height has nothing to be a percentage of, and it ends up being 0. That is bad.
    bool paginatedContentNeedsBaseHeight = document()->printing() && h.isPercent()
        && (isRoot() || (isBody() && document()->documentElement()->renderer()->style()->logicalHeight().isPercent()));
    if (stretchesToViewport() || paginatedContentNeedsBaseHeight) {
        // FIXME: Finish accounting for block flow here.
        // https://bugs.webkit.org/show_bug.cgi?id=46603
        LayoutUnit margins = collapsedMarginBefore() + collapsedMarginAfter();
        LayoutUnit visHeight;
        if (document()->printing())
            visHeight = static_cast<LayoutUnit>(view()->pageLogicalHeight());
        else  {
            if (isHorizontalWritingMode())
                visHeight = view()->viewHeight();
            else
                visHeight = view()->viewWidth();
        }
        if (isRoot())
            setLogicalHeight(max(logicalHeight(), visHeight - margins));
        else {
            LayoutUnit marginsBordersPadding = margins + parentBox()->marginBefore() + parentBox()->marginAfter() + parentBox()->borderAndPaddingLogicalHeight();
            setLogicalHeight(max(logicalHeight(), visHeight - marginsBordersPadding));
        }
    }
}

LayoutUnit RenderBox::computeLogicalHeightUsing(const Length& h)
{
    LayoutUnit logicalHeight = -1;
    if (!h.isAuto()) {
        if (h.isFixed())
            logicalHeight = h.value();
        else if (h.isPercent())
            logicalHeight = computePercentageLogicalHeight(h);
        // FIXME: Use < 0 or roughlyEquals when we move to float, see https://bugs.webkit.org/show_bug.cgi?id=66148
        if (logicalHeight != -1) {
            logicalHeight = computeBorderBoxLogicalHeight(logicalHeight);
            return logicalHeight;
        }
    }
    return logicalHeight;
}

LayoutUnit RenderBox::computePercentageLogicalHeight(const Length& height)
{
    LayoutUnit result = -1;
    
    // In quirks mode, blocks with auto height are skipped, and we keep looking for an enclosing
    // block that may have a specified height and then use it. In strict mode, this violates the
    // specification, which states that percentage heights just revert to auto if the containing
    // block has an auto height. We still skip anonymous containing blocks in both modes, though, and look
    // only at explicit containers.
    bool skippedAutoHeightContainingBlock = false;
    RenderBlock* cb = containingBlock();
    while (!cb->isRenderView() && !cb->isBody() && !cb->isTableCell() && !cb->isPositioned() && cb->style()->logicalHeight().isAuto()) {
        if (!document()->inQuirksMode() && !cb->isAnonymousBlock())
            break;
        skippedAutoHeightContainingBlock = true;
        cb = cb->containingBlock();
        cb->addPercentHeightDescendant(this);
    }

    // A positioned element that specified both top/bottom or that specifies height should be treated as though it has a height
    // explicitly specified that can be used for any percentage computations.
    // FIXME: We can't just check top/bottom here.
    // https://bugs.webkit.org/show_bug.cgi?id=46500
    bool isPositionedWithSpecifiedHeight = cb->isPositioned() && (!cb->style()->logicalHeight().isAuto() || (!cb->style()->top().isAuto() && !cb->style()->bottom().isAuto()));

    bool includeBorderPadding = isTable();

    // Table cells violate what the CSS spec says to do with heights.  Basically we
    // don't care if the cell specified a height or not.  We just always make ourselves
    // be a percentage of the cell's current content height.
    if (cb->isTableCell()) {
        if (!skippedAutoHeightContainingBlock) {
            if (!cb->hasOverrideHeight()) {
                // Normally we would let the cell size intrinsically, but scrolling overflow has to be
                // treated differently, since WinIE lets scrolled overflow regions shrink as needed.
                // While we can't get all cases right, we can at least detect when the cell has a specified
                // height or when the table has a specified height.  In these cases we want to initially have
                // no size and allow the flexing of the table or the cell to its specified height to cause us
                // to grow to fill the space.  This could end up being wrong in some cases, but it is
                // preferable to the alternative (sizing intrinsically and making the row end up too big).
                RenderTableCell* cell = toRenderTableCell(cb);
                if (scrollsOverflowY() && (!cell->style()->logicalHeight().isAuto() || !cell->table()->style()->logicalHeight().isAuto()))
                    return 0;
                return -1;
            }
            result = cb->overrideHeight();
            includeBorderPadding = true;
        }
    }
    // Otherwise we only use our percentage height if our containing block had a specified
    // height.
    else if (cb->style()->logicalHeight().isFixed())
        result = cb->computeContentBoxLogicalHeight(cb->style()->logicalHeight().value());
    else if (cb->style()->logicalHeight().isPercent() && !isPositionedWithSpecifiedHeight) {
        // We need to recur and compute the percentage height for our containing block.
        result = cb->computePercentageLogicalHeight(cb->style()->logicalHeight());
        // FIXME: Use < 0 or roughlyEquals when we move to float, see https://bugs.webkit.org/show_bug.cgi?id=66148
        if (result != -1)
            result = cb->computeContentBoxLogicalHeight(result);
    } else if (cb->isRenderView() || (cb->isBody() && document()->inQuirksMode()) || isPositionedWithSpecifiedHeight) {
        // Don't allow this to affect the block' height() member variable, since this
        // can get called while the block is still laying out its kids.
        LayoutUnit oldHeight = cb->logicalHeight();
        cb->computeLogicalHeight();
        result = cb->contentLogicalHeight();
        cb->setLogicalHeight(oldHeight);
    } else if (cb->isRoot() && isPositioned())
        // Match the positioned objects behavior, which is that positioned objects will fill their viewport
        // always.  Note we could only hit this case by recurring into computePercentageLogicalHeight on a positioned containing block.
        result = cb->computeContentBoxLogicalHeight(cb->availableLogicalHeight());

    // FIXME: Use < 0 or roughlyEquals when we move to float, see https://bugs.webkit.org/show_bug.cgi?id=66148
    if (result != -1) {
        result = height.calcValue(result);
        if (includeBorderPadding) {
            // It is necessary to use the border-box to match WinIE's broken
            // box model.  This is essential for sizing inside
            // table cells using percentage heights.
            result -= borderAndPaddingLogicalHeight();
            result = max<LayoutUnit>(0, result);
        }
    }
    return result;
}

LayoutUnit RenderBox::computeReplacedLogicalWidth(bool includeMaxWidth) const
{
    return computeReplacedLogicalWidthRespectingMinMaxWidth(computeReplacedLogicalWidthUsing(style()->logicalWidth()), includeMaxWidth);
}

LayoutUnit RenderBox::computeReplacedLogicalWidthRespectingMinMaxWidth(LayoutUnit logicalWidth, bool includeMaxWidth) const
{
    LayoutUnit minLogicalWidth = computeReplacedLogicalWidthUsing(style()->logicalMinWidth());
    LayoutUnit maxLogicalWidth = !includeMaxWidth || style()->logicalMaxWidth().isUndefined() ? logicalWidth : computeReplacedLogicalWidthUsing(style()->logicalMaxWidth());
    return max(minLogicalWidth, min(logicalWidth, maxLogicalWidth));
}

LayoutUnit RenderBox::computeReplacedLogicalWidthUsing(Length logicalWidth) const
{
    switch (logicalWidth.type()) {
        case Fixed:
            return computeContentBoxLogicalWidth(logicalWidth.value());
        case Percent: {
            // FIXME: containingBlockLogicalWidthForContent() is wrong if the replaced element's block-flow is perpendicular to the
            // containing block's block-flow.
            // https://bugs.webkit.org/show_bug.cgi?id=46496
            const LayoutUnit cw = isPositioned() ? containingBlockLogicalWidthForPositioned(toRenderBoxModelObject(container())) : containingBlockLogicalWidthForContent();
            if (cw > 0)
                return computeContentBoxLogicalWidth(logicalWidth.calcMinValue(cw));
        }
        // fall through
        default:
            return intrinsicLogicalWidth();
     }
}

LayoutUnit RenderBox::computeReplacedLogicalHeight() const
{
    return computeReplacedLogicalHeightRespectingMinMaxHeight(computeReplacedLogicalHeightUsing(style()->logicalHeight()));
}

LayoutUnit RenderBox::computeReplacedLogicalHeightRespectingMinMaxHeight(LayoutUnit logicalHeight) const
{
    LayoutUnit minLogicalHeight = computeReplacedLogicalHeightUsing(style()->logicalMinHeight());
    LayoutUnit maxLogicalHeight = style()->logicalMaxHeight().isUndefined() ? logicalHeight : computeReplacedLogicalHeightUsing(style()->logicalMaxHeight());
    return max(minLogicalHeight, min(logicalHeight, maxLogicalHeight));
}

LayoutUnit RenderBox::computeReplacedLogicalHeightUsing(Length logicalHeight) const
{
    switch (logicalHeight.type()) {
        case Fixed:
            return computeContentBoxLogicalHeight(logicalHeight.value());
        case Percent:
        {
            RenderObject* cb = isPositioned() ? container() : containingBlock();
            while (cb->isAnonymous()) {
                cb = cb->containingBlock();
                toRenderBlock(cb)->addPercentHeightDescendant(const_cast<RenderBox*>(this));
            }

            // FIXME: This calculation is not patched for block-flow yet.
            // https://bugs.webkit.org/show_bug.cgi?id=46500
            if (cb->isPositioned() && cb->style()->height().isAuto() && !(cb->style()->top().isAuto() || cb->style()->bottom().isAuto())) {
                ASSERT(cb->isRenderBlock());
                RenderBlock* block = toRenderBlock(cb);
                LayoutUnit oldHeight = block->height();
                block->computeLogicalHeight();
                LayoutUnit newHeight = block->computeContentBoxLogicalHeight(block->contentHeight());
                block->setHeight(oldHeight);
                return computeContentBoxLogicalHeight(logicalHeight.calcValue(newHeight));
            }
            
            // FIXME: availableLogicalHeight() is wrong if the replaced element's block-flow is perpendicular to the
            // containing block's block-flow.
            // https://bugs.webkit.org/show_bug.cgi?id=46496
            LayoutUnit availableHeight;
            if (isPositioned())
                availableHeight = containingBlockLogicalHeightForPositioned(toRenderBoxModelObject(cb));
            else {
                availableHeight =  toRenderBox(cb)->availableLogicalHeight();
                // It is necessary to use the border-box to match WinIE's broken
                // box model.  This is essential for sizing inside
                // table cells using percentage heights.
                // FIXME: This needs to be made block-flow-aware.  If the cell and image are perpendicular block-flows, this isn't right.
                // https://bugs.webkit.org/show_bug.cgi?id=46997
                while (cb && !cb->isRenderView() && (cb->style()->logicalHeight().isAuto() || cb->style()->logicalHeight().isPercent())) {
                    if (cb->isTableCell()) {
                        // Don't let table cells squeeze percent-height replaced elements
                        // <http://bugs.webkit.org/show_bug.cgi?id=15359>
                        availableHeight = max(availableHeight, intrinsicLogicalHeight());
                        return logicalHeight.calcValue(availableHeight - borderAndPaddingLogicalHeight());
                    }
                    cb = cb->containingBlock();
                }
            }
            return computeContentBoxLogicalHeight(logicalHeight.calcValue(availableHeight));
        }
        default:
            return intrinsicLogicalHeight();
    }
}

LayoutUnit RenderBox::availableLogicalHeight() const
{
    return availableLogicalHeightUsing(style()->logicalHeight());
}

LayoutUnit RenderBox::availableLogicalHeightUsing(const Length& h) const
{
    if (h.isFixed())
        return computeContentBoxLogicalHeight(h.value());

    if (isRenderView())
        return isHorizontalWritingMode() ? toRenderView(this)->frameView()->visibleHeight() : toRenderView(this)->frameView()->visibleWidth();

    // We need to stop here, since we don't want to increase the height of the table
    // artificially.  We're going to rely on this cell getting expanded to some new
    // height, and then when we lay out again we'll use the calculation below.
    if (isTableCell() && (h.isAuto() || h.isPercent()))
        return overrideHeight() - borderAndPaddingLogicalWidth();

    if (h.isPercent()) {
        LayoutUnit availableHeight;
        // https://bugs.webkit.org/show_bug.cgi?id=64046
        // For absolutely positioned elements whose containing block is based on a block-level element,
        // the percentage is calculated with respect to the height of the padding box of that element
        if (isPositioned())
            availableHeight = containingBlockLogicalHeightForPositioned(containingBlock());
        else
            availableHeight = containingBlock()->availableLogicalHeight();
        return computeContentBoxLogicalHeight(h.calcValue(availableHeight));
    }

    // FIXME: We can't just check top/bottom here.
    // https://bugs.webkit.org/show_bug.cgi?id=46500
    if (isRenderBlock() && isPositioned() && style()->height().isAuto() && !(style()->top().isAuto() || style()->bottom().isAuto())) {
        RenderBlock* block = const_cast<RenderBlock*>(toRenderBlock(this));
        LayoutUnit oldHeight = block->logicalHeight();
        block->computeLogicalHeight();
        LayoutUnit newHeight = block->computeContentBoxLogicalHeight(block->contentLogicalHeight());
        block->setLogicalHeight(oldHeight);
        return computeContentBoxLogicalHeight(newHeight);
    }

    return containingBlock()->availableLogicalHeight();
}

void RenderBox::computeBlockDirectionMargins(RenderBlock* containingBlock)
{
    if (isTableCell()) {
        // FIXME: Not right if we allow cells to have different directionality than the table.  If we do allow this, though,
        // we may just do it with an extra anonymous block inside the cell.
        setMarginBefore(0);
        setMarginAfter(0);
        return;
    }

    // Margins are calculated with respect to the logical width of
    // the containing block (8.3)
    LayoutUnit cw = containingBlockLogicalWidthForContent();

    RenderStyle* containingBlockStyle = containingBlock->style();
    containingBlock->setMarginBeforeForChild(this, style()->marginBeforeUsing(containingBlockStyle).calcMinValue(cw));
    containingBlock->setMarginAfterForChild(this, style()->marginAfterUsing(containingBlockStyle).calcMinValue(cw));
}

LayoutUnit RenderBox::containingBlockLogicalWidthForPositioned(const RenderBoxModelObject* containingBlock, RenderRegion* region,
    LayoutUnit offsetFromLogicalTopOfFirstPage, bool checkForPerpendicularWritingMode) const
{
    // Container for position:fixed is the frame.
    Frame* frame = view() ? view()->frame(): 0;
    FrameView* frameView = view() ? view()->frameView() : 0;
    if (shouldLayoutFixedElementRelativeToFrame(frame, frameView))
        return (view()->isHorizontalWritingMode() ? frameView->visibleWidth() : frameView->visibleHeight()) / frame->frameScaleFactor();

    if (checkForPerpendicularWritingMode && containingBlock->isHorizontalWritingMode() != isHorizontalWritingMode())
        return containingBlockLogicalHeightForPositioned(containingBlock, false);

    if (containingBlock->isBox()) {
        const RenderBlock* cb = toRenderBlock(containingBlock);
        LayoutUnit result = cb->clientLogicalWidth();
        if (inRenderFlowThread()) {
            RenderBoxRegionInfo* boxInfo = 0;
            if (!region) {
                if (containingBlock->isRenderFlowThread() && !checkForPerpendicularWritingMode)
                    return toRenderFlowThread(containingBlock)->contentLogicalWidthOfFirstRegion();
                if (isWritingModeRoot()) {
                    LayoutUnit cbPageOffset = offsetFromLogicalTopOfFirstPage - logicalTop();
                    RenderRegion* cbRegion = cb->regionAtBlockOffset(cbPageOffset);
                    cbRegion = cb->clampToStartAndEndRegions(cbRegion);
                    boxInfo = cb->renderBoxRegionInfo(cbRegion, cbPageOffset);
                }
            } else if (region && enclosingRenderFlowThread()->isHorizontalWritingMode() == containingBlock->isHorizontalWritingMode()) {
                RenderRegion* containingBlockRegion = cb->clampToStartAndEndRegions(region);
                boxInfo = cb->renderBoxRegionInfo(containingBlockRegion, offsetFromLogicalTopOfFirstPage - logicalTop());
            }
            if (boxInfo)
                return max<LayoutUnit>(0, result - (cb->logicalWidth() - boxInfo->logicalWidth()));
        }
        return result;
    }

    ASSERT(containingBlock->isRenderInline() && containingBlock->isRelPositioned());

    const RenderInline* flow = toRenderInline(containingBlock);
    InlineFlowBox* first = flow->firstLineBox();
    InlineFlowBox* last = flow->lastLineBox();

    // If the containing block is empty, return a width of 0.
    if (!first || !last)
        return 0;

    LayoutUnit fromLeft;
    LayoutUnit fromRight;
    if (containingBlock->style()->isLeftToRightDirection()) {
        fromLeft = first->logicalLeft() + first->borderLogicalLeft();
        fromRight = last->logicalLeft() + last->logicalWidth() - last->borderLogicalRight();
    } else {
        fromRight = first->logicalLeft() + first->logicalWidth() - first->borderLogicalRight();
        fromLeft = last->logicalLeft() + last->borderLogicalLeft();
    }

    return max<LayoutUnit>(0, fromRight - fromLeft);
}

LayoutUnit RenderBox::containingBlockLogicalHeightForPositioned(const RenderBoxModelObject* containingBlock, bool checkForPerpendicularWritingMode) const
{
    Frame* frame = view() ? view()->frame(): 0;
    FrameView* frameView = view() ? view()->frameView() : 0;
    if (shouldLayoutFixedElementRelativeToFrame(frame, frameView))
        return (view()->isHorizontalWritingMode() ? frameView->visibleHeight() : frameView->visibleWidth()) / frame->frameScaleFactor();

    if (checkForPerpendicularWritingMode && containingBlock->isHorizontalWritingMode() != isHorizontalWritingMode())
        return containingBlockLogicalWidthForPositioned(containingBlock, 0, 0, false);

    if (containingBlock->isBox()) {
        const RenderBlock* cb = toRenderBlock(containingBlock);
        LayoutUnit result = cb->clientLogicalHeight();
        if (inRenderFlowThread() && containingBlock->isRenderFlowThread() && enclosingRenderFlowThread()->isHorizontalWritingMode() == containingBlock->isHorizontalWritingMode())
            return toRenderFlowThread(containingBlock)->contentLogicalHeightOfFirstRegion();
        return result;
    }
        
    ASSERT(containingBlock->isRenderInline() && containingBlock->isRelPositioned());

    const RenderInline* flow = toRenderInline(containingBlock);
    InlineFlowBox* first = flow->firstLineBox();
    InlineFlowBox* last = flow->lastLineBox();

    // If the containing block is empty, return a height of 0.
    if (!first || !last)
        return 0;

    LayoutUnit heightResult;
    LayoutRect boundingBox = flow->linesBoundingBox();
    if (containingBlock->isHorizontalWritingMode())
        heightResult = boundingBox.height();
    else
        heightResult = boundingBox.width();
    heightResult -= (containingBlock->borderBefore() + containingBlock->borderAfter());
    return heightResult;
}

static void computeInlineStaticDistance(Length& logicalLeft, Length& logicalRight, const RenderBox* child, const RenderBoxModelObject* containerBlock, LayoutUnit containerLogicalWidth, RenderRegion* region)
{
    if (!logicalLeft.isAuto() || !logicalRight.isAuto())
        return;

    // FIXME: The static distance computation has not been patched for mixed writing modes yet.
    if (child->parent()->style()->direction() == LTR) {
        LayoutUnit staticPosition = child->layer()->staticInlinePosition() - containerBlock->borderLogicalLeft();
        for (RenderObject* curr = child->parent(); curr && curr != containerBlock; curr = curr->container()) {
            if (curr->isBox()) {
                staticPosition += toRenderBox(curr)->logicalLeft();
                if (region && curr->isRenderBlock()) {
                    const RenderBlock* cb = toRenderBlock(curr);
                    region = cb->clampToStartAndEndRegions(region);
                    RenderBoxRegionInfo* boxInfo = cb->renderBoxRegionInfo(region, region->offsetFromLogicalTopOfFirstPage());
                    if (boxInfo)
                        staticPosition += boxInfo->logicalLeft();
                }
            }
        }
        logicalLeft.setValue(Fixed, staticPosition);
    } else {
        RenderBox* enclosingBox = child->parent()->enclosingBox();
        LayoutUnit staticPosition = child->layer()->staticInlinePosition() + containerLogicalWidth + containerBlock->borderLogicalLeft();
        for (RenderObject* curr = enclosingBox; curr; curr = curr->container()) {
            if (curr->isBox()) {
                if (curr != containerBlock)
                    staticPosition -= toRenderBox(curr)->logicalLeft();
                if (curr == enclosingBox)
                    staticPosition -= enclosingBox->logicalWidth();
                if (region && curr->isRenderBlock()) {
                     const RenderBlock* cb = toRenderBlock(curr);
                     region = cb->clampToStartAndEndRegions(region);
                     RenderBoxRegionInfo* boxInfo = cb->renderBoxRegionInfo(region, region->offsetFromLogicalTopOfFirstPage());
                     if (boxInfo) {
                        if (curr != containerBlock)
                            staticPosition -= cb->logicalWidth() - (boxInfo->logicalLeft() + boxInfo->logicalWidth());
                        if (curr == enclosingBox)
                            staticPosition += enclosingBox->logicalWidth() - boxInfo->logicalWidth();
                    }
                }
            }
            if (curr == containerBlock)
                break;
        }
        logicalRight.setValue(Fixed, staticPosition);
    }
}

void RenderBox::computePositionedLogicalWidth(RenderRegion* region, LayoutUnit offsetFromLogicalTopOfFirstPage)
{
    if (isReplaced()) {
        computePositionedLogicalWidthReplaced(); // FIXME: Patch for regions when we add replaced element support.
        return;
    }

    // QUESTIONS
    // FIXME 1: Should we still deal with these the cases of 'left' or 'right' having
    // the type 'static' in determining whether to calculate the static distance?
    // NOTE: 'static' is not a legal value for 'left' or 'right' as of CSS 2.1.

    // FIXME 2: Can perhaps optimize out cases when max-width/min-width are greater
    // than or less than the computed width().  Be careful of box-sizing and
    // percentage issues.

    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.3.7 "Absolutely positioned, non-replaced elements"
    // <http://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width>
    // (block-style-comments in this function and in computePositionedLogicalWidthUsing()
    // correspond to text from the spec)


    // We don't use containingBlock(), since we may be positioned by an enclosing
    // relative positioned inline.
    const RenderBoxModelObject* containerBlock = toRenderBoxModelObject(container());
    
    const LayoutUnit containerLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock, region, offsetFromLogicalTopOfFirstPage);

    // Use the container block's direction except when calculating the static distance
    // This conforms with the reference results for abspos-replaced-width-margin-000.htm
    // of the CSS 2.1 test suite
    TextDirection containerDirection = containerBlock->style()->direction();

    bool isHorizontal = isHorizontalWritingMode();
    const LayoutUnit bordersPlusPadding = borderAndPaddingLogicalWidth();
    const Length marginLogicalLeft = isHorizontal ? style()->marginLeft() : style()->marginTop();
    const Length marginLogicalRight = isHorizontal ? style()->marginRight() : style()->marginBottom();
    LayoutUnit& marginLogicalLeftAlias = isHorizontal ? m_marginLeft : m_marginTop;
    LayoutUnit& marginLogicalRightAlias = isHorizontal ? m_marginRight : m_marginBottom;

    Length logicalLeftLength = style()->logicalLeft();
    Length logicalRightLength = style()->logicalRight();

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

    // see FIXME 1
    // Calculate the static distance if needed.
    computeInlineStaticDistance(logicalLeftLength, logicalRightLength, this, containerBlock, containerLogicalWidth, region);
    
    // Calculate constraint equation values for 'width' case.
    LayoutUnit logicalWidthResult;
    LayoutUnit logicalLeftResult;
    computePositionedLogicalWidthUsing(style()->logicalWidth(), containerBlock, containerDirection,
                                       containerLogicalWidth, bordersPlusPadding,
                                       logicalLeftLength, logicalRightLength, marginLogicalLeft, marginLogicalRight,
                                       logicalWidthResult, marginLogicalLeftAlias, marginLogicalRightAlias, logicalLeftResult);
    setLogicalWidth(logicalWidthResult);
    setLogicalLeft(logicalLeftResult);

    // Calculate constraint equation values for 'max-width' case.
    if (!style()->logicalMaxWidth().isUndefined()) {
        LayoutUnit maxLogicalWidth;
        LayoutUnit maxMarginLogicalLeft;
        LayoutUnit maxMarginLogicalRight;
        LayoutUnit maxLogicalLeftPos;

        computePositionedLogicalWidthUsing(style()->logicalMaxWidth(), containerBlock, containerDirection,
                                           containerLogicalWidth, bordersPlusPadding,
                                           logicalLeftLength, logicalRightLength, marginLogicalLeft, marginLogicalRight,
                                           maxLogicalWidth, maxMarginLogicalLeft, maxMarginLogicalRight, maxLogicalLeftPos);

        if (logicalWidth() > maxLogicalWidth) {
            setLogicalWidth(maxLogicalWidth);
            marginLogicalLeftAlias = maxMarginLogicalLeft;
            marginLogicalRightAlias = maxMarginLogicalRight;
            setLogicalLeft(maxLogicalLeftPos);
        }
    }

    // Calculate constraint equation values for 'min-width' case.
    if (!style()->logicalMinWidth().isZero()) {
        LayoutUnit minLogicalWidth;
        LayoutUnit minMarginLogicalLeft;
        LayoutUnit minMarginLogicalRight;
        LayoutUnit minLogicalLeftPos;

        computePositionedLogicalWidthUsing(style()->logicalMinWidth(), containerBlock, containerDirection,
                                           containerLogicalWidth, bordersPlusPadding,
                                           logicalLeftLength, logicalRightLength, marginLogicalLeft, marginLogicalRight,
                                           minLogicalWidth, minMarginLogicalLeft, minMarginLogicalRight, minLogicalLeftPos);

        if (logicalWidth() < minLogicalWidth) {
            setLogicalWidth(minLogicalWidth);
            marginLogicalLeftAlias = minMarginLogicalLeft;
            marginLogicalRightAlias = minMarginLogicalRight;
            setLogicalLeft(minLogicalLeftPos);
        }
    }

    if (stretchesToMinIntrinsicLogicalWidth() && logicalWidth() < minPreferredLogicalWidth() - bordersPlusPadding) {
        computePositionedLogicalWidthUsing(Length(minPreferredLogicalWidth() - bordersPlusPadding, Fixed), containerBlock, containerDirection,
                                           containerLogicalWidth, bordersPlusPadding,
                                           logicalLeftLength, logicalRightLength, marginLogicalLeft, marginLogicalRight,
                                           logicalWidthResult, marginLogicalLeftAlias, marginLogicalRightAlias, logicalLeftResult);
        setLogicalWidth(logicalWidthResult);
        setLogicalLeft(logicalLeftResult);
    }

    // Put logicalWidth() into correct form.
    setLogicalWidth(logicalWidth() + bordersPlusPadding);
    
    // Adjust logicalLeft if we need to for the flipped version of our writing mode in regions.
    if (inRenderFlowThread() && !region && isWritingModeRoot() && isHorizontalWritingMode() == containerBlock->isHorizontalWritingMode()) {
        LayoutUnit logicalLeftPos = logicalLeft();
        const RenderBlock* cb = toRenderBlock(containerBlock);
        LayoutUnit cbPageOffset = offsetFromLogicalTopOfFirstPage - logicalTop();
        RenderRegion* cbRegion = cb->regionAtBlockOffset(cbPageOffset);
        cbRegion = cb->clampToStartAndEndRegions(cbRegion);
        RenderBoxRegionInfo* boxInfo = cb->renderBoxRegionInfo(cbRegion, cbPageOffset);
        if (boxInfo) {
            logicalLeftPos += boxInfo->logicalLeft();
            setLogicalLeft(logicalLeftPos);
        }
    }
}

static void computeLogicalLeftPositionedOffset(LayoutUnit& logicalLeftPos, const RenderBox* child, LayoutUnit logicalWidthValue, const RenderBoxModelObject* containerBlock, LayoutUnit containerLogicalWidth)
{
    // Deal with differing writing modes here.  Our offset needs to be in the containing block's coordinate space. If the containing block is flipped
    // along this axis, then we need to flip the coordinate.  This can only happen if the containing block is both a flipped mode and perpendicular to us.
    if (containerBlock->isHorizontalWritingMode() != child->isHorizontalWritingMode() && containerBlock->style()->isFlippedBlocksWritingMode()) {
        logicalLeftPos = containerLogicalWidth - logicalWidthValue - logicalLeftPos;
        logicalLeftPos += (child->isHorizontalWritingMode() ? containerBlock->borderRight() : containerBlock->borderBottom());
    } else
        logicalLeftPos += (child->isHorizontalWritingMode() ? containerBlock->borderLeft() : containerBlock->borderTop());
}

void RenderBox::computePositionedLogicalWidthUsing(Length logicalWidth, const RenderBoxModelObject* containerBlock, TextDirection containerDirection,
                                                   LayoutUnit containerLogicalWidth, LayoutUnit bordersPlusPadding,
                                                   Length logicalLeft, Length logicalRight, Length marginLogicalLeft, Length marginLogicalRight,
                                                   LayoutUnit& logicalWidthValue, LayoutUnit& marginLogicalLeftValue, LayoutUnit& marginLogicalRightValue, LayoutUnit& logicalLeftPos)
{
    // 'left' and 'right' cannot both be 'auto' because one would of been
    // converted to the static position already
    ASSERT(!(logicalLeft.isAuto() && logicalRight.isAuto()));

    LayoutUnit logicalLeftValue = 0;

    bool logicalWidthIsAuto = logicalWidth.isIntrinsicOrAuto();
    bool logicalLeftIsAuto = logicalLeft.isAuto();
    bool logicalRightIsAuto = logicalRight.isAuto();

    if (!logicalLeftIsAuto && !logicalWidthIsAuto && !logicalRightIsAuto) {
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

        logicalLeftValue = logicalLeft.calcValue(containerLogicalWidth);
        logicalWidthValue = computeContentBoxLogicalWidth(logicalWidth.calcValue(containerLogicalWidth));

        const LayoutUnit availableSpace = containerLogicalWidth - (logicalLeftValue + logicalWidthValue + logicalRight.calcValue(containerLogicalWidth) + bordersPlusPadding);

        // Margins are now the only unknown
        if (marginLogicalLeft.isAuto() && marginLogicalRight.isAuto()) {
            // Both margins auto, solve for equality
            if (availableSpace >= 0) {
                marginLogicalLeftValue = availableSpace / 2; // split the difference
                marginLogicalRightValue = availableSpace - marginLogicalLeftValue; // account for odd valued differences
            } else {
                // Use the containing block's direction rather than the parent block's
                // per CSS 2.1 reference test abspos-non-replaced-width-margin-000.
                if (containerDirection == LTR) {
                    marginLogicalLeftValue = 0;
                    marginLogicalRightValue = availableSpace; // will be negative
                } else {
                    marginLogicalLeftValue = availableSpace; // will be negative
                    marginLogicalRightValue = 0;
                }
            }
        } else if (marginLogicalLeft.isAuto()) {
            // Solve for left margin
            marginLogicalRightValue = marginLogicalRight.calcValue(containerLogicalWidth);
            marginLogicalLeftValue = availableSpace - marginLogicalRightValue;
        } else if (marginLogicalRight.isAuto()) {
            // Solve for right margin
            marginLogicalLeftValue = marginLogicalLeft.calcValue(containerLogicalWidth);
            marginLogicalRightValue = availableSpace - marginLogicalLeftValue;
        } else {
            // Over-constrained, solve for left if direction is RTL
            marginLogicalLeftValue = marginLogicalLeft.calcValue(containerLogicalWidth);
            marginLogicalRightValue = marginLogicalRight.calcValue(containerLogicalWidth);

            // Use the containing block's direction rather than the parent block's
            // per CSS 2.1 reference test abspos-non-replaced-width-margin-000.
            if (containerDirection == RTL)
                logicalLeftValue = (availableSpace + logicalLeftValue) - marginLogicalLeftValue - marginLogicalRightValue;
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
        marginLogicalLeftValue = marginLogicalLeft.calcMinValue(containerLogicalWidth);
        marginLogicalRightValue = marginLogicalRight.calcMinValue(containerLogicalWidth);

        const LayoutUnit availableSpace = containerLogicalWidth - (marginLogicalLeftValue + marginLogicalRightValue + bordersPlusPadding);

        // FIXME: Is there a faster way to find the correct case?
        // Use rule/case that applies.
        if (logicalLeftIsAuto && logicalWidthIsAuto && !logicalRightIsAuto) {
            // RULE 1: (use shrink-to-fit for width, and solve of left)
            LayoutUnit logicalRightValue = logicalRight.calcValue(containerLogicalWidth);

            // FIXME: would it be better to have shrink-to-fit in one step?
            LayoutUnit preferredWidth = maxPreferredLogicalWidth() - bordersPlusPadding;
            LayoutUnit preferredMinWidth = minPreferredLogicalWidth() - bordersPlusPadding;
            LayoutUnit availableWidth = availableSpace - logicalRightValue;
            logicalWidthValue = min(max(preferredMinWidth, availableWidth), preferredWidth);
            logicalLeftValue = availableSpace - (logicalWidthValue + logicalRightValue);
        } else if (!logicalLeftIsAuto && logicalWidthIsAuto && logicalRightIsAuto) {
            // RULE 3: (use shrink-to-fit for width, and no need solve of right)
            logicalLeftValue = logicalLeft.calcValue(containerLogicalWidth);

            // FIXME: would it be better to have shrink-to-fit in one step?
            LayoutUnit preferredWidth = maxPreferredLogicalWidth() - bordersPlusPadding;
            LayoutUnit preferredMinWidth = minPreferredLogicalWidth() - bordersPlusPadding;
            LayoutUnit availableWidth = availableSpace - logicalLeftValue;
            logicalWidthValue = min(max(preferredMinWidth, availableWidth), preferredWidth);
        } else if (logicalLeftIsAuto && !logicalWidthIsAuto && !logicalRightIsAuto) {
            // RULE 4: (solve for left)
            logicalWidthValue = computeContentBoxLogicalWidth(logicalWidth.calcValue(containerLogicalWidth));
            logicalLeftValue = availableSpace - (logicalWidthValue + logicalRight.calcValue(containerLogicalWidth));
        } else if (!logicalLeftIsAuto && logicalWidthIsAuto && !logicalRightIsAuto) {
            // RULE 5: (solve for width)
            logicalLeftValue = logicalLeft.calcValue(containerLogicalWidth);
            logicalWidthValue = availableSpace - (logicalLeftValue + logicalRight.calcValue(containerLogicalWidth));
        } else if (!logicalLeftIsAuto && !logicalWidthIsAuto && logicalRightIsAuto) {
            // RULE 6: (no need solve for right)
            logicalLeftValue = logicalLeft.calcValue(containerLogicalWidth);
            logicalWidthValue = computeContentBoxLogicalWidth(logicalWidth.calcValue(containerLogicalWidth));
        }
    }

    // Use computed values to calculate the horizontal position.

    // FIXME: This hack is needed to calculate the  logical left position for a 'rtl' relatively
    // positioned, inline because right now, it is using the logical left position
    // of the first line box when really it should use the last line box.  When
    // this is fixed elsewhere, this block should be removed.
    if (containerBlock->isRenderInline() && !containerBlock->style()->isLeftToRightDirection()) {
        const RenderInline* flow = toRenderInline(containerBlock);
        InlineFlowBox* firstLine = flow->firstLineBox();
        InlineFlowBox* lastLine = flow->lastLineBox();
        if (firstLine && lastLine && firstLine != lastLine) {
            logicalLeftPos = logicalLeftValue + marginLogicalLeftValue + lastLine->borderLogicalLeft() + (lastLine->logicalLeft() - firstLine->logicalLeft());
            return;
        }
    }

    logicalLeftPos = logicalLeftValue + marginLogicalLeftValue;
    computeLogicalLeftPositionedOffset(logicalLeftPos, this, logicalWidthValue, containerBlock, containerLogicalWidth);
}

static void computeBlockStaticDistance(Length& logicalTop, Length& logicalBottom, const RenderBox* child, const RenderBoxModelObject* containerBlock)
{
    if (!logicalTop.isAuto() || !logicalBottom.isAuto())
        return;
    
    // FIXME: The static distance computation has not been patched for mixed writing modes.
    LayoutUnit staticLogicalTop = child->layer()->staticBlockPosition() - containerBlock->borderBefore();
    for (RenderObject* curr = child->parent(); curr && curr != containerBlock; curr = curr->container()) {
        if (curr->isBox() && !curr->isTableRow())
            staticLogicalTop += toRenderBox(curr)->logicalTop();
    }
    logicalTop.setValue(Fixed, staticLogicalTop);
}

void RenderBox::computePositionedLogicalHeight()
{
    if (isReplaced()) {
        computePositionedLogicalHeightReplaced();
        return;
    }

    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.6.4 "Absolutely positioned, non-replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-non-replaced-height>
    // (block-style-comments in this function and in computePositionedLogicalHeightUsing()
    // correspond to text from the spec)


    // We don't use containingBlock(), since we may be positioned by an enclosing relpositioned inline.
    const RenderBoxModelObject* containerBlock = toRenderBoxModelObject(container());

    const LayoutUnit containerLogicalHeight = containingBlockLogicalHeightForPositioned(containerBlock);

    bool isHorizontal = isHorizontalWritingMode();
    bool isFlipped = style()->isFlippedBlocksWritingMode();
    const LayoutUnit bordersPlusPadding = borderAndPaddingLogicalHeight();
    const Length marginBefore = style()->marginBefore();
    const Length marginAfter = style()->marginAfter();
    LayoutUnit& marginBeforeAlias = isHorizontal ? (isFlipped ? m_marginBottom : m_marginTop) : (isFlipped ? m_marginRight: m_marginLeft);
    LayoutUnit& marginAfterAlias = isHorizontal ? (isFlipped ? m_marginTop : m_marginBottom) : (isFlipped ? m_marginLeft: m_marginRight);

    Length logicalTopLength = style()->logicalTop();
    Length logicalBottomLength = style()->logicalBottom();
        
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

    // see FIXME 1
    // Calculate the static distance if needed.
    computeBlockStaticDistance(logicalTopLength, logicalBottomLength, this, containerBlock);

    LayoutUnit logicalHeightResult; // Needed to compute overflow.
    LayoutUnit logicalTopPos;

    // Calculate constraint equation values for 'height' case.
    computePositionedLogicalHeightUsing(style()->logicalHeight(), containerBlock, containerLogicalHeight, bordersPlusPadding,
                                        logicalTopLength, logicalBottomLength, marginBefore, marginAfter,
                                        logicalHeightResult, marginBeforeAlias, marginAfterAlias, logicalTopPos);
    setLogicalTop(logicalTopPos);

    // Avoid doing any work in the common case (where the values of min-height and max-height are their defaults).
    // see FIXME 2

    // Calculate constraint equation values for 'max-height' case.
    if (!style()->logicalMaxHeight().isUndefined()) {
        LayoutUnit maxLogicalHeight;
        LayoutUnit maxMarginBefore;
        LayoutUnit maxMarginAfter;
        LayoutUnit maxLogicalTopPos;

        computePositionedLogicalHeightUsing(style()->logicalMaxHeight(), containerBlock, containerLogicalHeight, bordersPlusPadding,
                                            logicalTopLength, logicalBottomLength, marginBefore, marginAfter,
                                            maxLogicalHeight, maxMarginBefore, maxMarginAfter, maxLogicalTopPos);

        if (logicalHeightResult > maxLogicalHeight) {
            logicalHeightResult = maxLogicalHeight;
            marginBeforeAlias = maxMarginBefore;
            marginAfterAlias = maxMarginAfter;
            setLogicalTop(maxLogicalTopPos);
        }
    }

    // Calculate constraint equation values for 'min-height' case.
    if (!style()->logicalMinHeight().isZero()) {
        LayoutUnit minLogicalHeight;
        LayoutUnit minMarginBefore;
        LayoutUnit minMarginAfter;
        LayoutUnit minLogicalTopPos;

        computePositionedLogicalHeightUsing(style()->logicalMinHeight(), containerBlock, containerLogicalHeight, bordersPlusPadding,
                                            logicalTopLength, logicalBottomLength, marginBefore, marginAfter,
                                            minLogicalHeight, minMarginBefore, minMarginAfter, minLogicalTopPos);

        if (logicalHeightResult < minLogicalHeight) {
            logicalHeightResult = minLogicalHeight;
            marginBeforeAlias = minMarginBefore;
            marginAfterAlias = minMarginAfter;
            setLogicalTop(minLogicalTopPos);
        }
    }

    // Set final height value.
    setLogicalHeight(logicalHeightResult + bordersPlusPadding);
    
    // Adjust logicalTop if we need to for perpendicular writing modes in regions.
    if (inRenderFlowThread() && isHorizontalWritingMode() != containerBlock->isHorizontalWritingMode()) {
        LayoutUnit logicalTopPos = logicalTop();
        const RenderBlock* cb = toRenderBlock(containerBlock);
        LayoutUnit cbPageOffset = cb->offsetFromLogicalTopOfFirstPage() - logicalLeft();
        RenderRegion* cbRegion = cb->regionAtBlockOffset(cbPageOffset);
        cbRegion = cb->clampToStartAndEndRegions(cbRegion);
        RenderBoxRegionInfo* boxInfo = cb->renderBoxRegionInfo(cbRegion, cbPageOffset);
        if (boxInfo) {
            logicalTopPos += boxInfo->logicalLeft();
            setLogicalTop(logicalTopPos);
        }
    }
}

static void computeLogicalTopPositionedOffset(LayoutUnit& logicalTopPos, const RenderBox* child, LayoutUnit logicalHeightValue, const RenderBoxModelObject* containerBlock, LayoutUnit containerLogicalHeight)
{
    // Deal with differing writing modes here.  Our offset needs to be in the containing block's coordinate space. If the containing block is flipped
    // along this axis, then we need to flip the coordinate.  This can only happen if the containing block is both a flipped mode and perpendicular to us.
    if ((child->style()->isFlippedBlocksWritingMode() && child->isHorizontalWritingMode() != containerBlock->isHorizontalWritingMode())
        || (child->style()->isFlippedBlocksWritingMode() != containerBlock->style()->isFlippedBlocksWritingMode() && child->isHorizontalWritingMode() == containerBlock->isHorizontalWritingMode()))
        logicalTopPos = containerLogicalHeight - logicalHeightValue - logicalTopPos;

    // Our offset is from the logical bottom edge in a flipped environment, e.g., right for vertical-rl and bottom for horizontal-bt.
    if (containerBlock->style()->isFlippedBlocksWritingMode() && child->isHorizontalWritingMode() == containerBlock->isHorizontalWritingMode()) {
        if (child->isHorizontalWritingMode())
            logicalTopPos += containerBlock->borderBottom();
        else
            logicalTopPos += containerBlock->borderRight();
    } else {
        if (child->isHorizontalWritingMode())
            logicalTopPos += containerBlock->borderTop();
        else
            logicalTopPos += containerBlock->borderLeft();
    }
}

void RenderBox::computePositionedLogicalHeightUsing(Length logicalHeightLength, const RenderBoxModelObject* containerBlock,
                                                    LayoutUnit containerLogicalHeight, LayoutUnit bordersPlusPadding,
                                                    Length logicalTop, Length logicalBottom, Length marginBefore, Length marginAfter,
                                                    LayoutUnit& logicalHeightValue, LayoutUnit& marginBeforeValue, LayoutUnit& marginAfterValue, LayoutUnit& logicalTopPos)
{
    // 'top' and 'bottom' cannot both be 'auto' because 'top would of been
    // converted to the static position in computePositionedLogicalHeight()
    ASSERT(!(logicalTop.isAuto() && logicalBottom.isAuto()));

    LayoutUnit contentLogicalHeight = logicalHeight() - bordersPlusPadding;

    LayoutUnit logicalTopValue = 0;

    bool logicalHeightIsAuto = logicalHeightLength.isAuto();
    bool logicalTopIsAuto = logicalTop.isAuto();
    bool logicalBottomIsAuto = logicalBottom.isAuto();

    // Height is never unsolved for tables.
    if (isTable()) {
        logicalHeightLength.setValue(Fixed, contentLogicalHeight);
        logicalHeightIsAuto = false;
    }

    if (!logicalTopIsAuto && !logicalHeightIsAuto && !logicalBottomIsAuto) {
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

        logicalHeightValue = computeContentBoxLogicalHeight(logicalHeightLength.calcValue(containerLogicalHeight));
        logicalTopValue = logicalTop.calcValue(containerLogicalHeight);

        const LayoutUnit availableSpace = containerLogicalHeight - (logicalTopValue + logicalHeightValue + logicalBottom.calcValue(containerLogicalHeight) + bordersPlusPadding);

        // Margins are now the only unknown
        if (marginBefore.isAuto() && marginAfter.isAuto()) {
            // Both margins auto, solve for equality
            // NOTE: This may result in negative values.
            marginBeforeValue = availableSpace / 2; // split the difference
            marginAfterValue = availableSpace - marginBeforeValue; // account for odd valued differences
        } else if (marginBefore.isAuto()) {
            // Solve for top margin
            marginAfterValue = marginAfter.calcValue(containerLogicalHeight);
            marginBeforeValue = availableSpace - marginAfterValue;
        } else if (marginAfter.isAuto()) {
            // Solve for bottom margin
            marginBeforeValue = marginBefore.calcValue(containerLogicalHeight);
            marginAfterValue = availableSpace - marginBeforeValue;
        } else {
            // Over-constrained, (no need solve for bottom)
            marginBeforeValue = marginBefore.calcValue(containerLogicalHeight);
            marginAfterValue = marginAfter.calcValue(containerLogicalHeight);
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
        marginBeforeValue = marginBefore.calcMinValue(containerLogicalHeight);
        marginAfterValue = marginAfter.calcMinValue(containerLogicalHeight);

        const LayoutUnit availableSpace = containerLogicalHeight - (marginBeforeValue + marginAfterValue + bordersPlusPadding);

        // Use rule/case that applies.
        if (logicalTopIsAuto && logicalHeightIsAuto && !logicalBottomIsAuto) {
            // RULE 1: (height is content based, solve of top)
            logicalHeightValue = contentLogicalHeight;
            logicalTopValue = availableSpace - (logicalHeightValue + logicalBottom.calcValue(containerLogicalHeight));
        } else if (!logicalTopIsAuto && logicalHeightIsAuto && logicalBottomIsAuto) {
            // RULE 3: (height is content based, no need solve of bottom)
            logicalTopValue = logicalTop.calcValue(containerLogicalHeight);
            logicalHeightValue = contentLogicalHeight;
        } else if (logicalTopIsAuto && !logicalHeightIsAuto && !logicalBottomIsAuto) {
            // RULE 4: (solve of top)
            logicalHeightValue = computeContentBoxLogicalHeight(logicalHeightLength.calcValue(containerLogicalHeight));
            logicalTopValue = availableSpace - (logicalHeightValue + logicalBottom.calcValue(containerLogicalHeight));
        } else if (!logicalTopIsAuto && logicalHeightIsAuto && !logicalBottomIsAuto) {
            // RULE 5: (solve of height)
            logicalTopValue = logicalTop.calcValue(containerLogicalHeight);
            logicalHeightValue = max<LayoutUnit>(0, availableSpace - (logicalTopValue + logicalBottom.calcValue(containerLogicalHeight)));
        } else if (!logicalTopIsAuto && !logicalHeightIsAuto && logicalBottomIsAuto) {
            // RULE 6: (no need solve of bottom)
            logicalHeightValue = computeContentBoxLogicalHeight(logicalHeightLength.calcValue(containerLogicalHeight));
            logicalTopValue = logicalTop.calcValue(containerLogicalHeight);
        }
    }

    // Use computed values to calculate the vertical position.
    logicalTopPos = logicalTopValue + marginBeforeValue;
    computeLogicalTopPositionedOffset(logicalTopPos, this, logicalHeightValue, containerBlock, containerLogicalHeight);
}

void RenderBox::computePositionedLogicalWidthReplaced()
{
    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.3.8 "Absolutely positioned, replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-replaced-width>
    // (block-style-comments in this function correspond to text from the spec and
    // the numbers correspond to numbers in spec)

    // We don't use containingBlock(), since we may be positioned by an enclosing
    // relative positioned inline.
    const RenderBoxModelObject* containerBlock = toRenderBoxModelObject(container());

    const LayoutUnit containerLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock);

    // To match WinIE, in quirks mode use the parent's 'direction' property
    // instead of the the container block's.
    TextDirection containerDirection = containerBlock->style()->direction();

    // Variables to solve.
    bool isHorizontal = isHorizontalWritingMode();
    Length logicalLeft = style()->logicalLeft();
    Length logicalRight = style()->logicalRight();
    Length marginLogicalLeft = isHorizontal ? style()->marginLeft() : style()->marginTop();
    Length marginLogicalRight = isHorizontal ? style()->marginRight() : style()->marginBottom();
    LayoutUnit& marginLogicalLeftAlias = isHorizontal ? m_marginLeft : m_marginTop;
    LayoutUnit& marginLogicalRightAlias = isHorizontal ? m_marginRight : m_marginBottom;

    /*-----------------------------------------------------------------------*\
     * 1. The used value of 'width' is determined as for inline replaced
     *    elements.
    \*-----------------------------------------------------------------------*/
    // NOTE: This value of width is FINAL in that the min/max width calculations
    // are dealt with in computeReplacedWidth().  This means that the steps to produce
    // correct max/min in the non-replaced version, are not necessary.
    setLogicalWidth(computeReplacedLogicalWidth() + borderAndPaddingLogicalWidth());

    const LayoutUnit availableSpace = containerLogicalWidth - logicalWidth();

    /*-----------------------------------------------------------------------*\
     * 2. If both 'left' and 'right' have the value 'auto', then if 'direction'
     *    of the containing block is 'ltr', set 'left' to the static position;
     *    else if 'direction' is 'rtl', set 'right' to the static position.
    \*-----------------------------------------------------------------------*/
    // see FIXME 1
    computeInlineStaticDistance(logicalLeft, logicalRight, this, containerBlock, containerLogicalWidth, 0); // FIXME: Pass the region.

    /*-----------------------------------------------------------------------*\
     * 3. If 'left' or 'right' are 'auto', replace any 'auto' on 'margin-left'
     *    or 'margin-right' with '0'.
    \*-----------------------------------------------------------------------*/
    if (logicalLeft.isAuto() || logicalRight.isAuto()) {
        if (marginLogicalLeft.isAuto())
            marginLogicalLeft.setValue(Fixed, 0);
        if (marginLogicalRight.isAuto())
            marginLogicalRight.setValue(Fixed, 0);
    }

    /*-----------------------------------------------------------------------*\
     * 4. If at this point both 'margin-left' and 'margin-right' are still
     *    'auto', solve the equation under the extra constraint that the two
     *    margins must get equal values, unless this would make them negative,
     *    in which case when the direction of the containing block is 'ltr'
     *    ('rtl'), set 'margin-left' ('margin-right') to zero and solve for
     *    'margin-right' ('margin-left').
    \*-----------------------------------------------------------------------*/
    LayoutUnit logicalLeftValue = 0;
    LayoutUnit logicalRightValue = 0;

    if (marginLogicalLeft.isAuto() && marginLogicalRight.isAuto()) {
        // 'left' and 'right' cannot be 'auto' due to step 3
        ASSERT(!(logicalLeft.isAuto() && logicalRight.isAuto()));

        logicalLeftValue = logicalLeft.calcValue(containerLogicalWidth);
        logicalRightValue = logicalRight.calcValue(containerLogicalWidth);

        LayoutUnit difference = availableSpace - (logicalLeftValue + logicalRightValue);
        if (difference > 0) {
            marginLogicalLeftAlias = difference / 2; // split the difference
            marginLogicalRightAlias = difference - marginLogicalLeftAlias; // account for odd valued differences
        } else {
            // Use the containing block's direction rather than the parent block's
            // per CSS 2.1 reference test abspos-replaced-width-margin-000.
            if (containerDirection == LTR) {
                marginLogicalLeftAlias = 0;
                marginLogicalRightAlias = difference; // will be negative
            } else {
                marginLogicalLeftAlias = difference; // will be negative
                marginLogicalRightAlias = 0;
            }
        }

    /*-----------------------------------------------------------------------*\
     * 5. If at this point there is an 'auto' left, solve the equation for
     *    that value.
    \*-----------------------------------------------------------------------*/
    } else if (logicalLeft.isAuto()) {
        marginLogicalLeftAlias = marginLogicalLeft.calcValue(containerLogicalWidth);
        marginLogicalRightAlias = marginLogicalRight.calcValue(containerLogicalWidth);
        logicalRightValue = logicalRight.calcValue(containerLogicalWidth);

        // Solve for 'left'
        logicalLeftValue = availableSpace - (logicalRightValue + marginLogicalLeftAlias + marginLogicalRightAlias);
    } else if (logicalRight.isAuto()) {
        marginLogicalLeftAlias = marginLogicalLeft.calcValue(containerLogicalWidth);
        marginLogicalRightAlias = marginLogicalRight.calcValue(containerLogicalWidth);
        logicalLeftValue = logicalLeft.calcValue(containerLogicalWidth);

        // Solve for 'right'
        logicalRightValue = availableSpace - (logicalLeftValue + marginLogicalLeftAlias + marginLogicalRightAlias);
    } else if (marginLogicalLeft.isAuto()) {
        marginLogicalRightAlias = marginLogicalRight.calcValue(containerLogicalWidth);
        logicalLeftValue = logicalLeft.calcValue(containerLogicalWidth);
        logicalRightValue = logicalRight.calcValue(containerLogicalWidth);

        // Solve for 'margin-left'
        marginLogicalLeftAlias = availableSpace - (logicalLeftValue + logicalRightValue + marginLogicalRightAlias);
    } else if (marginLogicalRight.isAuto()) {
        marginLogicalLeftAlias = marginLogicalLeft.calcValue(containerLogicalWidth);
        logicalLeftValue = logicalLeft.calcValue(containerLogicalWidth);
        logicalRightValue = logicalRight.calcValue(containerLogicalWidth);

        // Solve for 'margin-right'
        marginLogicalRightAlias = availableSpace - (logicalLeftValue + logicalRightValue + marginLogicalLeftAlias);
    } else {
        // Nothing is 'auto', just calculate the values.
        marginLogicalLeftAlias = marginLogicalLeft.calcValue(containerLogicalWidth);
        marginLogicalRightAlias = marginLogicalRight.calcValue(containerLogicalWidth);
        logicalRightValue = logicalRight.calcValue(containerLogicalWidth);
        logicalLeftValue = logicalLeft.calcValue(containerLogicalWidth);
        // If the containing block is right-to-left, then push the left position as far to the right as possible
        if (containerDirection == RTL) {
            int totalLogicalWidth = logicalWidth() + logicalLeftValue + logicalRightValue +  marginLogicalLeftAlias + marginLogicalRightAlias;
            logicalLeftValue = containerLogicalWidth - (totalLogicalWidth - logicalLeftValue);
        }
    }

    /*-----------------------------------------------------------------------*\
     * 6. If at this point the values are over-constrained, ignore the value
     *    for either 'left' (in case the 'direction' property of the
     *    containing block is 'rtl') or 'right' (in case 'direction' is
     *    'ltr') and solve for that value.
    \*-----------------------------------------------------------------------*/
    // NOTE: Constraints imposed by the width of the containing block and its content have already been accounted for above.

    // FIXME: Deal with differing writing modes here.  Our offset needs to be in the containing block's coordinate space, so that
    // can make the result here rather complicated to compute.

    // Use computed values to calculate the horizontal position.

    // FIXME: This hack is needed to calculate the logical left position for a 'rtl' relatively
    // positioned, inline containing block because right now, it is using the logical left position
    // of the first line box when really it should use the last line box.  When
    // this is fixed elsewhere, this block should be removed.
    if (containerBlock->isRenderInline() && !containerBlock->style()->isLeftToRightDirection()) {
        const RenderInline* flow = toRenderInline(containerBlock);
        InlineFlowBox* firstLine = flow->firstLineBox();
        InlineFlowBox* lastLine = flow->lastLineBox();
        if (firstLine && lastLine && firstLine != lastLine) {
            setLogicalLeft(logicalLeftValue + marginLogicalLeftAlias + lastLine->borderLogicalLeft() + (lastLine->logicalLeft() - firstLine->logicalLeft()));
            return;
        }
    }

    LayoutUnit logicalLeftPos = logicalLeftValue + marginLogicalLeftAlias;
    computeLogicalLeftPositionedOffset(logicalLeftPos, this, logicalWidth(), containerBlock, containerLogicalWidth);
    setLogicalLeft(logicalLeftPos);
}

void RenderBox::computePositionedLogicalHeightReplaced()
{
    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.6.5 "Absolutely positioned, replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-replaced-height>
    // (block-style-comments in this function correspond to text from the spec and
    // the numbers correspond to numbers in spec)

    // We don't use containingBlock(), since we may be positioned by an enclosing relpositioned inline.
    const RenderBoxModelObject* containerBlock = toRenderBoxModelObject(container());

    const LayoutUnit containerLogicalHeight = containingBlockLogicalHeightForPositioned(containerBlock);

    // Variables to solve.
    bool isHorizontal = isHorizontalWritingMode();
    bool isFlipped = style()->isFlippedBlocksWritingMode();
    Length marginBefore = style()->marginBefore();
    Length marginAfter = style()->marginAfter();
    LayoutUnit& marginBeforeAlias = isHorizontal ? (isFlipped ? m_marginBottom : m_marginTop) : (isFlipped ? m_marginRight: m_marginLeft);
    LayoutUnit& marginAfterAlias = isHorizontal ? (isFlipped ? m_marginTop : m_marginBottom) : (isFlipped ? m_marginLeft: m_marginRight);

    Length logicalTop = style()->logicalTop();
    Length logicalBottom = style()->logicalBottom();

    /*-----------------------------------------------------------------------*\
     * 1. The used value of 'height' is determined as for inline replaced
     *    elements.
    \*-----------------------------------------------------------------------*/
    // NOTE: This value of height is FINAL in that the min/max height calculations
    // are dealt with in computeReplacedHeight().  This means that the steps to produce
    // correct max/min in the non-replaced version, are not necessary.
    setLogicalHeight(computeReplacedLogicalHeight() + borderAndPaddingLogicalHeight());
    const LayoutUnit availableSpace = containerLogicalHeight - logicalHeight();

    /*-----------------------------------------------------------------------*\
     * 2. If both 'top' and 'bottom' have the value 'auto', replace 'top'
     *    with the element's static position.
    \*-----------------------------------------------------------------------*/
    // see FIXME 1
    computeBlockStaticDistance(logicalTop, logicalBottom, this, containerBlock);

    /*-----------------------------------------------------------------------*\
     * 3. If 'bottom' is 'auto', replace any 'auto' on 'margin-top' or
     *    'margin-bottom' with '0'.
    \*-----------------------------------------------------------------------*/
    // FIXME: The spec. says that this step should only be taken when bottom is
    // auto, but if only top is auto, this makes step 4 impossible.
    if (logicalTop.isAuto() || logicalBottom.isAuto()) {
        if (marginBefore.isAuto())
            marginBefore.setValue(Fixed, 0);
        if (marginAfter.isAuto())
            marginAfter.setValue(Fixed, 0);
    }

    /*-----------------------------------------------------------------------*\
     * 4. If at this point both 'margin-top' and 'margin-bottom' are still
     *    'auto', solve the equation under the extra constraint that the two
     *    margins must get equal values.
    \*-----------------------------------------------------------------------*/
    LayoutUnit logicalTopValue = 0;
    LayoutUnit logicalBottomValue = 0;

    if (marginBefore.isAuto() && marginAfter.isAuto()) {
        // 'top' and 'bottom' cannot be 'auto' due to step 2 and 3 combined.
        ASSERT(!(logicalTop.isAuto() || logicalBottom.isAuto()));

        logicalTopValue = logicalTop.calcValue(containerLogicalHeight);
        logicalBottomValue = logicalBottom.calcValue(containerLogicalHeight);

        LayoutUnit difference = availableSpace - (logicalTopValue + logicalBottomValue);
        // NOTE: This may result in negative values.
        marginBeforeAlias =  difference / 2; // split the difference
        marginAfterAlias = difference - marginBeforeAlias; // account for odd valued differences

    /*-----------------------------------------------------------------------*\
     * 5. If at this point there is only one 'auto' left, solve the equation
     *    for that value.
    \*-----------------------------------------------------------------------*/
    } else if (logicalTop.isAuto()) {
        marginBeforeAlias = marginBefore.calcValue(containerLogicalHeight);
        marginAfterAlias = marginAfter.calcValue(containerLogicalHeight);
        logicalBottomValue = logicalBottom.calcValue(containerLogicalHeight);

        // Solve for 'top'
        logicalTopValue = availableSpace - (logicalBottomValue + marginBeforeAlias + marginAfterAlias);
    } else if (logicalBottom.isAuto()) {
        marginBeforeAlias = marginBefore.calcValue(containerLogicalHeight);
        marginAfterAlias = marginAfter.calcValue(containerLogicalHeight);
        logicalTopValue = logicalTop.calcValue(containerLogicalHeight);

        // Solve for 'bottom'
        // NOTE: It is not necessary to solve for 'bottom' because we don't ever
        // use the value.
    } else if (marginBefore.isAuto()) {
        marginAfterAlias = marginAfter.calcValue(containerLogicalHeight);
        logicalTopValue = logicalTop.calcValue(containerLogicalHeight);
        logicalBottomValue = logicalBottom.calcValue(containerLogicalHeight);

        // Solve for 'margin-top'
        marginBeforeAlias = availableSpace - (logicalTopValue + logicalBottomValue + marginAfterAlias);
    } else if (marginAfter.isAuto()) {
        marginBeforeAlias = marginBefore.calcValue(containerLogicalHeight);
        logicalTopValue = logicalTop.calcValue(containerLogicalHeight);
        logicalBottomValue = logicalBottom.calcValue(containerLogicalHeight);

        // Solve for 'margin-bottom'
        marginAfterAlias = availableSpace - (logicalTopValue + logicalBottomValue + marginBeforeAlias);
    } else {
        // Nothing is 'auto', just calculate the values.
        marginBeforeAlias = marginBefore.calcValue(containerLogicalHeight);
        marginAfterAlias = marginAfter.calcValue(containerLogicalHeight);
        logicalTopValue = logicalTop.calcValue(containerLogicalHeight);
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
    LayoutUnit logicalTopPos = logicalTopValue + marginBeforeAlias;
    computeLogicalTopPositionedOffset(logicalTopPos, this, logicalHeight(), containerBlock, containerLogicalHeight);
    setLogicalTop(logicalTopPos);
}

LayoutRect RenderBox::localCaretRect(InlineBox* box, int caretOffset, LayoutUnit* extraWidthToEndOfLine)
{
    // VisiblePositions at offsets inside containers either a) refer to the positions before/after
    // those containers (tables and select elements) or b) refer to the position inside an empty block.
    // They never refer to children.
    // FIXME: Paint the carets inside empty blocks differently than the carets before/after elements.

    // FIXME: What about border and padding?
    LayoutRect rect(x(), y(), caretWidth, height());
    bool ltr = box ? box->isLeftToRightDirection() : style()->isLeftToRightDirection();

    if ((!caretOffset) ^ ltr)
        rect.move(LayoutSize(width() - caretWidth, 0));

    if (box) {
        RootInlineBox* rootBox = box->root();
        LayoutUnit top = rootBox->lineTop();
        rect.setY(top);
        rect.setHeight(rootBox->lineBottom() - top);
    }

    // If height of box is smaller than font height, use the latter one,
    // otherwise the caret might become invisible.
    //
    // Also, if the box is not a replaced element, always use the font height.
    // This prevents the "big caret" bug described in:
    // <rdar://problem/3777804> Deleting all content in a document can result in giant tall-as-window insertion point
    //
    // FIXME: ignoring :first-line, missing good reason to take care of
    LayoutUnit fontHeight = style()->fontMetrics().height();
    if (fontHeight > rect.height() || (!isReplaced() && !isTable()))
        rect.setHeight(fontHeight);

    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = x() + width() - rect.maxX();

    // Move to local coords
    rect.moveBy(-location());
    return rect;
}

VisiblePosition RenderBox::positionForPoint(const LayoutPoint& point)
{
    // no children...return this render object's element, if there is one, and offset 0
    if (!firstChild())
        return createVisiblePosition(node() ? firstPositionInOrBeforeNode(node()) : Position());

    if (isTable() && node()) {
        LayoutUnit right = contentWidth() + borderAndPaddingWidth();
        LayoutUnit bottom = contentHeight() + borderAndPaddingHeight();
        
        if (point.x() < 0 || point.x() > right || point.y() < 0 || point.y() > bottom) {
            if (point.x() <= right / 2)
                return createVisiblePosition(firstPositionInOrBeforeNode(node()));
            return createVisiblePosition(lastPositionInOrAfterNode(node()));
        }
    }

    // Pass off to the closest child.
    LayoutUnit minDist = numeric_limits<LayoutUnit>::max();
    RenderBox* closestRenderer = 0;
    LayoutPoint adjustedPoint = point;
    if (isTableRow())
        adjustedPoint.moveBy(location());

    for (RenderObject* renderObject = firstChild(); renderObject; renderObject = renderObject->nextSibling()) {
        if ((!renderObject->firstChild() && !renderObject->isInline() && !renderObject->isBlockFlow() )
            || renderObject->style()->visibility() != VISIBLE)
            continue;
        
        if (!renderObject->isBox())
            continue;
        
        RenderBox* renderer = toRenderBox(renderObject);

        LayoutUnit top = renderer->borderTop() + renderer->paddingTop() + (isTableRow() ? 0 : renderer->y());
        LayoutUnit bottom = top + renderer->contentHeight();
        LayoutUnit left = renderer->borderLeft() + renderer->paddingLeft() + (isTableRow() ? 0 : renderer->x());
        LayoutUnit right = left + renderer->contentWidth();
        
        if (point.x() <= right && point.x() >= left && point.y() <= top && point.y() >= bottom) {
            if (renderer->isTableRow())
                return renderer->positionForPoint(point + adjustedPoint - renderer->locationOffset());
            return renderer->positionForPoint(point - renderer->locationOffset());
        }

        // Find the distance from (x, y) to the box.  Split the space around the box into 8 pieces
        // and use a different compare depending on which piece (x, y) is in.
        LayoutPoint cmp;
        if (point.x() > right) {
            if (point.y() < top)
                cmp = LayoutPoint(right, top);
            else if (point.y() > bottom)
                cmp = LayoutPoint(right, bottom);
            else
                cmp = LayoutPoint(right, point.y());
        } else if (point.x() < left) {
            if (point.y() < top)
                cmp = LayoutPoint(left, top);
            else if (point.y() > bottom)
                cmp = LayoutPoint(left, bottom);
            else
                cmp = LayoutPoint(left, point.y());
        } else {
            if (point.y() < top)
                cmp = LayoutPoint(point.x(), top);
            else
                cmp = LayoutPoint(point.x(), bottom);
        }

        LayoutSize difference = cmp - point;

        LayoutUnit dist = difference.width() * difference.width() + difference.height() * difference.height();
        if (dist < minDist) {
            closestRenderer = renderer;
            minDist = dist;
        }
    }
    
    if (closestRenderer)
        return closestRenderer->positionForPoint(adjustedPoint - closestRenderer->locationOffset());
    
    return createVisiblePosition(firstPositionInOrBeforeNode(node()));
}

bool RenderBox::shrinkToAvoidFloats() const
{
    // Floating objects don't shrink.  Objects that don't avoid floats don't shrink.  Marquees don't shrink.
    if ((isInline() && !isHTMLMarquee()) || !avoidsFloats() || isFloating())
        return false;

    // All auto-width objects that avoid floats should always use lineWidth.
    return style()->width().isAuto(); 
}

bool RenderBox::avoidsFloats() const
{
    return isReplaced() || hasOverflowClip() || isHR() || isLegend() || isWritingModeRoot() || isDeprecatedFlexItem();
}

void RenderBox::addBoxShadowAndBorderOverflow()
{
    if (!style()->boxShadow() && !style()->hasBorderImageOutsets())
        return;

    bool isFlipped = style()->isFlippedBlocksWritingMode();
    bool isHorizontal = isHorizontalWritingMode();
    
    LayoutRect borderBox = borderBoxRect();
    LayoutUnit overflowMinX = borderBox.x();
    LayoutUnit overflowMaxX = borderBox.maxX();
    LayoutUnit overflowMinY = borderBox.y();
    LayoutUnit overflowMaxY = borderBox.maxY();
    
    // Compute box-shadow overflow first.
    if (style()->boxShadow()) {
        LayoutUnit shadowLeft;
        LayoutUnit shadowRight;
        LayoutUnit shadowTop;
        LayoutUnit shadowBottom;
        style()->getBoxShadowExtent(shadowTop, shadowRight, shadowBottom, shadowLeft);

        // In flipped blocks writing modes such as vertical-rl, the physical right shadow value is actually at the lower x-coordinate.
        overflowMinX = borderBox.x() + ((!isFlipped || isHorizontal) ? shadowLeft : -shadowRight);
        overflowMaxX = borderBox.maxX() + ((!isFlipped || isHorizontal) ? shadowRight : -shadowLeft);
        overflowMinY = borderBox.y() + ((!isFlipped || !isHorizontal) ? shadowTop : -shadowBottom);
        overflowMaxY = borderBox.maxY() + ((!isFlipped || !isHorizontal) ? shadowBottom : -shadowTop);
    }

    // Now compute border-image-outset overflow.
    if (style()->hasBorderImageOutsets()) {
        LayoutUnit borderOutsetLeft;
        LayoutUnit borderOutsetRight;
        LayoutUnit borderOutsetTop;
        LayoutUnit borderOutsetBottom;
        style()->getBorderImageOutsets(borderOutsetTop, borderOutsetRight, borderOutsetBottom, borderOutsetLeft);
        
        // In flipped blocks writing modes, the physical sides are inverted. For example in vertical-rl, the right
        // border is at the lower x coordinate value.
        overflowMinX = min(overflowMinX, borderBox.x() - ((!isFlipped || isHorizontal) ? borderOutsetLeft : borderOutsetRight));
        overflowMaxX = max(overflowMaxX, borderBox.maxX() + ((!isFlipped || isHorizontal) ? borderOutsetRight : borderOutsetLeft));
        overflowMinY = min(overflowMinY, borderBox.y() - ((!isFlipped || !isHorizontal) ? borderOutsetTop : borderOutsetBottom));
        overflowMaxY = max(overflowMaxY, borderBox.maxY() + ((!isFlipped || !isHorizontal) ? borderOutsetBottom : borderOutsetTop));
    }

    // Add in the final overflow with shadows and outsets combined.
    addVisualOverflow(LayoutRect(overflowMinX, overflowMinY, overflowMaxX - overflowMinX, overflowMaxY - overflowMinY));
}

void RenderBox::addOverflowFromChild(RenderBox* child, const LayoutSize& delta)
{
    // Only propagate layout overflow from the child if the child isn't clipping its overflow.  If it is, then
    // its overflow is internal to it, and we don't care about it.  layoutOverflowRectForPropagation takes care of this
    // and just propagates the border box rect instead.
    LayoutRect childLayoutOverflowRect = child->layoutOverflowRectForPropagation(style());
    childLayoutOverflowRect.move(delta);
    addLayoutOverflow(childLayoutOverflowRect);
            
    // Add in visual overflow from the child.  Even if the child clips its overflow, it may still
    // have visual overflow of its own set from box shadows or reflections.  It is unnecessary to propagate this
    // overflow if we are clipping our own overflow.
    if (child->hasSelfPaintingLayer() || hasOverflowClip())
        return;
    LayoutRect childVisualOverflowRect = child->visualOverflowRectForPropagation(style());
    childVisualOverflowRect.move(delta);
    addVisualOverflow(childVisualOverflowRect);
}

void RenderBox::addLayoutOverflow(const LayoutRect& rect)
{
    LayoutRect clientBox = clientBoxRect();
    if (clientBox.contains(rect) || rect.isEmpty())
        return;
    
    // For overflow clip objects, we don't want to propagate overflow into unreachable areas.
    LayoutRect overflowRect(rect);
    if (hasOverflowClip() || isRenderView()) {
        // Overflow is in the block's coordinate space and thus is flipped for horizontal-bt and vertical-rl 
        // writing modes.  At this stage that is actually a simplification, since we can treat horizontal-tb/bt as the same
        // and vertical-lr/rl as the same.
        bool hasTopOverflow = !style()->isLeftToRightDirection() && !isHorizontalWritingMode();
        bool hasLeftOverflow = !style()->isLeftToRightDirection() && isHorizontalWritingMode();
        
        if (!hasTopOverflow)
            overflowRect.shiftYEdgeTo(max(overflowRect.y(), clientBox.y()));
        else
            overflowRect.shiftMaxYEdgeTo(min(overflowRect.maxY(), clientBox.maxY()));
        if (!hasLeftOverflow)
            overflowRect.shiftXEdgeTo(max(overflowRect.x(), clientBox.x()));
        else
            overflowRect.shiftMaxXEdgeTo(min(overflowRect.maxX(), clientBox.maxX()));
        
        // Now re-test with the adjusted rectangle and see if it has become unreachable or fully
        // contained.
        if (clientBox.contains(overflowRect) || overflowRect.isEmpty())
            return;
    }

    if (!m_overflow)
        m_overflow = adoptPtr(new RenderOverflow(clientBox, borderBoxRect()));
    
    m_overflow->addLayoutOverflow(overflowRect);
}

void RenderBox::addVisualOverflow(const LayoutRect& rect)
{
    LayoutRect borderBox = borderBoxRect();
    if (borderBox.contains(rect) || rect.isEmpty())
        return;
        
    if (!m_overflow)
        m_overflow = adoptPtr(new RenderOverflow(clientBoxRect(), borderBox));
    
    m_overflow->addVisualOverflow(rect);
}

void RenderBox::clearLayoutOverflow()
{
    if (!m_overflow)
        return;
    
    if (visualOverflowRect() == borderBoxRect()) {
        m_overflow.clear();
        return;
    }
    
    m_overflow->resetLayoutOverflow(borderBoxRect());
}

static bool percentageLogicalHeightIsResolvable(const RenderBox* box)
{
    // In quirks mode, blocks with auto height are skipped, and we keep looking for an enclosing
    // block that may have a specified height and then use it. In strict mode, this violates the
    // specification, which states that percentage heights just revert to auto if the containing
    // block has an auto height. We still skip anonymous containing blocks in both modes, though, and look
    // only at explicit containers.
    const RenderBlock* cb = box->containingBlock();
    while (!cb->isRenderView() && !cb->isBody() && !cb->isTableCell() && !cb->isPositioned() && cb->style()->logicalHeight().isAuto()) {
        if (!box->document()->inQuirksMode() && !cb->isAnonymousBlock())
            break;
        cb = cb->containingBlock();
    }

    // A positioned element that specified both top/bottom or that specifies height should be treated as though it has a height
    // explicitly specified that can be used for any percentage computations.
    // FIXME: We can't just check top/bottom here.
    // https://bugs.webkit.org/show_bug.cgi?id=46500
    bool isPositionedWithSpecifiedHeight = cb->isPositioned() && (!cb->style()->logicalHeight().isAuto() || (!cb->style()->top().isAuto() && !cb->style()->bottom().isAuto()));

    // Table cells violate what the CSS spec says to do with heights.  Basically we
    // don't care if the cell specified a height or not.  We just always make ourselves
    // be a percentage of the cell's current content height.
    if (cb->isTableCell())
        return true;

    // Otherwise we only use our percentage height if our containing block had a specified
    // height.
    if (cb->style()->logicalHeight().isFixed())
        return true;
    if (cb->style()->logicalHeight().isPercent() && !isPositionedWithSpecifiedHeight)
        return percentageLogicalHeightIsResolvable(cb);
    if (cb->isRenderView() || (cb->isBody() && box->document()->inQuirksMode()) || isPositionedWithSpecifiedHeight)
        return true;
    if (cb->isRoot() && box->isPositioned()) {
        // Match the positioned objects behavior, which is that positioned objects will fill their viewport
        // always.  Note we could only hit this case by recurring into computePercentageLogicalHeight on a positioned containing block.
        return true;
    }

    return false;
}

bool RenderBox::hasUnsplittableScrollingOverflow() const
{
    // We will paginate as long as we don't scroll overflow in the pagination direction.
    bool isHorizontal = isHorizontalWritingMode();
    if ((isHorizontal && !scrollsOverflowY()) || (!isHorizontal && !scrollsOverflowX()))
        return false;
    
    // We do have overflow. We'll still be willing to paginate as long as the block
    // has auto logical height, auto or undefined max-logical-height and a zero or auto min-logical-height.
    // Note this is just a heuristic, and it's still possible to have overflow under these
    // conditions, but it should work out to be good enough for common cases. Paginating overflow
    // with scrollbars present is not the end of the world and is what we used to do in the old model anyway.
    return !style()->logicalHeight().isIntrinsicOrAuto()
        || (!style()->logicalMaxHeight().isIntrinsicOrAuto() && !style()->logicalMaxHeight().isUndefined() && (!style()->logicalMaxHeight().isPercent() || percentageLogicalHeightIsResolvable(this)))
        || (!style()->logicalMinHeight().isIntrinsicOrAuto() && style()->logicalMinHeight().isPositive() && (!style()->logicalMinHeight().isPercent() || percentageLogicalHeightIsResolvable(this)));
}

bool RenderBox::isUnsplittableForPagination() const
{
    return isReplaced() || hasUnsplittableScrollingOverflow() || (parent() && isWritingModeRoot());
}

LayoutUnit RenderBox::lineHeight(bool /*firstLine*/, LineDirectionMode direction, LinePositionMode /*linePositionMode*/) const
{
    if (isReplaced())
        return direction == HorizontalLine ? m_marginTop + height() + m_marginBottom : m_marginRight + width() + m_marginLeft;
    return 0;
}

LayoutUnit RenderBox::baselinePosition(FontBaseline baselineType, bool /*firstLine*/, LineDirectionMode direction, LinePositionMode /*linePositionMode*/) const
{
    if (isReplaced()) {
        LayoutUnit result = direction == HorizontalLine ? m_marginTop + height() + m_marginBottom : m_marginRight + width() + m_marginLeft;
        if (baselineType == AlphabeticBaseline)
            return result;
        return result - result / 2;
    }
    return 0;
}


RenderLayer* RenderBox::enclosingFloatPaintingLayer() const
{
    const RenderObject* curr = this;
    while (curr) {
        RenderLayer* layer = curr->hasLayer() && curr->isBox() ? toRenderBoxModelObject(curr)->layer() : 0;
        if (layer && layer->isSelfPaintingLayer())
            return layer;
        curr = curr->parent();
    }
    return 0;
}

LayoutRect RenderBox::logicalVisualOverflowRectForPropagation(RenderStyle* parentStyle) const
{
    LayoutRect rect = visualOverflowRectForPropagation(parentStyle);
    if (!parentStyle->isHorizontalWritingMode())
        return rect.transposedRect();
    return rect;
}

LayoutRect RenderBox::visualOverflowRectForPropagation(RenderStyle* parentStyle) const
{
    // If the writing modes of the child and parent match, then we don't have to 
    // do anything fancy. Just return the result.
    LayoutRect rect = visualOverflowRect();
    if (parentStyle->writingMode() == style()->writingMode())
        return rect;
    
    // We are putting ourselves into our parent's coordinate space.  If there is a flipped block mismatch
    // in a particular axis, then we have to flip the rect along that axis.
    if (style()->writingMode() == RightToLeftWritingMode || parentStyle->writingMode() == RightToLeftWritingMode)
        rect.setX(width() - rect.maxX());
    else if (style()->writingMode() == BottomToTopWritingMode || parentStyle->writingMode() == BottomToTopWritingMode)
        rect.setY(height() - rect.maxY());

    return rect;
}

LayoutRect RenderBox::logicalLayoutOverflowRectForPropagation(RenderStyle* parentStyle) const
{
    LayoutRect rect = layoutOverflowRectForPropagation(parentStyle);
    if (!parentStyle->isHorizontalWritingMode())
        return rect.transposedRect();
    return rect;
}

LayoutRect RenderBox::layoutOverflowRectForPropagation(RenderStyle* parentStyle) const
{
    // Only propagate interior layout overflow if we don't clip it.
    LayoutRect rect = borderBoxRect();
    if (!hasOverflowClip())
        rect.unite(layoutOverflowRect());

    bool hasTransform = hasLayer() && layer()->transform();
    if (isRelPositioned() || hasTransform) {
        // If we are relatively positioned or if we have a transform, then we have to convert
        // this rectangle into physical coordinates, apply relative positioning and transforms
        // to it, and then convert it back.
        flipForWritingMode(rect);
        
        if (hasTransform)
            rect = layer()->currentTransform().mapRect(rect);

        if (isRelPositioned())
            rect.move(relativePositionOffsetX(), relativePositionOffsetY());
        
        // Now we need to flip back.
        flipForWritingMode(rect);
    }
    
    // If the writing modes of the child and parent match, then we don't have to 
    // do anything fancy. Just return the result.
    if (parentStyle->writingMode() == style()->writingMode())
        return rect;
    
    // We are putting ourselves into our parent's coordinate space.  If there is a flipped block mismatch
    // in a particular axis, then we have to flip the rect along that axis.
    if (style()->writingMode() == RightToLeftWritingMode || parentStyle->writingMode() == RightToLeftWritingMode)
        rect.setX(width() - rect.maxX());
    else if (style()->writingMode() == BottomToTopWritingMode || parentStyle->writingMode() == BottomToTopWritingMode)
        rect.setY(height() - rect.maxY());

    return rect;
}

LayoutPoint RenderBox::flipForWritingModeForChild(const RenderBox* child, const LayoutPoint& point) const
{
    if (!style()->isFlippedBlocksWritingMode())
        return point;
    
    // The child is going to add in its x() and y(), so we have to make sure it ends up in
    // the right place.
    if (isHorizontalWritingMode())
        return LayoutPoint(point.x(), point.y() + height() - child->height() - (2 * child->y()));
    return LayoutPoint(point.x() + width() - child->width() - (2 * child->x()), point.y());
}

void RenderBox::flipForWritingMode(IntRect& rect) const
{
    if (!style()->isFlippedBlocksWritingMode())
        return;

    if (isHorizontalWritingMode())
        rect.setY(height() - rect.maxY());
    else
        rect.setX(width() - rect.maxX());
}

int RenderBox::flipForWritingMode(int position) const
{
    if (!style()->isFlippedBlocksWritingMode())
        return position;
    return logicalHeight() - position;
}

IntPoint RenderBox::flipForWritingMode(const IntPoint& position) const
{
    if (!style()->isFlippedBlocksWritingMode())
        return position;
    return isHorizontalWritingMode() ? IntPoint(position.x(), height() - position.y()) : IntPoint(width() - position.x(), position.y());
}

LayoutPoint RenderBox::flipForWritingModeIncludingColumns(const LayoutPoint& point) const
{
    if (!hasColumns() || !style()->isFlippedBlocksWritingMode())
        return flipForWritingMode(point);
    return toRenderBlock(this)->flipForWritingModeIncludingColumns(point);
}

IntSize RenderBox::flipForWritingMode(const IntSize& offset) const
{
    if (!style()->isFlippedBlocksWritingMode())
        return offset;
    return isHorizontalWritingMode() ? IntSize(offset.width(), height() - offset.height()) : IntSize(width() - offset.width(), offset.height());
}

FloatPoint RenderBox::flipForWritingMode(const FloatPoint& position) const
{
    if (!style()->isFlippedBlocksWritingMode())
        return position;
    return isHorizontalWritingMode() ? FloatPoint(position.x(), height() - position.y()) : FloatPoint(width() - position.x(), position.y());
}

void RenderBox::flipForWritingMode(FloatRect& rect) const
{
    if (!style()->isFlippedBlocksWritingMode())
        return;

    if (isHorizontalWritingMode())
        rect.setY(height() - rect.maxY());
    else
        rect.setX(width() - rect.maxX());
}

LayoutPoint RenderBox::topLeftLocation() const
{
    RenderBlock* containerBlock = containingBlock();
    if (!containerBlock || containerBlock == this)
        return location();
    return containerBlock->flipForWritingModeForChild(this, location());
}

LayoutSize RenderBox::topLeftLocationOffset() const
{
    RenderBlock* containerBlock = containingBlock();
    if (!containerBlock || containerBlock == this)
        return locationOffset();
    
    LayoutRect rect(frameRect());
    containerBlock->flipForWritingMode(rect); // FIXME: This is wrong if we are an absolutely positioned object enclosed by a relative-positioned inline.
    return LayoutSize(rect.x(), rect.y());
}

} // namespace WebCore
