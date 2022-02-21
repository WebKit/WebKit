/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005-2010, 2015 Apple Inc. All rights reserved.
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

#include "CSSFontSelector.h"
#include "ControlStates.h"
#include "Document.h"
#include "Editing.h"
#include "EventHandler.h"
#include "FloatQuad.h"
#include "FloatRoundedRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLBodyElement.h"
#include "HTMLButtonElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLegendElement.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "HitTestResult.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLine.h"
#include "InlineRunAndOffset.h"
#include "LayoutIntegrationLineLayout.h"
#include "LegacyInlineElementBox.h"
#include "Page.h"
#include "PaintInfo.h"
#include "PathOperation.h"
#include "RenderBoxFragmentInfo.h"
#include "RenderChildIterator.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderFlexibleBox.h"
#include "RenderFragmentContainer.h"
#include "RenderGeometryMap.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerScrollableArea.h"
#include "RenderLayoutState.h"
#include "RenderMultiColumnFlow.h"
#include "RenderSVGResourceClipper.h"
#include "RenderTableCell.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RuntimeApplicationChecks.h"
#include "SVGClipPathElement.h"
#include "SVGElementTypeHelpers.h"
#include "ScrollAnimator.h"
#include "ScrollbarTheme.h"
#include "Settings.h"
#include "StyleScrollSnapPoints.h"
#include "TextDirection.h"
#include "TransformState.h"
#include <algorithm>
#include <math.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderBox);

struct SameSizeAsRenderBox : public RenderBoxModelObject {
    virtual ~SameSizeAsRenderBox() = default;
    LayoutRect frameRect;
    LayoutBoxExtent marginBox;
    LayoutUnit preferredLogicalWidths[2];
    void* pointers[2];
};

COMPILE_ASSERT(sizeof(RenderBox) == sizeof(SameSizeAsRenderBox), RenderBox_should_stay_small);

using namespace HTMLNames;

typedef HashMap<const RenderBox*, LayoutUnit> OverrideSizeMap;
static OverrideSizeMap* gOverridingLogicalHeightMap = nullptr;
static OverrideSizeMap* gOverridingLogicalWidthMap = nullptr;

typedef HashMap<const RenderBox*, Length> OverridingLengthMap;
static OverridingLengthMap* gOverridingLogicalHeightLengthMap = nullptr;
static OverridingLengthMap* gOverridingLogicalWidthLengthMap = nullptr;

// FIXME: We should store these based on physical direction.
typedef HashMap<const RenderBox*, std::optional<LayoutUnit>> OverrideOptionalSizeMap;
static OverrideOptionalSizeMap* gOverridingContainingBlockContentLogicalHeightMap = nullptr;
static OverrideOptionalSizeMap* gOverridingContainingBlockContentLogicalWidthMap = nullptr;

// Size of border belt for autoscroll. When mouse pointer in border belt,
// autoscroll is started.
static const int autoscrollBeltSize = 20;
static const unsigned backgroundObscurationTestMaxDepth = 4;

using ControlStatesRendererMap = HashMap<const RenderObject*, std::unique_ptr<ControlStates>>;
static ControlStatesRendererMap& controlStatesRendererMap()
{
    static NeverDestroyed<ControlStatesRendererMap> map;
    return map;
}

static ControlStates* controlStatesForRenderer(const RenderBox& renderer)
{
    return controlStatesRendererMap().ensure(&renderer, [] {
        return makeUnique<ControlStates>();
    }).iterator->value.get();
}

static void removeControlStatesForRenderer(const RenderBox& renderer)
{
    controlStatesRendererMap().remove(&renderer);
}

bool RenderBox::s_hadNonVisibleOverflow = false;

RenderBox::RenderBox(Element& element, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : RenderBoxModelObject(element, WTFMove(style), baseTypeFlags)
{
    setIsBox();
}

RenderBox::RenderBox(Document& document, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : RenderBoxModelObject(document, WTFMove(style), baseTypeFlags)
{
    setIsBox();
}

RenderBox::~RenderBox()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderBox::willBeDestroyed()
{
    if (frame().eventHandler().autoscrollRenderer() == this)
        frame().eventHandler().stopAutoscrollTimer(true);

    clearOverridingContentSize();
    clearOverridingContainingBlockContentSize();

    RenderBlock::removePercentHeightDescendantIfNeeded(*this);

    ShapeOutsideInfo::removeInfo(*this);

    view().unscheduleLazyRepaint(*this);
    removeControlStatesForRenderer(*this);

    if (hasInitializedStyle()) {
        if (style().hasSnapPosition())
            view().unregisterBoxWithScrollSnapPositions(*this);
        if (style().containerType() != ContainerType::None)
            view().unregisterContainerQueryBox(*this);
    }

    RenderBoxModelObject::willBeDestroyed();
}

RenderFragmentContainer* RenderBox::clampToStartAndEndFragments(RenderFragmentContainer* fragment) const
{
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();

    ASSERT(isRenderView() || (fragment && fragmentedFlow));
    if (isRenderView())
        return fragment;

    // We need to clamp to the block, since we want any lines or blocks that overflow out of the
    // logical top or logical bottom of the block to size as though the border box in the first and
    // last fragments extended infinitely. Otherwise the lines are going to size according to the fragments
    // they overflow into, which makes no sense when this block doesn't exist in |fragment| at all.
    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!fragmentedFlow->getFragmentRangeForBox(this, startFragment, endFragment))
        return fragment;

    if (fragment->logicalTopForFragmentedFlowContent() < startFragment->logicalTopForFragmentedFlowContent())
        return startFragment;
    if (fragment->logicalTopForFragmentedFlowContent() > endFragment->logicalTopForFragmentedFlowContent())
        return endFragment;

    return fragment;
}

bool RenderBox::hasFragmentRangeInFragmentedFlow() const
{
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (!fragmentedFlow || !fragmentedFlow->hasValidFragmentInfo())
        return false;

    return fragmentedFlow->hasCachedFragmentRangeForBox(*this);
}

LayoutRect RenderBox::clientBoxRectInFragment(RenderFragmentContainer* fragment) const
{
    if (!fragment)
        return clientBoxRect();

    LayoutRect clientBox = borderBoxRectInFragment(fragment);
    clientBox.setLocation(clientBox.location() + LayoutSize(borderLeft(), borderTop()));
    clientBox.setSize(clientBox.size() - LayoutSize(borderLeft() + borderRight() + verticalScrollbarWidth(), borderTop() + borderBottom() + horizontalScrollbarHeight()));

    return clientBox;
}

LayoutRect RenderBox::borderBoxRectInFragment(RenderFragmentContainer*, RenderBoxFragmentInfoFlags) const
{
    return borderBoxRect();
}

static RenderBlockFlow* outermostBlockContainingFloatingObject(RenderBox& box)
{
    ASSERT(box.isFloating());
    RenderBlockFlow* parentBlock = nullptr;
    for (auto& ancestor : ancestorsOfType<RenderBlockFlow>(box)) {
        if (!parentBlock || ancestor.containsFloat(box))
            parentBlock = &ancestor;
    }
    return parentBlock;
}

void RenderBox::removeFloatingOrPositionedChildFromBlockLists()
{
    ASSERT(isFloatingOrOutOfFlowPositioned());

    if (renderTreeBeingDestroyed())
        return;

    if (isFloating()) {
        if (RenderBlockFlow* parentBlock = outermostBlockContainingFloatingObject(*this)) {
            parentBlock->markSiblingsWithFloatsForLayout(this);
            parentBlock->markAllDescendantsWithFloatsForLayout(this, false);
        }
    }

    if (isOutOfFlowPositioned())
        RenderBlock::removePositionedObject(*this);
}

void RenderBox::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    s_hadNonVisibleOverflow = hasNonVisibleOverflow();

    const RenderStyle* oldStyle = hasInitializedStyle() ? &style() : nullptr;
    if (oldStyle) {
        // The background of the root element or the body element could propagate up to
        // the canvas. Issue full repaint, when our style changes substantially.
        if (diff >= StyleDifference::Repaint && (isDocumentElementRenderer() || isBody())) {
            view().repaintRootContents();
            if (oldStyle->hasEntirelyFixedBackground() != newStyle.hasEntirelyFixedBackground())
                view().compositor().rootLayerConfigurationChanged();
        }
        
        // When a layout hint happens and an object's position style changes, we have to do a layout
        // to dirty the render tree using the old position value now.
        if (diff == StyleDifference::Layout && parent() && oldStyle->position() != newStyle.position()) {
            markContainingBlocksForLayout();
            if (oldStyle->position() != PositionType::Static && newStyle.hasOutOfFlowPosition())
                parent()->setChildNeedsLayout();
            if (isFloating() && !isOutOfFlowPositioned() && newStyle.hasOutOfFlowPosition())
                removeFloatingOrPositionedChildFromBlockLists();
        }
    } else if (isBody())
        view().repaintRootContents();

    bool boxContributesSnapPositions = newStyle.hasSnapPosition();
    if (boxContributesSnapPositions || (oldStyle && oldStyle->hasSnapPosition())) {
        if (boxContributesSnapPositions)
            view().registerBoxWithScrollSnapPositions(*this);
        else
            view().unregisterBoxWithScrollSnapPositions(*this);
    }

    if (newStyle.containerType() != ContainerType::None)
        view().registerContainerQueryBox(*this);
    else if (oldStyle && oldStyle->containerType() != ContainerType::None)
        view().unregisterContainerQueryBox(*this);

    RenderBoxModelObject::styleWillChange(diff, newStyle);
}

void RenderBox::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    // Horizontal writing mode definition is updated in RenderBoxModelObject::updateFromStyle,
    // (as part of the RenderBoxModelObject::styleDidChange call below). So, we can safely cache the horizontal
    // writing mode value before style change here.
    bool oldHorizontalWritingMode = isHorizontalWritingMode();

    RenderBoxModelObject::styleDidChange(diff, oldStyle);

    const RenderStyle& newStyle = style();
    if (needsLayout() && oldStyle) {
        RenderBlock::removePercentHeightDescendantIfNeeded(*this);

        // Normally we can do optimized positioning layout for absolute/fixed positioned objects. There is one special case, however, which is
        // when the positioned object's margin-before is changed. In this case the parent has to get a layout in order to run margin collapsing
        // to determine the new static position.
        if (isOutOfFlowPositioned() && newStyle.hasStaticBlockPosition(isHorizontalWritingMode()) && oldStyle->marginBefore() != newStyle.marginBefore()
            && parent() && !parent()->normalChildNeedsLayout())
            parent()->setChildNeedsLayout();
    }

    if (RenderBlock::hasPercentHeightContainerMap() && firstChild()
        && oldHorizontalWritingMode != isHorizontalWritingMode())
        RenderBlock::clearPercentHeightDescendantsFrom(*this);

    // If our zoom factor changes and we have a defined scrollLeft/Top, we need to adjust that value into the
    // new zoomed coordinate space.
    if (hasNonVisibleOverflow() && layer() && oldStyle && oldStyle->effectiveZoom() != newStyle.effectiveZoom()) {
        if (auto* scrollableArea = layer()->scrollableArea()) {
            ScrollPosition scrollPosition = scrollableArea->scrollPosition();
            float zoomScaleFactor = newStyle.effectiveZoom() / oldStyle->effectiveZoom();
            scrollPosition.scale(zoomScaleFactor);
            scrollableArea->setPostLayoutScrollPosition(scrollPosition);
        }
    }

    // Our opaqueness might have changed without triggering layout.
    if (diff >= StyleDifference::Repaint && diff <= StyleDifference::RepaintLayer) {
        auto parentToInvalidate = parent();
        for (unsigned i = 0; i < backgroundObscurationTestMaxDepth && parentToInvalidate; ++i) {
            parentToInvalidate->invalidateBackgroundObscurationStatus();
            parentToInvalidate = parentToInvalidate->parent();
        }
    }

    bool isBodyRenderer = isBody();
    bool isDocElementRenderer = isDocumentElementRenderer();

    if (isDocElementRenderer || isBodyRenderer) {
        auto* documentElementRenderer = document().documentElement()->renderer();
        auto& viewStyle = view().mutableStyle();
        bool rootStyleChanged = false;
        bool viewDirectionOrWritingModeChanged = false;
        auto* rootRenderer = isBodyRenderer ? documentElementRenderer : nullptr;

        auto propagateWritingModeToRenderViewIfApplicable = [&] {
            // Propagate the new writing mode and direction up to the RenderView.
            if (!documentElementRenderer)
                return;
            if (!isBodyRenderer || !(shouldApplyAnyContainment(*this) || shouldApplyAnyContainment(*documentElementRenderer))) {
                if (viewStyle.direction() != newStyle.direction() && (isDocElementRenderer || !documentElementRenderer->style().hasExplicitlySetDirection())) {
                    viewStyle.setDirection(newStyle.direction());
                    viewDirectionOrWritingModeChanged = true;
                    if (isBodyRenderer) {
                        rootRenderer->mutableStyle().setDirection(newStyle.direction());
                        rootStyleChanged = true;
                    }
                    setNeedsLayoutAndPrefWidthsRecalc();

                    view().frameView().topContentDirectionDidChange();
                }

                if (viewStyle.writingMode() != newStyle.writingMode() && (isDocElementRenderer || !documentElementRenderer->style().hasExplicitlySetWritingMode())) {
                    viewStyle.setWritingMode(newStyle.writingMode());
                    viewDirectionOrWritingModeChanged = true;
                    view().setHorizontalWritingMode(newStyle.isHorizontalWritingMode());
                    view().markAllDescendantsWithFloatsForLayout();
                    if (isBodyRenderer) {
                        rootStyleChanged = true;
                        rootRenderer->mutableStyle().setWritingMode(newStyle.writingMode());
                        rootRenderer->setHorizontalWritingMode(newStyle.isHorizontalWritingMode());
                    }
                    setNeedsLayoutAndPrefWidthsRecalc();
                }
            }
        };
        propagateWritingModeToRenderViewIfApplicable();

#if ENABLE(DARK_MODE_CSS)
        view().frameView().recalculateBaseBackgroundColor();
#endif

        view().frameView().recalculateScrollbarOverlayStyle();
        
        const Pagination& pagination = view().frameView().pagination();
        if (viewDirectionOrWritingModeChanged && pagination.mode != Pagination::Unpaginated) {
            viewStyle.setColumnStylesFromPaginationMode(pagination.mode);
            if (view().multiColumnFlow())
                view().updateColumnProgressionFromStyle(viewStyle);
        }
        
        if (viewDirectionOrWritingModeChanged && view().multiColumnFlow())
            view().updateStylesForColumnChildren();
        
        if (rootStyleChanged && is<RenderBlockFlow>(rootRenderer) && downcast<RenderBlockFlow>(*rootRenderer).multiColumnFlow())
            downcast<RenderBlockFlow>(*rootRenderer).updateStylesForColumnChildren();
        
        if (isBodyRenderer && pagination.mode != Pagination::Unpaginated && page().paginationLineGridEnabled()) {
            // Propagate the body font back up to the RenderView and use it as
            // the basis of the grid.
            if (newStyle.fontDescription() != view().style().fontDescription()) {
                view().mutableStyle().setFontDescription(FontCascadeDescription { newStyle.fontDescription() });
                view().mutableStyle().fontCascade().update(&document().fontSelector());
            }
        }

        if (diff != StyleDifference::Equal)
            view().compositor().rootOrBodyStyleChanged(*this, oldStyle);
    }

    if ((oldStyle && oldStyle->shapeOutside()) || style().shapeOutside())
        updateShapeOutsideInfoAfterStyleChange(style(), oldStyle);
    updateGridPositionAfterStyleChange(style(), oldStyle);

    // Changing the position from/to absolute can potentially create/remove flex/grid items, as absolutely positioned
    // children of a flex/grid box are out-of-flow, and thus, not flex/grid items. This means that we need to clear
    // any override content size set by our container, because it would likely be incorrect after the style change.
    if (isOutOfFlowPositioned() && parent() && parent()->style().isDisplayFlexibleBoxIncludingDeprecatedOrGridBox())
        clearOverridingContentSize();

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this))
        lineLayout->updateStyle(*this, *oldStyle);
#endif
}

void RenderBox::updateGridPositionAfterStyleChange(const RenderStyle& style, const RenderStyle* oldStyle)
{
    if (!oldStyle || !is<RenderGrid>(parent()))
        return;

    if (oldStyle->gridItemColumnStart() == style.gridItemColumnStart()
        && oldStyle->gridItemColumnEnd() == style.gridItemColumnEnd()
        && oldStyle->gridItemRowStart() == style.gridItemRowStart()
        && oldStyle->gridItemRowEnd() == style.gridItemRowEnd()
        && oldStyle->order() == style.order()
        && oldStyle->hasOutOfFlowPosition() == style.hasOutOfFlowPosition())
        return;

    // Positioned items don't participate on the layout of the grid,
    // so we don't need to mark the grid as dirty if they change positions.
    if (oldStyle->hasOutOfFlowPosition() && style.hasOutOfFlowPosition())
        return;

    // It should be possible to not dirty the grid in some cases (like moving an
    // explicitly placed grid item).
    // For now, it's more simple to just always recompute the grid.
    downcast<RenderGrid>(*parent()).dirtyGrid();
}

void RenderBox::updateShapeOutsideInfoAfterStyleChange(const RenderStyle& style, const RenderStyle* oldStyle)
{
    const ShapeValue* shapeOutside = style.shapeOutside();
    const ShapeValue* oldShapeOutside = oldStyle ? oldStyle->shapeOutside() : nullptr;

    Length shapeMargin = style.shapeMargin();
    Length oldShapeMargin = oldStyle ? oldStyle->shapeMargin() : RenderStyle::initialShapeMargin();

    float shapeImageThreshold = style.shapeImageThreshold();
    float oldShapeImageThreshold = oldStyle ? oldStyle->shapeImageThreshold() : RenderStyle::initialShapeImageThreshold();

    // FIXME: A future optimization would do a deep comparison for equality. (bug 100811)
    if (shapeOutside == oldShapeOutside && shapeMargin == oldShapeMargin && shapeImageThreshold == oldShapeImageThreshold)
        return;

    if (!shapeOutside)
        ShapeOutsideInfo::removeInfo(*this);
    else
        ShapeOutsideInfo::ensureInfo(*this).markShapeAsDirty();

    if (shapeOutside || shapeOutside != oldShapeOutside)
        markShapeOutsideDependentsForLayout();
}

void RenderBox::updateFromStyle()
{
    RenderBoxModelObject::updateFromStyle();

    const RenderStyle& styleToUse = style();
    bool isDocElementRenderer = isDocumentElementRenderer();
    bool isViewObject = isRenderView();

    // The root and the RenderView always paint their backgrounds/borders.
    if (isDocElementRenderer || isViewObject)
        setHasVisibleBoxDecorations(true);

    setFloating(!isOutOfFlowPositioned() && styleToUse.isFloating());

    // We also handle <body> and <html>, whose overflow applies to the viewport.
    if (!(effectiveOverflowX() == Overflow::Visible && effectiveOverflowY() == Overflow::Visible) && !isDocElementRenderer && isRenderBlock()) {
        bool boxHasNonVisibleOverflow = true;
        if (isBody()) {
            // Overflow on the body can propagate to the viewport under the following conditions.
            // (1) The root element is <html>.
            // (2) We are the primary <body> (can be checked by looking at document.body).
            // (3) The root element has visible overflow.
            if (is<HTMLHtmlElement>(*document().documentElement())
                && document().body() == element()
                && document().documentElement()->renderer()->effectiveOverflowX() == Overflow::Visible) {
                boxHasNonVisibleOverflow = false;
            }
        }
        // Check for overflow clip.
        // It's sufficient to just check one direction, since it's illegal to have visible on only one overflow value.
        if (boxHasNonVisibleOverflow) {
            if (!s_hadNonVisibleOverflow && hasRenderOverflow()) {
                // Erase the overflow.
                // Overflow changes have to result in immediate repaints of the entire layout overflow area because
                // repaints issued by removal of descendants get clipped using the updated style when they shouldn't.
                repaintRectangle(visualOverflowRect());
                repaintRectangle(layoutOverflowRect());
            }
            setHasNonVisibleOverflow();
        }
    }
    setHasTransformRelatedProperty(styleToUse.hasTransformRelatedProperty());
    setHasReflection(styleToUse.boxReflect());
}

void RenderBox::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    RenderObject* child = firstChild();
    if (!child) {
        clearNeedsLayout();
        return;
    }

    LayoutStateMaintainer statePusher(*this, locationOffset(), style().isFlippedBlocksWritingMode());
    while (child) {
        if (child->needsLayout())
            downcast<RenderElement>(*child).layout();
        ASSERT(!child->needsLayout());
        child = child->nextSibling();
    }
    invalidateBackgroundObscurationStatus();
    clearNeedsLayout();
}

// More IE extensions.  clientWidth and clientHeight represent the interior of an object
// excluding border and scrollbar.
LayoutUnit RenderBox::clientWidth() const
{
    return paddingBoxWidth();
}

LayoutUnit RenderBox::clientHeight() const
{
    return paddingBoxHeight();
}

int RenderBox::scrollWidth() const
{
    if (hasPotentiallyScrollableOverflow() && layer())
        return layer()->scrollWidth();
    // For objects with visible overflow, this matches IE.
    // FIXME: Need to work right with writing modes.
    if (style().isLeftToRightDirection()) {
        // FIXME: This should use snappedIntSize() instead with absolute coordinates.
        return roundToInt(std::max(clientWidth(), layoutOverflowRect().maxX() - borderLeft()));
    }
    return roundToInt(clientWidth() - std::min<LayoutUnit>(0, layoutOverflowRect().x() - borderLeft()));
}

int RenderBox::scrollHeight() const
{
    if (hasPotentiallyScrollableOverflow() && layer())
        return layer()->scrollHeight();
    // For objects with visible overflow, this matches IE.
    // FIXME: Need to work right with writing modes.
    // FIXME: This should use snappedIntSize() instead with absolute coordinates.
    return roundToInt(std::max(clientHeight(), layoutOverflowRect().maxY() - borderTop()));
}

int RenderBox::scrollLeft() const
{
    auto* scrollableArea = layer() ? layer()->scrollableArea() : nullptr;
    return (hasNonVisibleOverflow() && scrollableArea) ? scrollableArea->scrollPosition().x() : 0;
}

int RenderBox::scrollTop() const
{
    auto* scrollableArea = layer() ? layer()->scrollableArea() : nullptr;
    return (hasNonVisibleOverflow() && scrollableArea) ? scrollableArea->scrollPosition().y() : 0;
}

void RenderBox::resetLogicalHeightBeforeLayoutIfNeeded()
{
    if (shouldResetLogicalHeightBeforeLayout() || (is<RenderBlock>(parent()) && downcast<RenderBlock>(*parent()).shouldResetChildLogicalHeightBeforeLayout(*this)))
        setLogicalHeight(0_lu);
}

static void setupWheelEventMonitor(RenderLayerScrollableArea& scrollableArea)
{
    Page& page = scrollableArea.layer().renderer().page();
    if (!page.isMonitoringWheelEvents())
        return;
    scrollableArea.scrollAnimator().setWheelEventTestMonitor(page.wheelEventTestMonitor());
}

void RenderBox::setScrollLeft(int newLeft, const ScrollPositionChangeOptions& options)
{
    if (!hasPotentiallyScrollableOverflow() || !layer())
        return;
    auto* scrollableArea = layer()->scrollableArea();
    ASSERT(scrollableArea);
    setupWheelEventMonitor(*scrollableArea);
    scrollableArea->scrollToXPosition(newLeft, options);
}

void RenderBox::setScrollTop(int newTop, const ScrollPositionChangeOptions& options)
{
    if (!hasPotentiallyScrollableOverflow() || !layer())
        return;
    auto* scrollableArea = layer()->scrollableArea();
    ASSERT(scrollableArea);
    setupWheelEventMonitor(*scrollableArea);
    scrollableArea->scrollToYPosition(newTop, options);
}

void RenderBox::setScrollPosition(const ScrollPosition& position, const ScrollPositionChangeOptions& options)
{
    if (!hasPotentiallyScrollableOverflow() || !layer())
        return;
    auto* scrollableArea = layer()->scrollableArea();
    ASSERT(scrollableArea);
    setupWheelEventMonitor(*scrollableArea);
    scrollableArea->setScrollPosition(position, options);
}

void RenderBox::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    rects.append(snappedIntRect(accumulatedOffset, size()));
}

void RenderBox::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (fragmentedFlow && fragmentedFlow->absoluteQuadsForBox(quads, wasFixed, this))
        return;

    auto localRect = FloatRect { 0, 0, width(), height() };
    quads.append(localToAbsoluteQuad(localRect, UseTransforms, wasFixed));
}

void RenderBox::updateLayerTransform()
{
    // Transform-origin depends on box size, so we need to update the layer transform after layout.
    if (hasLayer())
        layer()->updateTransform();
}

void RenderBox::applyTransform(TransformationMatrix& t, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    style().applyTransform(t, boundingBox, options);
}

LayoutUnit RenderBox::constrainLogicalWidthInFragmentByMinMax(LayoutUnit logicalWidth, LayoutUnit availableWidth, RenderBlock& cb, RenderFragmentContainer* fragment, AllowIntrinsic allowIntrinsic) const
{
    const RenderStyle& styleToUse = style();

    if (shouldComputeLogicalHeightFromAspectRatio()) {
        auto [logicalMinWidth, logicalMaxWidth] = computeMinMaxLogicalWidthFromAspectRatio();
        logicalWidth = std::clamp(logicalWidth, logicalMinWidth, logicalMaxWidth);
    }

    if (!styleToUse.logicalMaxWidth().isUndefined() && (allowIntrinsic == AllowIntrinsic::Yes || !styleToUse.logicalMaxWidth().isIntrinsic()))
        logicalWidth = std::min(logicalWidth, computeLogicalWidthInFragmentUsing(MaxSize, styleToUse.logicalMaxWidth(), availableWidth, cb, fragment));
    if (allowIntrinsic == AllowIntrinsic::No && styleToUse.logicalMinWidth().isIntrinsic())
        return logicalWidth;
    auto logicalMinWidth = styleToUse.logicalMinWidth();
    if (logicalMinWidth.isAuto() && shouldComputeLogicalWidthFromAspectRatio() && (styleToUse.logicalWidth().isAuto() || styleToUse.logicalWidth().isMinContent() || styleToUse.logicalWidth().isMaxContent()) && !is<RenderReplaced>(*this) && effectiveOverflowInlineDirection() == Overflow::Visible) {
        // Make sure we actually used the aspect ratio.
        logicalMinWidth = Length(LengthType::MinContent);
    }
    return std::max(logicalWidth, computeLogicalWidthInFragmentUsing(MinSize, logicalMinWidth, availableWidth, cb, fragment));
}

LayoutUnit RenderBox::constrainLogicalHeightByMinMax(LayoutUnit logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    const RenderStyle& styleToUse = style();
    if (!styleToUse.logicalMaxHeight().isUndefined()) {
        if (std::optional<LayoutUnit> maxH = computeLogicalHeightUsing(MaxSize, styleToUse.logicalMaxHeight(), intrinsicContentHeight))
            logicalHeight = std::min(logicalHeight, maxH.value());
    }
    auto logicalMinHeight = styleToUse.logicalMinHeight();
    if (logicalMinHeight.isAuto() && shouldComputeLogicalHeightFromAspectRatio() && intrinsicContentHeight && !is<RenderReplaced>(*this) && effectiveOverflowBlockDirection() == Overflow::Visible) {
        auto heightFromAspectRatio = blockSizeFromAspectRatio(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent(), style().logicalAspectRatio(), style().boxSizingForAspectRatio(), logicalWidth()) - borderAndPaddingLogicalHeight();
        if (firstChild())
            heightFromAspectRatio = std::max(heightFromAspectRatio, *intrinsicContentHeight);
        logicalMinHeight = Length(heightFromAspectRatio, LengthType::Fixed);
    }
    if (logicalMinHeight.isMinContent() || logicalMinHeight.isMaxContent())
        logicalMinHeight = Length();
    if (std::optional<LayoutUnit> computedLogicalHeight = computeLogicalHeightUsing(MinSize, logicalMinHeight, intrinsicContentHeight))
        return std::max(logicalHeight, computedLogicalHeight.value());
    return logicalHeight;
}

LayoutUnit RenderBox::constrainContentBoxLogicalHeightByMinMax(LayoutUnit logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    const RenderStyle& styleToUse = style();
    if (!styleToUse.logicalMaxHeight().isUndefined()) {
        if (std::optional<LayoutUnit> maxH = computeContentLogicalHeight(MaxSize, styleToUse.logicalMaxHeight(), intrinsicContentHeight))
            logicalHeight = std::min(logicalHeight, maxH.value());
    }
    if (std::optional<LayoutUnit> computedContentLogicalHeight = computeContentLogicalHeight(MinSize, styleToUse.logicalMinHeight(), intrinsicContentHeight))
        return std::max(logicalHeight, computedContentLogicalHeight.value());
    return logicalHeight;
}

RoundedRect::Radii RenderBox::borderRadii() const
{
    auto& style = this->style();
    LayoutRect bounds = frameRect();

    unsigned borderLeft = style.borderLeftWidth();
    unsigned borderTop = style.borderTopWidth();
    bounds.moveBy(LayoutPoint(borderLeft, borderTop));
    bounds.contract(borderLeft + style.borderRightWidth(), borderTop + style.borderBottomWidth());
    return style.getRoundedBorderFor(bounds).radii();
}

RoundedRect RenderBox::roundedBorderBoxRect() const
{
    return style().getRoundedInnerBorderFor(borderBoxRect());
}

LayoutRect RenderBox::paddingBoxRect() const
{
    auto verticalScrollbarWidth = this->verticalScrollbarWidth();
    LayoutUnit offsetForScrollbar = shouldPlaceVerticalScrollbarOnLeft() ? verticalScrollbarWidth : 0;

    return LayoutRect(borderLeft() + offsetForScrollbar, borderTop(),
        width() - borderLeft() - borderRight() - verticalScrollbarWidth,
        height() - borderTop() - borderBottom() - horizontalScrollbarHeight());
}

LayoutRect RenderBox::contentBoxRect() const
{
    return { contentBoxLocation(), contentSize() };
}

LayoutPoint RenderBox::contentBoxLocation() const
{
    LayoutUnit scrollbarSpace = shouldPlaceVerticalScrollbarOnLeft() ? verticalScrollbarWidth() : 0;
    return { borderLeft() + paddingLeft() + scrollbarSpace, borderTop() + paddingTop() };
}

LayoutRect RenderBox::referenceBox(CSSBoxType boxType) const
{
    LayoutRect referenceBox;
    switch (boxType) {
    case CSSBoxType::ContentBox:
    case CSSBoxType::FillBox:
        return contentBoxRect();
    case CSSBoxType::PaddingBox:
        return paddingBoxRect();
    case CSSBoxType::MarginBox:
        return marginBoxRect();
    // stroke-box, view-box compute to border-box for HTML elements.
    case CSSBoxType::StrokeBox:
    case CSSBoxType::ViewBox:
    case CSSBoxType::BorderBox:
    case CSSBoxType::BoxMissing:
        return borderBoxRect();
    }
    return { };
}

IntRect RenderBox::absoluteContentBox() const
{
    // This is wrong with transforms and flipped writing modes.
    IntRect rect = snappedIntRect(contentBoxRect());
    FloatPoint absPos = localToAbsolute();
    rect.move(absPos.x(), absPos.y());
    return rect;
}

FloatQuad RenderBox::absoluteContentQuad() const
{
    LayoutRect rect = contentBoxRect();
    return localToAbsoluteQuad(FloatRect(rect));
}

LayoutRect RenderBox::outlineBoundsForRepaint(const RenderLayerModelObject* repaintContainer, const RenderGeometryMap* geometryMap) const
{
    LayoutRect box = borderBoundingBox();
    adjustRectForOutlineAndShadow(box);

    if (repaintContainer != this) {
        FloatQuad containerRelativeQuad;
        if (geometryMap)
            containerRelativeQuad = geometryMap->mapToContainer(box, repaintContainer);
        else
            containerRelativeQuad = localToContainerQuad(FloatRect(box), repaintContainer);

        box = LayoutRect(containerRelativeQuad.boundingBox());
    }
    
    // FIXME: layoutDelta needs to be applied in parts before/after transforms and
    // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
    box.move(view().frameView().layoutContext().layoutDelta());

    return LayoutRect(snapRectToDevicePixels(box, document().deviceScaleFactor()));
}

void RenderBox::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject*)
{
    if (!size().isEmpty())
        rects.append(LayoutRect(additionalOffset, size()));
}

int RenderBox::reflectionOffset() const
{
    if (!style().boxReflect())
        return 0;
    if (style().boxReflect()->direction() == ReflectionDirection::Left || style().boxReflect()->direction() == ReflectionDirection::Right)
        return valueForLength(style().boxReflect()->offset(), borderBoxRect().width());
    return valueForLength(style().boxReflect()->offset(), borderBoxRect().height());
}

LayoutRect RenderBox::reflectedRect(const LayoutRect& r) const
{
    if (!style().boxReflect())
        return LayoutRect();

    LayoutRect box = borderBoxRect();
    LayoutRect result = r;
    switch (style().boxReflect()->direction()) {
    case ReflectionDirection::Below:
        result.setY(box.maxY() + reflectionOffset() + (box.maxY() - r.maxY()));
        break;
    case ReflectionDirection::Above:
        result.setY(box.y() - reflectionOffset() - box.height() + (box.maxY() - r.maxY()));
        break;
    case ReflectionDirection::Left:
        result.setX(box.x() - reflectionOffset() - box.width() + (box.maxX() - r.maxX()));
        break;
    case ReflectionDirection::Right:
        result.setX(box.maxX() + reflectionOffset() + (box.maxX() - r.maxX()));
        break;
    }
    return result;
}

bool RenderBox::fixedElementLaysOutRelativeToFrame(const FrameView& frameView) const
{
    return isFixedPositioned() && container()->isRenderView() && frameView.fixedElementsLayoutRelativeToFrame();
}

bool RenderBox::includeVerticalScrollbarSize() const
{
    return hasNonVisibleOverflow() && layer() && !layer()->hasOverlayScrollbars()
        && (style().overflowY() == Overflow::Scroll || style().overflowY() == Overflow::Auto);
}

bool RenderBox::includeHorizontalScrollbarSize() const
{
    return hasNonVisibleOverflow() && layer() && !layer()->hasOverlayScrollbars()
        && (style().overflowX() == Overflow::Scroll || style().overflowX() == Overflow::Auto);
}

int RenderBox::verticalScrollbarWidth() const
{
    auto* scrollableArea = layer() ? layer()->scrollableArea() : nullptr;
    if (!scrollableArea)
        return 0;
    return includeVerticalScrollbarSize() ? scrollableArea->verticalScrollbarWidth() : 0;
}

int RenderBox::horizontalScrollbarHeight() const
{
    auto* scrollableArea = layer() ? layer()->scrollableArea() : nullptr;
    if (!scrollableArea)
        return 0;
    return includeHorizontalScrollbarSize() ? scrollableArea->horizontalScrollbarHeight() : 0;
}

int RenderBox::intrinsicScrollbarLogicalWidth() const
{
    if (!hasNonVisibleOverflow())
        return 0;

    if (isHorizontalWritingMode() && (style().overflowY() == Overflow::Scroll && !canUseOverlayScrollbars())) {
        ASSERT(layer() && layer()->scrollableArea() && layer()->scrollableArea()->hasVerticalScrollbar());
        return verticalScrollbarWidth();
    }

    if (!isHorizontalWritingMode() && (style().overflowX() == Overflow::Scroll && !canUseOverlayScrollbars())) {
        ASSERT(layer() && layer()->scrollableArea() && layer()->scrollableArea()->hasHorizontalScrollbar());
        return horizontalScrollbarHeight();
    }

    return 0;
}

bool RenderBox::scrollLayer(ScrollDirection direction, ScrollGranularity granularity, unsigned stepCount, Element** stopElement)
{
    auto* scrollableArea = layer() ? layer()->scrollableArea() : nullptr;
    if (scrollableArea && scrollableArea->scroll(direction, granularity, stepCount)) {
        if (stopElement)
            *stopElement = element();

        return true;
    }

    return false;
}

bool RenderBox::scroll(ScrollDirection direction, ScrollGranularity granularity, unsigned stepCount, Element** stopElement, RenderBox* startBox, const IntPoint& wheelEventAbsolutePoint)
{
    if (scrollLayer(direction, granularity, stepCount, stopElement))
        return true;

    if (stopElement && *stopElement && *stopElement == element())
        return true;

    RenderBlock* nextScrollBlock = containingBlock();

    if (nextScrollBlock && !nextScrollBlock->isRenderView())
        return nextScrollBlock->scroll(direction, granularity, stepCount, stopElement, startBox, wheelEventAbsolutePoint);

    return false;
}

bool RenderBox::logicalScroll(ScrollLogicalDirection direction, ScrollGranularity granularity, unsigned stepCount, Element** stopElement)
{
    bool scrolled = false;
    
    if (auto* scrollableArea = layer() ? layer()->scrollableArea() : nullptr) {
#if PLATFORM(COCOA)
        // On Mac only we reset the inline direction position when doing a document scroll (e.g., hitting Home/End).
        if (granularity == ScrollGranularity::Document)
            scrolled = scrollableArea->scroll(logicalToPhysical(ScrollInlineDirectionBackward, isHorizontalWritingMode(), style().isFlippedBlocksWritingMode()), ScrollGranularity::Document, stepCount);
#endif
        if (scrollableArea->scroll(logicalToPhysical(direction, isHorizontalWritingMode(), style().isFlippedBlocksWritingMode()), granularity, stepCount))
            scrolled = true;
        
        if (scrolled) {
            if (stopElement)
                *stopElement = element();
            return true;
        }
    }

    if (stopElement && *stopElement && *stopElement == element())
        return true;

    RenderBlock* b = containingBlock();
    if (b && !b->isRenderView())
        return b->logicalScroll(direction, granularity, stepCount, stopElement);
    return false;
}

bool RenderBox::canBeScrolledAndHasScrollableArea() const
{
    return canBeProgramaticallyScrolled() && (hasHorizontalOverflow() || hasVerticalOverflow());
}

bool RenderBox::isScrollableOrRubberbandableBox() const
{
    return canBeScrolledAndHasScrollableArea();
}

bool RenderBox::requiresLayerWithScrollableArea() const
{
    // FIXME: This is wrong; these boxes' layers should not need ScrollableAreas via RenderLayer.
    if (isRenderView() || isDocumentElementRenderer())
        return true;

    if (hasPotentiallyScrollableOverflow())
        return true;

    if (style().resize() != Resize::None)
        return true;

    if (isHTMLMarquee() && style().marqueeBehavior() != MarqueeBehavior::None)
        return true;

    return false;
}

// FIXME: This is badly named. overflow:hidden can be programmatically scrolled, yet this returns false in that case.
bool RenderBox::canBeProgramaticallyScrolled() const
{
    if (isRenderView())
        return true;

    if (!hasPotentiallyScrollableOverflow())
        return false;

    if (hasScrollableOverflowX() || hasScrollableOverflowY())
        return true;

    return element() && element()->hasEditableStyle();
}

bool RenderBox::usesCompositedScrolling() const
{
    return hasNonVisibleOverflow() && hasLayer() && layer()->usesCompositedScrolling();
}

void RenderBox::autoscroll(const IntPoint& position)
{
    if (layer())
        layer()->autoscroll(position);
}

// There are two kinds of renderer that can autoscroll.
bool RenderBox::canAutoscroll() const
{
    if (isRenderView())
        return view().frameView().isScrollable();

    // Check for a box that can be scrolled in its own right.
    if (canBeScrolledAndHasScrollableArea())
        return true;

    return false;
}

// If specified point is in border belt, returned offset denotes direction of
// scrolling.
IntSize RenderBox::calculateAutoscrollDirection(const IntPoint& windowPoint) const
{
    IntRect box(absoluteBoundingBoxRect());
    box.moveBy(view().frameView().scrollPosition());
    IntRect windowBox = view().frameView().contentsToWindow(box);

    IntPoint windowAutoscrollPoint = windowPoint;

    if (windowAutoscrollPoint.x() < windowBox.x() + autoscrollBeltSize)
        windowAutoscrollPoint.move(-autoscrollBeltSize, 0);
    else if (windowAutoscrollPoint.x() > windowBox.maxX() - autoscrollBeltSize)
        windowAutoscrollPoint.move(autoscrollBeltSize, 0);

    if (windowAutoscrollPoint.y() < windowBox.y() + autoscrollBeltSize)
        windowAutoscrollPoint.move(0, -autoscrollBeltSize);
    else if (windowAutoscrollPoint.y() > windowBox.maxY() - autoscrollBeltSize)
        windowAutoscrollPoint.move(0, autoscrollBeltSize);

    return windowAutoscrollPoint - windowPoint;
}

RenderBox* RenderBox::findAutoscrollable(RenderObject* renderer)
{
    while (renderer && !(is<RenderBox>(*renderer) && downcast<RenderBox>(*renderer).canAutoscroll())) {
        if (is<RenderView>(*renderer) && renderer->document().ownerElement())
            renderer = renderer->document().ownerElement()->renderer();
        else
            renderer = renderer->parent();
    }

    return dynamicDowncast<RenderBox>(renderer);
}

void RenderBox::panScroll(const IntPoint& source)
{
    if (auto* scrollableArea = layer() ? layer()->scrollableArea() : nullptr)
        scrollableArea->panScrollFromPoint(source);
}

bool RenderBox::canUseOverlayScrollbars() const
{
    return !style().hasPseudoStyle(PseudoId::Scrollbar) && ScrollbarTheme::theme().usesOverlayScrollbars();
}

bool RenderBox::hasAutoScrollbar(ScrollbarOrientation orientation) const
{
    if (!hasNonVisibleOverflow())
        return false;

    auto isAutoOrScrollWithOverlayScrollbar = [&](Overflow overflow) {
        return overflow == Overflow::Auto || (overflow == Overflow::Scroll && canUseOverlayScrollbars());
    };

    switch (orientation) {
    case ScrollbarOrientation::Horizontal:
        return isAutoOrScrollWithOverlayScrollbar(style().overflowX());
    case ScrollbarOrientation::Vertical:
        return isAutoOrScrollWithOverlayScrollbar(style().overflowY());
    }
    return false;
}

bool RenderBox::hasAlwaysPresentScrollbar(ScrollbarOrientation orientation) const
{
    if (!hasNonVisibleOverflow())
        return false;

    auto isAlwaysVisibleScrollbar = [&](Overflow overflow) {
        return overflow == Overflow::Scroll && !canUseOverlayScrollbars();
    };

    switch (orientation) {
    case ScrollbarOrientation::Horizontal:
        return isAlwaysVisibleScrollbar(style().overflowX());
    case ScrollbarOrientation::Vertical:
        return isAlwaysVisibleScrollbar(style().overflowY());
    }
    return false;
}

bool RenderBox::needsPreferredWidthsRecalculation() const
{
    return style().paddingStart().isPercentOrCalculated() || style().paddingEnd().isPercentOrCalculated() || (style().hasAspectRatio() && (hasRelativeLogicalHeight() || (isFlexItem() && hasStretchedLogicalHeight())));
}

ScrollPosition RenderBox::scrollPosition() const
{
    if (!hasPotentiallyScrollableOverflow())
        return { 0, 0 };

    ASSERT(hasLayer());
    auto* scrollableArea = layer()->scrollableArea();
    if (!scrollableArea)
        return { 0, 0 };

    return scrollableArea->scrollPosition();
}

LayoutSize RenderBox::cachedSizeForOverflowClip() const
{
    ASSERT(hasNonVisibleOverflow());
    ASSERT(hasLayer());
    return layer()->size();
}

bool RenderBox::applyCachedClipAndScrollPosition(LayoutRect& rect, const RenderLayerModelObject* container, VisibleRectContext context) const
{
    flipForWritingMode(rect);

    if (context.options.contains(VisibleRectContextOption::ApplyCompositedContainerScrolls) || this != container || !usesCompositedScrolling())
        rect.moveBy(-scrollPosition()); // For overflow:auto/scroll/hidden.

    // Do not clip scroll layer contents to reduce the number of repaints while scrolling.
    if ((!context.options.contains(VisibleRectContextOption::ApplyCompositedClips) && usesCompositedScrolling())
        || (!context.options.contains(VisibleRectContextOption::ApplyContainerClip) && this == container)) {
        flipForWritingMode(rect);
        return true;
    }

    // height() is inaccurate if we're in the middle of a layout of this RenderBox, so use the
    // layer's size instead. Even if the layer's size is wrong, the layer itself will repaint
    // anyway if its size does change.
    LayoutRect clipRect(LayoutPoint(), cachedSizeForOverflowClip());
    bool intersects;
    if (context.options.contains(VisibleRectContextOption::UseEdgeInclusiveIntersection))
        intersects = rect.edgeInclusiveIntersect(clipRect);
    else {
        rect.intersect(clipRect);
        intersects = !rect.isEmpty();
    }
    flipForWritingMode(rect);
    return intersects;
}

void RenderBox::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    minLogicalWidth = minPreferredLogicalWidth() - borderAndPaddingLogicalWidth();
    maxLogicalWidth = maxPreferredLogicalWidth() - borderAndPaddingLogicalWidth();
}

LayoutUnit RenderBox::minPreferredLogicalWidth() const
{
    if (preferredLogicalWidthsDirty()) {
        SetLayoutNeededForbiddenScope layoutForbiddenScope(*this);
        const_cast<RenderBox&>(*this).computePreferredLogicalWidths();
    }
    return m_minPreferredLogicalWidth;
}

LayoutUnit RenderBox::maxPreferredLogicalWidth() const
{
    if (preferredLogicalWidthsDirty()) {
        SetLayoutNeededForbiddenScope layoutForbiddenScope(*this);
        const_cast<RenderBox&>(*this).computePreferredLogicalWidths();
    }
    return m_maxPreferredLogicalWidth;
}

bool RenderBox::hasOverridingLogicalHeight() const
{
    return gOverridingLogicalHeightMap && gOverridingLogicalHeightMap->contains(this);
}

bool RenderBox::hasOverridingLogicalWidth() const
{
    return gOverridingLogicalWidthMap && gOverridingLogicalWidthMap->contains(this);
}

void RenderBox::setOverridingLogicalHeight(LayoutUnit height)
{
    if (!gOverridingLogicalHeightMap)
        gOverridingLogicalHeightMap = new OverrideSizeMap();
    gOverridingLogicalHeightMap->set(this, height);
}

void RenderBox::setOverridingLogicalWidth(LayoutUnit width)
{
    if (!gOverridingLogicalWidthMap)
        gOverridingLogicalWidthMap = new OverrideSizeMap();
    gOverridingLogicalWidthMap->set(this, width);
}

void RenderBox::clearOverridingLogicalHeight()
{
    if (gOverridingLogicalHeightMap)
        gOverridingLogicalHeightMap->remove(this);
}

void RenderBox::clearOverridingLogicalWidth()
{
    if (gOverridingLogicalWidthMap)
        gOverridingLogicalWidthMap->remove(this);
}

void RenderBox::clearOverridingContentSize()
{
    clearOverridingLogicalHeight();
    clearOverridingLogicalWidth();
}

LayoutUnit RenderBox::overridingLogicalWidth() const
{
    ASSERT(hasOverridingLogicalWidth());
    return gOverridingLogicalWidthMap->get(this);
}

LayoutUnit RenderBox::overridingLogicalHeight() const
{
    ASSERT(hasOverridingLogicalHeight());
    return gOverridingLogicalHeightMap->get(this);
}

std::optional<LayoutUnit> RenderBox::overridingContainingBlockContentWidth() const
{
    ASSERT(hasOverridingContainingBlockContentWidth());
    return containingBlock()->style().isHorizontalWritingMode()
        ? gOverridingContainingBlockContentLogicalWidthMap->get(this)
        : gOverridingContainingBlockContentLogicalHeightMap->get(this);
}

std::optional<LayoutUnit> RenderBox::overridingContainingBlockContentHeight() const
{
    ASSERT(hasOverridingContainingBlockContentHeight());
    return containingBlock()->style().isHorizontalWritingMode()
        ? gOverridingContainingBlockContentLogicalHeightMap->get(this)
        : gOverridingContainingBlockContentLogicalWidthMap->get(this);
}

bool RenderBox::hasOverridingContainingBlockContentWidth() const
{
    RenderBlock* cb = containingBlock();
    if (!cb)
        return false;

    return cb->style().isHorizontalWritingMode()
        ? gOverridingContainingBlockContentLogicalWidthMap && gOverridingContainingBlockContentLogicalWidthMap->contains(this)
        : gOverridingContainingBlockContentLogicalHeightMap && gOverridingContainingBlockContentLogicalHeightMap->contains(this);
}

bool RenderBox::hasOverridingContainingBlockContentHeight() const
{
    RenderBlock* cb = containingBlock();
    if (!cb)
        return false;

    return cb->style().isHorizontalWritingMode()
        ? gOverridingContainingBlockContentLogicalHeightMap && gOverridingContainingBlockContentLogicalHeightMap->contains(this)
        : gOverridingContainingBlockContentLogicalWidthMap && gOverridingContainingBlockContentLogicalWidthMap->contains(this);
}

std::optional<LayoutUnit> RenderBox::overridingContainingBlockContentLogicalWidth() const
{
    ASSERT(hasOverridingContainingBlockContentLogicalWidth());
    return gOverridingContainingBlockContentLogicalWidthMap->get(this);
}

std::optional<LayoutUnit> RenderBox::overridingContainingBlockContentLogicalHeight() const
{
    ASSERT(hasOverridingContainingBlockContentLogicalHeight());
    return gOverridingContainingBlockContentLogicalHeightMap->get(this);
}

bool RenderBox::hasOverridingContainingBlockContentLogicalWidth() const
{
    return gOverridingContainingBlockContentLogicalWidthMap && gOverridingContainingBlockContentLogicalWidthMap->contains(this);
}

bool RenderBox::hasOverridingContainingBlockContentLogicalHeight() const
{
    return gOverridingContainingBlockContentLogicalHeightMap && gOverridingContainingBlockContentLogicalHeightMap->contains(this);
}

void RenderBox::setOverridingContainingBlockContentLogicalWidth(std::optional<LayoutUnit> logicalWidth)
{
    if (!gOverridingContainingBlockContentLogicalWidthMap)
        gOverridingContainingBlockContentLogicalWidthMap = new OverrideOptionalSizeMap;
    gOverridingContainingBlockContentLogicalWidthMap->set(this, logicalWidth);
}

void RenderBox::setOverridingContainingBlockContentLogicalHeight(std::optional<LayoutUnit> logicalHeight)
{
    if (!gOverridingContainingBlockContentLogicalHeightMap)
        gOverridingContainingBlockContentLogicalHeightMap = new OverrideOptionalSizeMap;
    gOverridingContainingBlockContentLogicalHeightMap->set(this, logicalHeight);
}

void RenderBox::clearOverridingContainingBlockContentSize()
{
    if (gOverridingContainingBlockContentLogicalWidthMap)
        gOverridingContainingBlockContentLogicalWidthMap->remove(this);
    clearOverridingContainingBlockContentLogicalHeight();
}

void RenderBox::clearOverridingContainingBlockContentLogicalHeight()
{
    if (gOverridingContainingBlockContentLogicalHeightMap)
        gOverridingContainingBlockContentLogicalHeightMap->remove(this);
}

Length RenderBox::overridingLogicalHeightLength() const
{
    ASSERT(hasOverridingLogicalHeightLength());
    return gOverridingLogicalHeightLengthMap->get(this);
}

void RenderBox::setOverridingLogicalHeightLength(const Length& height)
{
    if (!gOverridingLogicalHeightLengthMap)
        gOverridingLogicalHeightLengthMap = new OverridingLengthMap();
    gOverridingLogicalHeightLengthMap->set(this, height);
}

bool RenderBox::hasOverridingLogicalHeightLength() const
{
    return gOverridingLogicalHeightLengthMap && gOverridingLogicalHeightLengthMap->contains(this);
}

void RenderBox::clearOverridingLogicalHeightLength()
{
    if (gOverridingLogicalHeightLengthMap)
        gOverridingLogicalHeightLengthMap->remove(this);
}

Length RenderBox::overridingLogicalWidthLength() const
{
    ASSERT(hasOverridingLogicalWidthLength());
    return gOverridingLogicalWidthLengthMap->get(this);
}

void RenderBox::setOverridingLogicalWidthLength(const Length& height)
{
    if (!gOverridingLogicalWidthLengthMap)
        gOverridingLogicalWidthLengthMap = new OverridingLengthMap();
    gOverridingLogicalWidthLengthMap->set(this, height);
}

bool RenderBox::hasOverridingLogicalWidthLength() const
{
    return gOverridingLogicalWidthLengthMap && gOverridingLogicalWidthLengthMap->contains(this);
}

void RenderBox::clearOverridingLogicalWidthLength()
{
    if (gOverridingLogicalWidthLengthMap)
        gOverridingLogicalWidthLengthMap->remove(this);
}

LayoutUnit RenderBox::adjustBorderBoxLogicalWidthForBoxSizing(const Length& logicalWidth) const
{
    auto width = LayoutUnit { logicalWidth.value() };
    LayoutUnit bordersPlusPadding = borderAndPaddingLogicalWidth();
    if (style().boxSizing() == BoxSizing::ContentBox || logicalWidth.isIntrinsicOrAuto())
        return width + bordersPlusPadding;
    return std::max(width, bordersPlusPadding);
}

LayoutUnit RenderBox::adjustBorderBoxLogicalWidthForBoxSizing(LayoutUnit computedLogicalWidth, LengthType originalType) const
{
    if (originalType == LengthType::Calculated)
        return adjustBorderBoxLogicalWidthForBoxSizing({ computedLogicalWidth, LengthType::Fixed, false });
    return adjustBorderBoxLogicalWidthForBoxSizing({ computedLogicalWidth, originalType, false });
}

LayoutUnit RenderBox::adjustBorderBoxLogicalHeightForBoxSizing(LayoutUnit height) const
{
    LayoutUnit bordersPlusPadding = borderAndPaddingLogicalHeight();
    if (style().boxSizing() == BoxSizing::ContentBox)
        return height + bordersPlusPadding;
    return std::max(height, bordersPlusPadding);
}

LayoutUnit RenderBox::adjustContentBoxLogicalWidthForBoxSizing(const Length& logicalWidth) const
{
    auto width = LayoutUnit { logicalWidth.value() };
    if (style().boxSizing() == BoxSizing::ContentBox || logicalWidth.isIntrinsicOrAuto())
        return std::max(0_lu, width);
    return std::max(0_lu, width - borderAndPaddingLogicalWidth());
}

LayoutUnit RenderBox::adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit computedLogicalWidth, LengthType originalType) const
{
    if (originalType == LengthType::Calculated)
        return adjustContentBoxLogicalWidthForBoxSizing({ computedLogicalWidth, LengthType::Fixed, false });
    return adjustContentBoxLogicalWidthForBoxSizing({ computedLogicalWidth, originalType, false });
}

LayoutUnit RenderBox::adjustContentBoxLogicalHeightForBoxSizing(std::optional<LayoutUnit> height) const
{
    if (!height)
        return 0;
    LayoutUnit result = height.value();
    if (style().boxSizing() == BoxSizing::BorderBox)
        result -= borderAndPaddingLogicalHeight();
    return std::max(0_lu, result);
}

// Hit Testing
bool RenderBox::hitTestVisualOverflow(const HitTestLocation& hitTestLocation, const LayoutPoint& accumulatedOffset) const
{
    if (isRenderView())
        return true;

    LayoutPoint adjustedLocation = accumulatedOffset + location();
    LayoutRect overflowBox = visualOverflowRect();
    flipForWritingMode(overflowBox);
    overflowBox.moveBy(adjustedLocation);
    return hitTestLocation.intersects(overflowBox);
}

bool RenderBox::hitTestClipPath(const HitTestLocation& hitTestLocation, const LayoutPoint& accumulatedOffset) const
{
    if (!style().clipPath())
        return true;

    auto offsetFromHitTestRoot = toLayoutSize(accumulatedOffset + location());
    auto hitTestLocationInLocalCoordinates = hitTestLocation.point() - offsetFromHitTestRoot;
    switch (style().clipPath()->type()) {
    case PathOperation::Shape: {
        auto& clipPath = downcast<ShapePathOperation>(*style().clipPath());
        auto referenceBoxRect = referenceBox(clipPath.referenceBox());
        if (!clipPath.pathForReferenceRect(referenceBoxRect).contains(hitTestLocationInLocalCoordinates, clipPath.windRule()))
            return false;
        break;
    }
    case PathOperation::Reference: {
        const auto& referencePathOperation = downcast<ReferencePathOperation>(*style().clipPath());
        auto* element = document().getElementById(referencePathOperation.fragment());
        if (!element || !element->renderer())
            break;
        if (!is<SVGClipPathElement>(*element))
            break;
        auto& clipper = downcast<RenderSVGResourceClipper>(*element->renderer());
        if (!clipper.hitTestClipContent(FloatRect(borderBoxRect()), FloatPoint { hitTestLocationInLocalCoordinates }))
            return false;
        break;
    }
    case PathOperation::Box:
        break;
    case PathOperation::Ray:
        ASSERT_NOT_REACHED("clip-path does not support Ray shape");
        break;
    }

    return true;
}

bool RenderBox::hitTestBorderRadius(const HitTestLocation& hitTestLocation, const LayoutPoint& accumulatedOffset) const
{
    if (isRenderView() || !style().hasBorderRadius())
        return true;

    LayoutPoint adjustedLocation = accumulatedOffset + location();
    LayoutRect borderRect = borderBoxRect();
    borderRect.moveBy(adjustedLocation);
    RoundedRect border = style().getRoundedBorderFor(borderRect);
    return hitTestLocation.intersects(border);
}

bool RenderBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    LayoutPoint adjustedLocation = accumulatedOffset + location();

    // Check kids first.
    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (!child->hasLayer() && child->nodeAtPoint(request, result, locationInContainer, adjustedLocation, action)) {
            updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
            return true;
        }
    }

    // Check our bounds next. For this purpose always assume that we can only be hit in the
    // foreground phase (which is true for replaced elements like images).
    LayoutRect boundsRect = borderBoxRectInFragment(nullptr);
    boundsRect.moveBy(adjustedLocation);
    if (visibleToHitTesting(request) && action == HitTestForeground && locationInContainer.intersects(boundsRect)) {
        if (!hitTestVisualOverflow(locationInContainer, accumulatedOffset))
            return false;

        if (!hitTestClipPath(locationInContainer, accumulatedOffset))
            return false;

        if (!hitTestBorderRadius(locationInContainer, accumulatedOffset))
            return false;

        updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
        if (result.addNodeToListBasedTestResult(nodeForHitTest(), request, locationInContainer, boundsRect) == HitTestProgress::Stop)
            return true;
    }

    return RenderBoxModelObject::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, action);
}

// --------------------- painting stuff -------------------------------

void RenderBox::paintRootBoxFillLayers(const PaintInfo& paintInfo)
{
    ASSERT(isDocumentElementRenderer());
    if (paintInfo.skipRootBackground())
        return;

    auto* rootBackgroundRenderer = view().rendererForRootBackground();
    if (!rootBackgroundRenderer)
        return;

    auto& style = rootBackgroundRenderer->style();
    auto color = style.visitedDependentColor(CSSPropertyBackgroundColor);
    auto compositeOp = document().compositeOperatorForBackgroundColor(color, *this);

    paintFillLayers(paintInfo, style.colorByApplyingColorFilter(color), style.backgroundLayers(), view().backgroundRect(), BackgroundBleedNone, compositeOp, rootBackgroundRenderer);
}

BackgroundBleedAvoidance RenderBox::determineBackgroundBleedAvoidance(GraphicsContext& context) const
{
    if (context.paintingDisabled())
        return BackgroundBleedNone;

    const RenderStyle& style = this->style();

    if (!style.hasBackground() || !style.hasBorder() || !style.hasBorderRadius() || borderImageIsLoadedAndCanBeRendered())
        return BackgroundBleedNone;

    AffineTransform ctm = context.getCTM();
    FloatSize contextScaling(static_cast<float>(ctm.xScale()), static_cast<float>(ctm.yScale()));

    // Because RoundedRect uses IntRect internally the inset applied by the 
    // BackgroundBleedShrinkBackground strategy cannot be less than one integer
    // layout coordinate, even with subpixel layout enabled. To take that into
    // account, we clamp the contextScaling to 1.0 for the following test so
    // that borderObscuresBackgroundEdge can only return true if the border
    // widths are greater than 2 in both layout coordinates and screen
    // coordinates.
    // This precaution will become obsolete if RoundedRect is ever promoted to
    // a sub-pixel representation.
    if (contextScaling.width() > 1) 
        contextScaling.setWidth(1);
    if (contextScaling.height() > 1) 
        contextScaling.setHeight(1);

    if (borderObscuresBackgroundEdge(contextScaling))
        return BackgroundBleedShrinkBackground;
    if (!style.hasEffectiveAppearance() && borderObscuresBackground() && backgroundHasOpaqueTopLayer())
        return BackgroundBleedBackgroundOverBorder;

    return BackgroundBleedUseTransparencyLayer;
}

void RenderBox::paintBoxDecorations(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(*this))
        return;

    LayoutRect paintRect = borderBoxRectInFragment(nullptr);
    paintRect.moveBy(paintOffset);
    adjustBorderBoxRectForPainting(paintRect);

    paintRect = theme().adjustedPaintRect(*this, paintRect);
    BackgroundBleedAvoidance bleedAvoidance = determineBackgroundBleedAvoidance(paintInfo.context());

    // FIXME: Should eventually give the theme control over whether the box shadow should paint, since controls could have
    // custom shadows of their own.
    if (!boxShadowShouldBeAppliedToBackground(paintRect.location(), bleedAvoidance, { }))
        paintBoxShadow(paintInfo, paintRect, style(), ShadowStyle::Normal);

    GraphicsContextStateSaver stateSaver(paintInfo.context(), false);
    if (bleedAvoidance == BackgroundBleedUseTransparencyLayer) {
        // To avoid the background color bleeding out behind the border, we'll render background and border
        // into a transparency layer, and then clip that in one go (which requires setting up the clip before
        // beginning the layer).
        stateSaver.save();
        paintInfo.context().clipRoundedRect(style().getRoundedBorderFor(paintRect).pixelSnappedRoundedRectForPainting(document().deviceScaleFactor()));
        paintInfo.context().beginTransparencyLayer(1);
    }

    // If we have a native theme appearance, paint that before painting our background.
    // The theme will tell us whether or not we should also paint the CSS background.
    bool borderOrBackgroundPaintingIsNeeded = true;
    if (style().hasEffectiveAppearance()) {
        ControlStates* controlStates = controlStatesForRenderer(*this);
        borderOrBackgroundPaintingIsNeeded = theme().paint(*this, *controlStates, paintInfo, paintRect);
        if (controlStates->needsRepaint())
            view().scheduleLazyRepaint(*this);
    }

    if (borderOrBackgroundPaintingIsNeeded) {
        if (bleedAvoidance == BackgroundBleedBackgroundOverBorder)
            paintBorder(paintInfo, paintRect, style(), bleedAvoidance);

        paintBackground(paintInfo, paintRect, bleedAvoidance);

        if (style().hasEffectiveAppearance())
            theme().paintDecorations(*this, paintInfo, paintRect);
    }
    paintBoxShadow(paintInfo, paintRect, style(), ShadowStyle::Inset);

    // The theme will tell us whether or not we should also paint the CSS border.
    if (bleedAvoidance != BackgroundBleedBackgroundOverBorder && (!style().hasEffectiveAppearance() || (borderOrBackgroundPaintingIsNeeded && theme().paintBorderOnly(*this, paintInfo, paintRect))) && style().hasVisibleBorderDecoration())
        paintBorder(paintInfo, paintRect, style(), bleedAvoidance);

    if (bleedAvoidance == BackgroundBleedUseTransparencyLayer)
        paintInfo.context().endTransparencyLayer();
}

bool RenderBox::paintsOwnBackground() const
{
    if (isBody()) {
        // The <body> only paints its background if the root element has defined a background independent of the body,
        // or if the <body>'s parent is not the document element's renderer (e.g. inside SVG foreignObject).
        auto documentElementRenderer = document().documentElement()->renderer();
        return !documentElementRenderer
            || documentElementRenderer->hasBackground()
            || (documentElementRenderer != parent());
    }
    
    return true;
}

void RenderBox::paintBackground(const PaintInfo& paintInfo, const LayoutRect& paintRect, BackgroundBleedAvoidance bleedAvoidance)
{
    if (isDocumentElementRenderer()) {
        paintRootBoxFillLayers(paintInfo);
        return;
    }

    if (!paintsOwnBackground())
        return;

    if (backgroundIsKnownToBeObscured(paintRect.location()) && !boxShadowShouldBeAppliedToBackground(paintRect.location(), bleedAvoidance, { }))
        return;

    auto backgroundColor = style().visitedDependentColor(CSSPropertyBackgroundColor);
    auto compositeOp = document().compositeOperatorForBackgroundColor(backgroundColor, *this);

    paintFillLayers(paintInfo, style().colorByApplyingColorFilter(backgroundColor), style().backgroundLayers(), paintRect, bleedAvoidance, compositeOp);
}

bool RenderBox::getBackgroundPaintedExtent(const LayoutPoint& paintOffset, LayoutRect& paintedExtent) const
{
    ASSERT(hasBackground());
    LayoutRect backgroundRect = snappedIntRect(borderBoxRect());

    Color backgroundColor = style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
    if (backgroundColor.isVisible()) {
        paintedExtent = backgroundRect;
        return true;
    }

    auto& layers = style().backgroundLayers();
    if (!layers.image() || layers.next()) {
        paintedExtent =  backgroundRect;
        return true;
    }

    auto geometry = calculateBackgroundImageGeometry(nullptr, layers, paintOffset, backgroundRect);
    paintedExtent = geometry.destRect();
    return !geometry.hasNonLocalGeometry();
}

bool RenderBox::backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const
{
    if (!paintsOwnBackground())
        return false;

    Color backgroundColor = style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
    if (!backgroundColor.isOpaque())
        return false;

    // If the element has appearance, it might be painted by theme.
    // We cannot be sure if theme paints the background opaque.
    // In this case it is safe to not assume opaqueness.
    // FIXME: May be ask theme if it paints opaque.
    if (style().hasEffectiveAppearance())
        return false;
    // FIXME: Check the opaqueness of background images.

    if (hasClip() || hasClipPath())
        return false;

    // FIXME: Use rounded rect if border radius is present.
    if (style().hasBorderRadius())
        return false;
    
    // FIXME: The background color clip is defined by the last layer.
    if (style().backgroundLayers().next())
        return false;
    LayoutRect backgroundRect;
    switch (style().backgroundClip()) {
    case FillBox::Border:
        backgroundRect = borderBoxRect();
        break;
    case FillBox::Padding:
        backgroundRect = paddingBoxRect();
        break;
    case FillBox::Content:
        backgroundRect = contentBoxRect();
        break;
    default:
        break;
    }
    return backgroundRect.contains(localRect);
}

static bool isCandidateForOpaquenessTest(const RenderBox& childBox)
{
    const RenderStyle& childStyle = childBox.style();
    if (childStyle.position() != PositionType::Static && childBox.containingBlock() != childBox.parent())
        return false;
    if (childStyle.visibility() != Visibility::Visible)
        return false;
    if (childStyle.shapeOutside())
        return false;
    if (!childBox.width() || !childBox.height())
        return false;
    if (RenderLayer* childLayer = childBox.layer()) {
        if (childLayer->isComposited())
            return false;
        // FIXME: Deal with z-index.
        if (!childStyle.hasAutoUsedZIndex())
            return false;
        if (childLayer->hasTransform() || childLayer->isTransparent() || childLayer->hasFilter())
            return false;
        if (!childBox.scrollPosition().isZero())
            return false;
    }
    return true;
}

bool RenderBox::foregroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect, unsigned maxDepthToTest) const
{
    if (!maxDepthToTest)
        return false;

    for (auto& childBox : childrenOfType<RenderBox>(*this)) {
        if (!isCandidateForOpaquenessTest(childBox))
            continue;
        LayoutPoint childLocation = childBox.location();
        if (childBox.isRelativelyPositioned())
            childLocation.move(childBox.relativePositionOffset());
        LayoutRect childLocalRect = localRect;
        childLocalRect.moveBy(-childLocation);
        if (childLocalRect.y() < 0 || childLocalRect.x() < 0) {
            // If there is unobscured area above/left of a static positioned box then the rect is probably not covered.
            if (childBox.style().position() == PositionType::Static)
                return false;
            continue;
        }
        if (childLocalRect.maxY() > childBox.height() || childLocalRect.maxX() > childBox.width())
            continue;
        if (childBox.backgroundIsKnownToBeOpaqueInRect(childLocalRect))
            return true;
        if (childBox.foregroundIsKnownToBeOpaqueInRect(childLocalRect, maxDepthToTest - 1))
            return true;
    }
    return false;
}

bool RenderBox::computeBackgroundIsKnownToBeObscured(const LayoutPoint& paintOffset)
{
    // Test to see if the children trivially obscure the background.
    // FIXME: This test can be much more comprehensive.
    if (!hasBackground())
        return false;
    // Table and root background painting is special.
    if (isTable() || isDocumentElementRenderer())
        return false;

    LayoutRect backgroundRect;
    if (!getBackgroundPaintedExtent(paintOffset, backgroundRect))
        return false;

    if (auto* scrollableArea = layer() ? layer()->scrollableArea() : nullptr) {
        if (scrollableArea->scrollingMayRevealBackground())
            return false;
    }
    return foregroundIsKnownToBeOpaqueInRect(backgroundRect, backgroundObscurationTestMaxDepth);
}

bool RenderBox::backgroundHasOpaqueTopLayer() const
{
    auto& fillLayer = style().backgroundLayers();
    if (fillLayer.clip() != FillBox::Border)
        return false;

    // Clipped with local scrolling
    if (hasNonVisibleOverflow() && fillLayer.attachment() == FillAttachment::LocalBackground)
        return false;

    if (fillLayer.hasOpaqueImage(*this) && fillLayer.hasRepeatXY() && fillLayer.image()->canRender(this, style().effectiveZoom()))
        return true;

    // If there is only one layer and no image, check whether the background color is opaque.
    if (!fillLayer.next() && !fillLayer.hasImage()) {
        Color bgColor = style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
        if (bgColor.isOpaque())
            return true;
    }

    return false;
}

void RenderBox::paintMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(*this) || style().visibility() != Visibility::Visible || paintInfo.phase != PaintPhase::Mask || paintInfo.context().paintingDisabled())
        return;

    LayoutRect paintRect = LayoutRect(paintOffset, size());
    adjustBorderBoxRectForPainting(paintRect);
    paintMaskImages(paintInfo, paintRect);
}

void RenderBox::paintClippingMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(*this) || style().visibility() != Visibility::Visible || paintInfo.phase != PaintPhase::ClippingMask || paintInfo.context().paintingDisabled())
        return;

    LayoutRect paintRect = LayoutRect(paintOffset, size());
    paintInfo.context().fillRect(snappedIntRect(paintRect), Color::black);
}

void RenderBox::paintMaskImages(const PaintInfo& paintInfo, const LayoutRect& paintRect)
{
    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    bool compositedMask = hasLayer() && layer()->hasCompositedMask();
    bool flattenCompositingLayers = paintInfo.paintBehavior.contains(PaintBehavior::FlattenCompositingLayers);
    CompositeOperator compositeOp = CompositeOperator::SourceOver;

    bool allMaskImagesLoaded = true;
    
    if (!compositedMask || flattenCompositingLayers) {
        pushTransparencyLayer = true;

        // Don't render a masked element until all the mask images have loaded, to prevent a flash of unmasked content.
        if (auto* maskBoxImage = style().maskBoxImage().image())
            allMaskImagesLoaded &= maskBoxImage->isLoaded();

        allMaskImagesLoaded &= style().maskLayers().imagesAreLoaded();

        paintInfo.context().setCompositeOperation(CompositeOperator::DestinationIn);
        paintInfo.context().beginTransparencyLayer(1);
        compositeOp = CompositeOperator::SourceOver;
    }

    if (allMaskImagesLoaded) {
        paintFillLayers(paintInfo, Color(), style().maskLayers(), paintRect, BackgroundBleedNone, compositeOp);
        paintNinePieceImage(paintInfo.context(), paintRect, style(), style().maskBoxImage(), compositeOp);
    }
    
    if (pushTransparencyLayer)
        paintInfo.context().endTransparencyLayer();
}

LayoutRect RenderBox::maskClipRect(const LayoutPoint& paintOffset)
{
    const NinePieceImage& maskBoxImage = style().maskBoxImage();
    if (maskBoxImage.image()) {
        LayoutRect borderImageRect = borderBoxRect();
        
        // Apply outsets to the border box.
        borderImageRect.expand(style().maskBoxImageOutsets());
        return borderImageRect;
    }
    
    LayoutRect result;
    LayoutRect borderBox = borderBoxRect();
    for (auto* maskLayer = &style().maskLayers(); maskLayer; maskLayer = maskLayer->next()) {
        if (maskLayer->image()) {
            // Masks should never have fixed attachment, so it's OK for paintContainer to be null.
            result.unite(calculateBackgroundImageGeometry(nullptr, *maskLayer, paintOffset, borderBox).destRect());
        }
    }
    return result;
}

void RenderBox::paintFillLayers(const PaintInfo& paintInfo, const Color& color, const FillLayer& fillLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, CompositeOperator op, RenderElement* backgroundObject)
{
    Vector<const FillLayer*, 8> layers;
    bool shouldDrawBackgroundInSeparateBuffer = false;

    for (auto* layer = &fillLayer; layer; layer = layer->next()) {
        layers.append(layer);

        if (layer->blendMode() != BlendMode::Normal)
            shouldDrawBackgroundInSeparateBuffer = true;

        // Stop traversal when an opaque layer is encountered.
        // FIXME: It would be possible for the following occlusion culling test to be more aggressive
        // on layers with no repeat by testing whether the image covers the layout rect.
        // Testing that here would imply duplicating a lot of calculations that are currently done in
        // RenderBoxModelObject::paintFillLayerExtended. A more efficient solution might be to move
        // the layer recursion into paintFillLayerExtended, or to compute the layer geometry here
        // and pass it down.

        // The clipOccludesNextLayers condition must be evaluated first to avoid short-circuiting.
        if (layer->clipOccludesNextLayers(layer == &fillLayer) && layer->hasOpaqueImage(*this) && layer->image()->canRender(this, style().effectiveZoom()) && layer->hasRepeatXY() && layer->blendMode() == BlendMode::Normal)
            break;
    }

    auto& context = paintInfo.context();
    auto baseBgColorUsage = BaseBackgroundColorUse;

    if (shouldDrawBackgroundInSeparateBuffer) {
        paintFillLayer(paintInfo, color, *layers.last(), rect, bleedAvoidance, op, backgroundObject, BaseBackgroundColorOnly);
        baseBgColorUsage = BaseBackgroundColorSkip;
        context.beginTransparencyLayer(1);
    }

    auto topLayer = layers.rend();
    for (auto it = layers.rbegin(); it != topLayer; ++it)
        paintFillLayer(paintInfo, color, **it, rect, bleedAvoidance, op, backgroundObject, baseBgColorUsage);

    if (shouldDrawBackgroundInSeparateBuffer)
        context.endTransparencyLayer();
}

void RenderBox::paintFillLayer(const PaintInfo& paintInfo, const Color& c, const FillLayer& fillLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, CompositeOperator op, RenderElement* backgroundObject, BaseBackgroundColorUsage baseBgColorUsage)
{
    paintFillLayerExtended(paintInfo, c, fillLayer, rect, bleedAvoidance, { }, { }, op, backgroundObject, baseBgColorUsage);
}

static StyleImage* findLayerUsedImage(WrappedImagePtr image, const FillLayer& layers)
{
    for (auto* layer = &layers; layer; layer = layer->next()) {
        if (layer->image() && image == layer->image()->data())
            return layer->image();
    }
    return nullptr;
}

void RenderBox::imageChanged(WrappedImagePtr image, const IntRect*)
{
    if (!parent())
        return;

    if ((style().borderImage().image() && style().borderImage().image()->data() == image) ||
        (style().maskBoxImage().image() && style().maskBoxImage().image()->data() == image)) {
        repaint();
        return;
    }

    ShapeValue* shapeOutsideValue = style().shapeOutside();
    if (!view().frameView().layoutContext().isInRenderTreeLayout() && isFloating() && shapeOutsideValue && shapeOutsideValue->image() && shapeOutsideValue->image()->data() == image) {
        ShapeOutsideInfo::ensureInfo(*this).markShapeAsDirty();
        markShapeOutsideDependentsForLayout();
    }

    bool didFullRepaint = repaintLayerRectsForImage(image, style().backgroundLayers(), true);
    if (!didFullRepaint)
        repaintLayerRectsForImage(image, style().maskLayers(), false);

    if (!isComposited())
        return;

    if (layer()->hasCompositedMask() && findLayerUsedImage(image, style().maskLayers()))
        layer()->contentChanged(MaskImageChanged);
    
    if (auto* styleImage = findLayerUsedImage(image, style().backgroundLayers())) {
        layer()->contentChanged(BackgroundImageChanged);
        incrementVisuallyNonEmptyPixelCountIfNeeded(flooredIntSize(styleImage->imageSize(this, style().effectiveZoom())));
    }
}

void RenderBox::incrementVisuallyNonEmptyPixelCountIfNeeded(const IntSize& size)
{
    if (didContibuteToVisuallyNonEmptyPixelCount())
        return;

    view().frameView().incrementVisuallyNonEmptyPixelCount(size);
    setDidContibuteToVisuallyNonEmptyPixelCount();
}

bool RenderBox::repaintLayerRectsForImage(WrappedImagePtr image, const FillLayer& layers, bool drawingBackground)
{
    LayoutRect rendererRect;
    RenderBox* layerRenderer = nullptr;

    for (auto* layer = &layers; layer; layer = layer->next()) {
        if (layer->image() && image == layer->image()->data() && (layer->image()->isLoaded() || layer->image()->canRender(this, style().effectiveZoom()))) {
            // Now that we know this image is being used, compute the renderer and the rect if we haven't already.
            bool drawingRootBackground = drawingBackground && (isDocumentElementRenderer() || (isBody() && !document().documentElement()->renderer()->hasBackground()));
            if (!layerRenderer) {
                if (drawingRootBackground) {
                    layerRenderer = &view();

                    LayoutUnit rw = downcast<RenderView>(*layerRenderer).frameView().contentsWidth();
                    LayoutUnit rh = downcast<RenderView>(*layerRenderer).frameView().contentsHeight();

                    rendererRect = LayoutRect(-layerRenderer->marginLeft(),
                        -layerRenderer->marginTop(),
                        std::max(layerRenderer->width() + layerRenderer->horizontalMarginExtent() + layerRenderer->borderLeft() + layerRenderer->borderRight(), rw),
                        std::max(layerRenderer->height() + layerRenderer->verticalMarginExtent() + layerRenderer->borderTop() + layerRenderer->borderBottom(), rh));
                } else {
                    layerRenderer = this;
                    rendererRect = borderBoxRect();
                }
            }
            // FIXME: Figure out how to pass absolute position to calculateBackgroundImageGeometry (for pixel snapping)
            BackgroundImageGeometry geometry = layerRenderer->calculateBackgroundImageGeometry(nullptr, *layer, LayoutPoint(), rendererRect);
            if (geometry.hasNonLocalGeometry()) {
                // Rather than incur the costs of computing the paintContainer for renderers with fixed backgrounds
                // in order to get the right destRect, just repaint the entire renderer.
                layerRenderer->repaint();
                return true;
            }
            
            LayoutRect rectToRepaint = geometry.destRect();
            bool shouldClipToLayer = true;

            // If this is the root background layer, we may need to extend the repaintRect if the FrameView has an
            // extendedBackground. We should only extend the rect if it is already extending the full width or height
            // of the rendererRect.
            if (drawingRootBackground && view().frameView().hasExtendedBackgroundRectForPainting()) {
                shouldClipToLayer = false;
                IntRect extendedBackgroundRect = view().frameView().extendedBackgroundRectForPainting();
                if (rectToRepaint.width() == rendererRect.width()) {
                    rectToRepaint.move(extendedBackgroundRect.x(), 0);
                    rectToRepaint.setWidth(extendedBackgroundRect.width());
                }
                if (rectToRepaint.height() == rendererRect.height()) {
                    rectToRepaint.move(0, extendedBackgroundRect.y());
                    rectToRepaint.setHeight(extendedBackgroundRect.height());
                }
            }

            layerRenderer->repaintRectangle(rectToRepaint, shouldClipToLayer);
            if (geometry.destRect() == rendererRect)
                return true;
        }
    }
    return false;
}

bool RenderBox::pushContentsClip(PaintInfo& paintInfo, const LayoutPoint& accumulatedOffset)
{
    if (paintInfo.phase == PaintPhase::BlockBackground || paintInfo.phase == PaintPhase::SelfOutline || paintInfo.phase == PaintPhase::Mask)
        return false;

    bool isControlClip = hasControlClip();
    bool isOverflowClip = hasNonVisibleOverflow() && !layer()->isSelfPaintingLayer();
    
    if (!isControlClip && !isOverflowClip)
        return false;
    
    if (paintInfo.phase == PaintPhase::Outline)
        paintInfo.phase = PaintPhase::ChildOutlines;
    else if (paintInfo.phase == PaintPhase::ChildBlockBackground) {
        paintInfo.phase = PaintPhase::BlockBackground;
        paintObject(paintInfo, accumulatedOffset);
        paintInfo.phase = PaintPhase::ChildBlockBackgrounds;
    }
    float deviceScaleFactor = document().deviceScaleFactor();
    FloatRect clipRect = snapRectToDevicePixels((isControlClip ? controlClipRect(accumulatedOffset) : overflowClipRect(accumulatedOffset, nullptr, IgnoreOverlayScrollbarSize, paintInfo.phase)), deviceScaleFactor);
    paintInfo.context().save();
    if (style().hasBorderRadius())
        paintInfo.context().clipRoundedRect(style().getRoundedInnerBorderFor(LayoutRect(accumulatedOffset, size())).pixelSnappedRoundedRectForPainting(deviceScaleFactor));
    paintInfo.context().clip(clipRect);

    if (paintInfo.phase == PaintPhase::EventRegion)
        paintInfo.eventRegionContext->pushClip(enclosingIntRect(clipRect));

    return true;
}

void RenderBox::popContentsClip(PaintInfo& paintInfo, PaintPhase originalPhase, const LayoutPoint& accumulatedOffset)
{
    ASSERT(hasControlClip() || (hasNonVisibleOverflow() && !layer()->isSelfPaintingLayer()));

    if (paintInfo.phase == PaintPhase::EventRegion)
        paintInfo.eventRegionContext->popClip();

    paintInfo.context().restore();
    if (originalPhase == PaintPhase::Outline) {
        paintInfo.phase = PaintPhase::SelfOutline;
        paintObject(paintInfo, accumulatedOffset);
        paintInfo.phase = originalPhase;
    } else if (originalPhase == PaintPhase::ChildBlockBackground)
        paintInfo.phase = originalPhase;
}

LayoutRect RenderBox::overflowClipRect(const LayoutPoint& location, RenderFragmentContainer* fragment, OverlayScrollbarSizeRelevancy relevancy, PaintPhase) const
{
    LayoutRect clipRect = borderBoxRectInFragment(fragment);
    clipRect.setLocation(location + clipRect.location() + LayoutSize(borderLeft(), borderTop()));
    clipRect.setSize(clipRect.size() - LayoutSize(borderLeft() + borderRight(), borderTop() + borderBottom()));
    if (style().overflowX() == Overflow::Clip && style().overflowY() == Overflow::Visible) {
        LayoutRect infRect = LayoutRect::infiniteRect();
        clipRect.setY(infRect.y());
        clipRect.setHeight(infRect.height());
    } else if (style().overflowY() == Overflow::Clip && style().overflowX() == Overflow::Visible) {
        LayoutRect infRect = LayoutRect::infiniteRect();
        clipRect.setX(infRect.x());
        clipRect.setWidth(infRect.width());
    }

    // Subtract out scrollbars if we have them.
    if (auto* scrollableArea = layer() ? layer()->scrollableArea() : nullptr) {
        if (shouldPlaceVerticalScrollbarOnLeft())
            clipRect.move(scrollableArea->verticalScrollbarWidth(relevancy), 0);
        clipRect.contract(scrollableArea->verticalScrollbarWidth(relevancy), scrollableArea->horizontalScrollbarHeight(relevancy));
    }

    return clipRect;
}

LayoutRect RenderBox::clipRect(const LayoutPoint& location, RenderFragmentContainer* fragment) const
{
    LayoutRect borderBoxRect = borderBoxRectInFragment(fragment);
    LayoutRect clipRect = LayoutRect(borderBoxRect.location() + location, borderBoxRect.size());

    if (!style().clipLeft().isAuto()) {
        LayoutUnit c = valueForLength(style().clipLeft(), borderBoxRect.width());
        clipRect.move(c, 0_lu);
        clipRect.contract(c, 0_lu);
    }

    // We don't use the fragment-specific border box's width and height since clip offsets are (stupidly) specified
    // from the left and top edges. Therefore it's better to avoid constraining to smaller widths and heights.

    if (!style().clipRight().isAuto())
        clipRect.contract(width() - valueForLength(style().clipRight(), width()), 0_lu);

    if (!style().clipTop().isAuto()) {
        LayoutUnit c = valueForLength(style().clipTop(), borderBoxRect.height());
        clipRect.move(0_lu, c);
        clipRect.contract(0_lu, c);
    }

    if (!style().clipBottom().isAuto())
        clipRect.contract(0_lu, height() - valueForLength(style().clipBottom(), height()));

    return clipRect;
}

LayoutUnit RenderBox::shrinkLogicalWidthToAvoidFloats(LayoutUnit childMarginStart, LayoutUnit childMarginEnd, const RenderBlock& cb, RenderFragmentContainer* fragment) const
{    
    RenderFragmentContainer* containingBlockFragment = nullptr;
    LayoutUnit logicalTopPosition = logicalTop();
    if (fragment) {
        LayoutUnit offsetFromLogicalTopOfFragment = fragment ? fragment->logicalTopForFragmentedFlowContent() - offsetFromLogicalTopOfFirstPage() : 0_lu;
        logicalTopPosition = std::max(logicalTopPosition, logicalTopPosition + offsetFromLogicalTopOfFragment);
        containingBlockFragment = cb.clampToStartAndEndFragments(fragment);
    }

    LayoutUnit logicalHeight = cb.logicalHeightForChild(*this);
    LayoutUnit result = cb.availableLogicalWidthForLineInFragment(logicalTopPosition, DoNotIndentText, containingBlockFragment, logicalHeight) - childMarginStart - childMarginEnd;

    // We need to see if margins on either the start side or the end side can contain the floats in question. If they can,
    // then just using the line width is inaccurate. In the case where a float completely fits, we don't need to use the line
    // offset at all, but can instead push all the way to the content edge of the containing block. In the case where the float
    // doesn't fit, we can use the line offset, but we need to grow it by the margin to reflect the fact that the margin was
    // "consumed" by the float. Negative margins aren't consumed by the float, and so we ignore them.
    if (childMarginStart > 0) {
        LayoutUnit startContentSide = cb.startOffsetForContent(containingBlockFragment);
        LayoutUnit startContentSideWithMargin = startContentSide + childMarginStart;
        LayoutUnit startOffset = cb.startOffsetForLineInFragment(logicalTopPosition, DoNotIndentText, containingBlockFragment, logicalHeight);
        if (startOffset > startContentSideWithMargin)
            result += childMarginStart;
        else
            result += startOffset - startContentSide;
    }
    
    if (childMarginEnd > 0) {
        LayoutUnit endContentSide = cb.endOffsetForContent(containingBlockFragment);
        LayoutUnit endContentSideWithMargin = endContentSide + childMarginEnd;
        LayoutUnit endOffset = cb.endOffsetForLineInFragment(logicalTopPosition, DoNotIndentText, containingBlockFragment, logicalHeight);
        if (endOffset > endContentSideWithMargin)
            result += childMarginEnd;
        else
            result += endOffset - endContentSide;
    }

    return result;
}

LayoutUnit RenderBox::containingBlockLogicalWidthForContent() const
{
    if (hasOverridingContainingBlockContentLogicalWidth())
        return overridingContainingBlockContentLogicalWidth().value_or(0_lu);

    if (RenderBlock* cb = containingBlock()) {
        if (isOutOfFlowPositioned())
            return cb->clientLogicalWidth();
        return cb->availableLogicalWidth();
    }
    return 0_lu;
}

LayoutUnit RenderBox::containingBlockLogicalHeightForContent(AvailableLogicalHeightType heightType) const
{
    if (hasOverridingContainingBlockContentLogicalHeight()) {
        // FIXME: Containing block for a grid item is the grid area it's located in. We need to return whatever
        // height value we get from overridingContainingBlockContentLogicalHeight() here, including std::nullopt.
        if (auto height = overridingContainingBlockContentLogicalHeight())
            return height.value();
    }

    if (RenderBlock* cb = containingBlock())
        return cb->availableLogicalHeight(heightType);
    return 0_lu;
}

LayoutUnit RenderBox::containingBlockLogicalWidthForContentInFragment(RenderFragmentContainer* fragment) const
{
    if (!fragment)
        return containingBlockLogicalWidthForContent();

    RenderBlock* cb = containingBlock();
    RenderFragmentContainer* containingBlockFragment = cb->clampToStartAndEndFragments(fragment);
    // FIXME: It's unclear if a fragment's content should use the containing block's override logical width.
    // If it should, the following line should call containingBlockLogicalWidthForContent.
    LayoutUnit result = cb->availableLogicalWidth();
    RenderBoxFragmentInfo* boxInfo = cb->renderBoxFragmentInfo(containingBlockFragment);
    if (!boxInfo)
        return result;
    return std::max<LayoutUnit>(0, result - (cb->logicalWidth() - boxInfo->logicalWidth()));
}

LayoutUnit RenderBox::containingBlockAvailableLineWidthInFragment(RenderFragmentContainer* fragment) const
{
    RenderBlock* cb = containingBlock();
    RenderFragmentContainer* containingBlockFragment = nullptr;
    LayoutUnit logicalTopPosition = logicalTop();
    if (fragment) {
        LayoutUnit offsetFromLogicalTopOfFragment = fragment ? fragment->logicalTopForFragmentedFlowContent() - offsetFromLogicalTopOfFirstPage() : 0_lu;
        logicalTopPosition = std::max(logicalTopPosition, logicalTopPosition + offsetFromLogicalTopOfFragment);
        containingBlockFragment = cb->clampToStartAndEndFragments(fragment);
    }
    return cb->availableLogicalWidthForLineInFragment(logicalTopPosition, DoNotIndentText, containingBlockFragment, availableLogicalHeight(IncludeMarginBorderPadding));
}

LayoutUnit RenderBox::perpendicularContainingBlockLogicalHeight() const
{
    if (hasOverridingContainingBlockContentLogicalHeight()) {
        if (auto height = overridingContainingBlockContentLogicalHeight())
            return height.value();
    }

    RenderBlock* cb = containingBlock();
    if (cb->hasOverridingLogicalHeight())
        return cb->overridingContentLogicalHeight();

    const RenderStyle& containingBlockStyle = cb->style();
    Length logicalHeightLength = containingBlockStyle.logicalHeight();

    // FIXME: For now just support fixed heights.  Eventually should support percentage heights as well.
    if (!logicalHeightLength.isFixed()) {
        LayoutUnit fillFallbackExtent = containingBlockStyle.isHorizontalWritingMode() ? view().frameView().layoutSize().height() : view().frameView().layoutSize().width();
        LayoutUnit fillAvailableExtent = containingBlock()->availableLogicalHeight(ExcludeMarginBorderPadding);
        view().addPercentHeightDescendant(const_cast<RenderBox&>(*this));
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=158286 We also need to perform the same percentHeightDescendant treatment to the element which dictates the return value for containingBlock()->availableLogicalHeight() above.
        return std::min(fillAvailableExtent, fillFallbackExtent);
    }

    // Use the content box logical height as specified by the style.
    return cb->adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit(logicalHeightLength.value()));
}

void RenderBox::mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState& transformState, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    if (ancestorContainer == this)
        return;

    if (!ancestorContainer && view().frameView().layoutContext().isPaintOffsetCacheEnabled()) {
        auto* layoutState = view().frameView().layoutContext().layoutState();
        LayoutSize offset = layoutState->paintOffset() + locationOffset();
        if (style().hasInFlowPosition() && layer())
            offset += layer()->offsetForInFlowPosition();
        transformState.move(offset);
        return;
    }

    bool containerSkipped;
    RenderElement* container = this->container(ancestorContainer, containerSkipped);
    if (!container)
        return;

    bool isFixedPos = isFixedPositioned();
    // If this box has a transform, it acts as a fixed position container for fixed descendants,
    // and may itself also be fixed position. So propagate 'fixed' up only if this box is fixed position.
    if (hasTransform() && !isFixedPos)
        mode.remove(IsFixed);
    else if (isFixedPos)
        mode.add(IsFixed);

    if (wasFixed)
        *wasFixed = mode.contains(IsFixed);
    
    LayoutSize containerOffset = offsetFromContainer(*container, LayoutPoint(transformState.mappedPoint()));

    // Remove sticky positioning from the offset if it should be ignored. This is done here in
    // order to avoid piping this flag down the method chain.
    if (mode.contains(IgnoreStickyOffsets) && isStickilyPositioned())
        containerOffset -= stickyPositionOffset();
    
    bool preserve3D = mode.contains(UseTransforms) && (container->style().preserves3D() || style().preserves3D());
    if (mode.contains(UseTransforms) && shouldUseTransformFromContainer(container)) {
        TransformationMatrix t;
        getTransformFromContainer(container, containerOffset, t);
        transformState.applyTransform(t, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    } else
        transformState.move(containerOffset.width(), containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);

    if (containerSkipped) {
        // There can't be a transform between ancestorContainer and o, because transforms create containers, so it should be safe
        // to just subtract the delta between the ancestorContainer and o.
        LayoutSize containerOffset = ancestorContainer->offsetFromAncestorContainer(*container);
        transformState.move(-containerOffset.width(), -containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
        return;
    }

    mode.remove(ApplyContainerFlip);

    container->mapLocalToContainer(ancestorContainer, transformState, mode, wasFixed);
}

const RenderObject* RenderBox::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    ASSERT(ancestorToStopAt != this);

    bool ancestorSkipped;
    RenderElement* container = this->container(ancestorToStopAt, ancestorSkipped);
    if (!container)
        return nullptr;

    bool isFixedPos = isFixedPositioned();
    LayoutSize adjustmentForSkippedAncestor;
    if (ancestorSkipped) {
        // There can't be a transform between repaintContainer and container, because transforms create containers, so it should be safe
        // to just subtract the delta between the ancestor and container.
        adjustmentForSkippedAncestor = -ancestorToStopAt->offsetFromAncestorContainer(*container);
    }

    bool offsetDependsOnPoint = false;
    LayoutSize containerOffset = offsetFromContainer(*container, LayoutPoint(), &offsetDependsOnPoint);

    bool preserve3D = container->style().preserves3D() || style().preserves3D();
    if (shouldUseTransformFromContainer(container) && (geometryMap.mapCoordinatesFlags() & UseTransforms)) {
        TransformationMatrix t;
        getTransformFromContainer(container, containerOffset, t);
        t.translateRight(adjustmentForSkippedAncestor.width(), adjustmentForSkippedAncestor.height());
        
        geometryMap.push(this, t, preserve3D, offsetDependsOnPoint, isFixedPos, hasTransform());
    } else {
        containerOffset += adjustmentForSkippedAncestor;
        geometryMap.push(this, containerOffset, preserve3D, offsetDependsOnPoint, isFixedPos, hasTransform());
    }
    
    return ancestorSkipped ? ancestorToStopAt : container;
}

void RenderBox::mapAbsoluteToLocalPoint(OptionSet<MapCoordinatesMode> mode, TransformState& transformState) const
{
    bool isFixedPos = isFixedPositioned();
    if (hasTransform() && !isFixedPos) {
        // If this box has a transform, it acts as a fixed position container for fixed descendants,
        // and may itself also be fixed position. So propagate 'fixed' up only if this box is fixed position.
        mode.remove(IsFixed);
    } else if (isFixedPos)
        mode.add(IsFixed);

    RenderBoxModelObject::mapAbsoluteToLocalPoint(mode, transformState);
}

LayoutSize RenderBox::offsetFromContainer(RenderElement& container, const LayoutPoint&, bool* offsetDependsOnPoint) const
{
    // A fragment "has" boxes inside it without being their container. 
    ASSERT(&container == this->container() || is<RenderFragmentContainer>(container));

    LayoutSize offset;    
    if (isInFlowPositioned())
        offset += offsetForInFlowPosition();

    if (!isInline() || isReplacedOrInlineBlock())
        offset += topLeftLocationOffset();

    if (is<RenderBox>(container))
        offset -= toLayoutSize(downcast<RenderBox>(container).scrollPosition());

    if (isAbsolutelyPositioned() && container.isInFlowPositioned() && is<RenderInline>(container))
        offset += downcast<RenderInline>(container).offsetForInFlowPositionedInline(this);

    if (offsetDependsOnPoint)
        *offsetDependsOnPoint |= is<RenderFragmentedFlow>(container);

    return offset;
}

std::unique_ptr<LegacyInlineElementBox> RenderBox::createInlineBox()
{
    return makeUnique<LegacyInlineElementBox>(*this);
}

void RenderBox::dirtyLineBoxes(bool fullLayout)
{
    if (!m_inlineBoxWrapper)
        return;
    
    if (fullLayout) {
        delete m_inlineBoxWrapper;
        m_inlineBoxWrapper = nullptr;
    } else
        m_inlineBoxWrapper->dirtyLineBoxes();
}

void RenderBox::positionLineBox(LegacyInlineElementBox& box)
{
    if (isOutOfFlowPositioned()) {
        // Cache the x position only if we were an DisplayType::Inline type originally.
        bool wasInline = style().isOriginalDisplayInlineType();
        if (wasInline) {
            // The value is cached in the xPos of the box.  We only need this value if
            // our object was inline originally, since otherwise it would have ended up underneath
            // the inlines.
            LegacyRootInlineBox& rootBox = box.root();
            rootBox.blockFlow().setStaticInlinePositionForChild(*this, rootBox.lineBoxTop(), LayoutUnit::fromFloatRound(box.logicalLeft()));
            if (style().hasStaticInlinePosition(box.isHorizontal()))
                setChildNeedsLayout(MarkOnlyThis); // Just mark the positioned object as needing layout, so it will update its position properly.
        } else {
            // Our object was a block originally, so we make our normal flow position be
            // just below the line box (as though all the inlines that came before us got
            // wrapped in an anonymous block, which is what would have happened had we been
            // in flow).  This value was cached in the y() of the box.
            layer()->setStaticBlockPosition(LayoutUnit(box.logicalTop()));
            if (style().hasStaticBlockPosition(box.isHorizontal()))
                setChildNeedsLayout(MarkOnlyThis); // Just mark the positioned object as needing layout, so it will update its position properly.
        }
        return;
    }

    if (isReplacedOrInlineBlock()) {
        setLocation(LayoutPoint(box.topLeft()));
        setInlineBoxWrapper(&box);
    }
}

void RenderBox::deleteLineBoxWrapper()
{
    if (!m_inlineBoxWrapper)
        return;
    
    if (!renderTreeBeingDestroyed())
        m_inlineBoxWrapper->removeFromParent();
    delete m_inlineBoxWrapper;
    m_inlineBoxWrapper = nullptr;
}

LayoutRect RenderBox::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const
{
    if (isInsideEntirelyHiddenLayer())
        return { };

    LayoutRect r = visualOverflowRect();
    // FIXME: layoutDelta needs to be applied in parts before/after transforms and
    // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
    r.move(view().frameView().layoutContext().layoutDelta());
    return computeRect(r, repaintContainer, context);
}

LayoutRect RenderBox::computeVisibleRectUsingPaintOffset(const LayoutRect& rect) const
{
    LayoutRect adjustedRect = rect;
    auto* layoutState = view().frameView().layoutContext().layoutState();

    if (layer() && layer()->transform())
        adjustedRect = LayoutRect(encloseRectToDevicePixels(layer()->transform()->mapRect(adjustedRect), document().deviceScaleFactor()));

    // We can't trust the bits on RenderObject, because this might be called while re-resolving style.
    if (style().hasInFlowPosition() && layer())
        adjustedRect.move(layer()->offsetForInFlowPosition());

    adjustedRect.moveBy(location());
    adjustedRect.move(layoutState->paintOffset());
    if (layoutState->isClipped())
        adjustedRect.intersect(layoutState->clipRect());
    return adjustedRect;
}

std::optional<LayoutRect> RenderBox::computeVisibleRectInContainer(const LayoutRect& rect, const RenderLayerModelObject* container, VisibleRectContext context) const
{
    // The rect we compute at each step is shifted by our x/y offset in the parent container's coordinate space.
    // Only when we cross a writing mode boundary will we have to possibly flipForWritingMode (to convert into a more appropriate
    // offset corner for the enclosing container).  This allows for a fully RL or BT document to repaint
    // properly even during layout, since the rect remains flipped all the way until the end.
    //
    // RenderView::computeVisibleRectInContainer then converts the rect to physical coordinates. We also convert to
    // physical when we hit a repaint container boundary. Therefore the final rect returned is always in the
    // physical coordinate space of the container.
    const RenderStyle& styleToUse = style();
    // Paint offset cache is only valid for root-relative, non-fixed position repainting
    if (view().frameView().layoutContext().isPaintOffsetCacheEnabled() && !container && styleToUse.position() != PositionType::Fixed && !context.options.contains(VisibleRectContextOption::UseEdgeInclusiveIntersection))
        return computeVisibleRectUsingPaintOffset(rect);

    LayoutRect adjustedRect = rect;
    if (hasReflection())
        adjustedRect.unite(reflectedRect(adjustedRect));

    if (container == this) {
        if (container->style().isFlippedBlocksWritingMode())
            flipForWritingMode(adjustedRect);
        if (context.descendantNeedsEnclosingIntRect)
            adjustedRect = enclosingIntRect(adjustedRect);
        return adjustedRect;
    }

    bool containerIsSkipped;
    auto* localContainer = this->container(container, containerIsSkipped);
    if (!localContainer)
        return adjustedRect;
    
    if (isWritingModeRoot()) {
        if (!isOutOfFlowPositioned() || !context.dirtyRectIsFlipped) {
            flipForWritingMode(adjustedRect);
            context.dirtyRectIsFlipped = true;
        }
    }

    LayoutSize locationOffset = this->locationOffset();
    // FIXME: This is needed as long as RenderWidget snaps to integral size/position.
    if (isRenderReplaced() && isWidget()) {
        LayoutSize flooredLocationOffset = toIntSize(flooredIntPoint(locationOffset));
        adjustedRect.expand(locationOffset - flooredLocationOffset);
        locationOffset = flooredLocationOffset;
        context.descendantNeedsEnclosingIntRect = true;
    }

    if (is<RenderMultiColumnFlow>(this)) {
        // We won't normally run this code. Only when the container is null (i.e., we're trying
        // to get the rect in view coordinates) will we come in here, since normally container
        // will be set and we'll stop at the flow thread. This case is mainly hit by the check for whether
        // or not images should animate.
        // FIXME: Just as with offsetFromContainer, we aren't really handling objects that span
        // multiple columns properly.
        LayoutPoint physicalPoint(flipForWritingMode(adjustedRect.location()));
        if (auto* fragment = downcast<RenderMultiColumnFlow>(*this).physicalTranslationFromFlowToFragment((physicalPoint))) {
            adjustedRect.setLocation(fragment->flipForWritingMode(physicalPoint));
            return fragment->computeVisibleRectInContainer(adjustedRect, container, context);
        }
    }

    LayoutPoint topLeft = adjustedRect.location();
    topLeft.move(locationOffset);

    // We are now in our parent container's coordinate space. Apply our transform to obtain a bounding box
    // in the parent's coordinate space that encloses us.
    auto position = styleToUse.position();
    if (hasLayer() && layer()->transform()) {
        context.hasPositionFixedDescendant = position == PositionType::Fixed;
        adjustedRect = LayoutRect(encloseRectToDevicePixels(layer()->transform()->mapRect(adjustedRect), document().deviceScaleFactor()));
        topLeft = adjustedRect.location();
        topLeft.move(locationOffset);
    } else if (position == PositionType::Fixed)
        context.hasPositionFixedDescendant = true;

    if (position == PositionType::Absolute && localContainer->isInFlowPositioned() && is<RenderInline>(*localContainer))
        topLeft += downcast<RenderInline>(*localContainer).offsetForInFlowPositionedInline(this);
    else if (styleToUse.hasInFlowPosition() && layer()) {
        // Apply the relative position offset when invalidating a rectangle.  The layer
        // is translated, but the render box isn't, so we need to do this to get the
        // right dirty rect.  Since this is called from RenderObject::setStyle, the relative position
        // flag on the RenderObject has been cleared, so use the one on the style().
        topLeft += layer()->offsetForInFlowPosition();
    }

    // FIXME: We ignore the lightweight clipping rect that controls use, since if |o| is in mid-layout,
    // its controlClipRect will be wrong. For overflow clip we use the values cached by the layer.
    adjustedRect.setLocation(topLeft);
    if (localContainer->hasNonVisibleOverflow()) {
        RenderBox& containerBox = downcast<RenderBox>(*localContainer);
        bool isEmpty = !containerBox.applyCachedClipAndScrollPosition(adjustedRect, container, context);
        if (isEmpty) {
            if (context.options.contains(VisibleRectContextOption::UseEdgeInclusiveIntersection))
                return std::nullopt;
            return adjustedRect;
        }
    }

    if (containerIsSkipped) {
        // If the container is below localContainer, then we need to map the rect into container's coordinates.
        LayoutSize containerOffset = container->offsetFromAncestorContainer(*localContainer);
        adjustedRect.move(-containerOffset);
        return adjustedRect;
    }
    return localContainer->computeVisibleRectInContainer(adjustedRect, container, context);
}

void RenderBox::repaintDuringLayoutIfMoved(const LayoutRect& oldRect)
{
    if (oldRect.location() != m_frameRect.location()) {
        LayoutRect newRect = m_frameRect;
        // The child moved.  Invalidate the object's old and new positions.  We have to do this
        // since the object may not have gotten a layout.
        m_frameRect = oldRect;
        repaint();
        repaintOverhangingFloats(true);
        m_frameRect = newRect;
        repaint();
        repaintOverhangingFloats(true);
    }
}

void RenderBox::repaintOverhangingFloats(bool)
{
}

void RenderBox::updateLogicalWidth()
{
    LogicalExtentComputedValues computedValues;
    computeLogicalWidthInFragment(computedValues);

    setLogicalWidth(computedValues.m_extent);
    setLogicalLeft(computedValues.m_position);
    setMarginStart(computedValues.m_margins.m_start);
    setMarginEnd(computedValues.m_margins.m_end);
}

static LayoutUnit inlineSizeFromAspectRatio(LayoutUnit borderPaddingInlineSum, LayoutUnit borderPaddingBlockSum, double aspectRatio, BoxSizing boxSizing, LayoutUnit blockSize)
{
    if (boxSizing == BoxSizing::BorderBox)
        return LayoutUnit(blockSize * aspectRatio);

    return LayoutUnit((blockSize - borderPaddingBlockSum) * aspectRatio) + borderPaddingInlineSum;
}

void RenderBox::computeLogicalWidthInFragment(LogicalExtentComputedValues& computedValues, RenderFragmentContainer* fragment) const
{
    computedValues.m_extent = logicalWidth();
    computedValues.m_position = logicalLeft();
    computedValues.m_margins.m_start = marginStart();
    computedValues.m_margins.m_end = marginEnd();

    if (isOutOfFlowPositioned()) {
        // FIXME: This calculation is not patched for block-flow yet.
        // https://bugs.webkit.org/show_bug.cgi?id=46500
        computePositionedLogicalWidth(computedValues, fragment);
        return;
    }

    // If layout is limited to a subtree, the subtree root's logical width does not change.
    if (element() && !view().frameView().layoutContext().isLayoutPending() && view().frameView().layoutContext().subtreeLayoutRoot() == this)
        return;

    // The parent box is flexing us, so it has increased or decreased our
    // width.  Use the width from the style context.
    // FIXME: Account for block-flow in flexible boxes.
    // https://bugs.webkit.org/show_bug.cgi?id=46418
    if (hasOverridingLogicalWidth() && (isRubyRun() || (parent()->isFlexibleBoxIncludingDeprecated()))) {
        computedValues.m_extent = overridingLogicalWidth();
        return;
    }

    // FIXME: Account for block-flow in flexible boxes.
    // https://bugs.webkit.org/show_bug.cgi?id=46418
    bool inVerticalBox = parent()->isDeprecatedFlexibleBox() && (parent()->style().boxOrient() == BoxOrient::Vertical);
    bool stretching = (parent()->style().boxAlign() == BoxAlignment::Stretch);
    // FIXME: Stretching is the only reason why we don't want the box to be treated as a replaced element, so we could perhaps
    // refactor all this logic, not only for flex and grid since alignment is intended to be applied to any block.
    bool treatAsReplaced = shouldComputeSizeAsReplaced() && (!inVerticalBox || !stretching);
    treatAsReplaced = treatAsReplaced && (!isGridItem() || !hasStretchedLogicalWidth());

    const RenderStyle& styleToUse = style();
    Length logicalWidthLength;
    if (hasOverridingLogicalWidthLength())
        logicalWidthLength = overridingLogicalWidthLength();
    else
        logicalWidthLength = treatAsReplaced ? Length(computeReplacedLogicalWidth(), LengthType::Fixed) : styleToUse.logicalWidth();

    RenderBlock& cb = *containingBlock();
    LayoutUnit containerLogicalWidth = std::max<LayoutUnit>(0, containingBlockLogicalWidthForContentInFragment(fragment));
    bool hasPerpendicularContainingBlock = cb.isHorizontalWritingMode() != isHorizontalWritingMode();

    if (isInline() && !isInlineBlockOrInlineTable()) {
        // just calculate margins
        computedValues.m_margins.m_start = minimumValueForLength(styleToUse.marginStart(), containerLogicalWidth);
        computedValues.m_margins.m_end = minimumValueForLength(styleToUse.marginEnd(), containerLogicalWidth);
        if (treatAsReplaced)
            computedValues.m_extent = std::max(LayoutUnit(floatValueForLength(logicalWidthLength, 0) + borderAndPaddingLogicalWidth()), minPreferredLogicalWidth());
        return;
    }

    LayoutUnit containerWidthInInlineDirection = containerLogicalWidth;
    if (hasPerpendicularContainingBlock)
        containerWidthInInlineDirection = perpendicularContainingBlockLogicalHeight();

    // Width calculations
    if (treatAsReplaced) {
        computedValues.m_extent = logicalWidthLength.value() + borderAndPaddingLogicalWidth();
    } else if (shouldComputeLogicalWidthFromAspectRatio() && style().logicalWidth().isAuto()) {
        computedValues.m_extent = computeLogicalWidthFromAspectRatio(fragment);
    } else {
        LayoutUnit preferredWidth = computeLogicalWidthInFragmentUsing(MainOrPreferredSize, hasOverridingLogicalWidthLength() ? logicalWidthLength : styleToUse.logicalWidth(), containerWidthInInlineDirection, cb, fragment);
        computedValues.m_extent = constrainLogicalWidthInFragmentByMinMax(preferredWidth, containerWidthInInlineDirection, cb, fragment);
    }

    // Margin calculations.
    if (hasPerpendicularContainingBlock || isFloating() || isInline()) {
        computedValues.m_margins.m_start = minimumValueForLength(styleToUse.marginStart(), containerLogicalWidth);
        computedValues.m_margins.m_end = minimumValueForLength(styleToUse.marginEnd(), containerLogicalWidth);
    } else {
        LayoutUnit containerLogicalWidthForAutoMargins = containerLogicalWidth;
        if (avoidsFloats() && cb.containsFloats())
            containerLogicalWidthForAutoMargins = containingBlockAvailableLineWidthInFragment(fragment);
        bool hasInvertedDirection = cb.style().isLeftToRightDirection() != style().isLeftToRightDirection();
        computeInlineDirectionMargins(cb, containerLogicalWidthForAutoMargins, computedValues.m_extent,
            hasInvertedDirection ? computedValues.m_margins.m_end : computedValues.m_margins.m_start,
            hasInvertedDirection ? computedValues.m_margins.m_start : computedValues.m_margins.m_end);
    }
    
    if (!hasPerpendicularContainingBlock && containerLogicalWidth && containerLogicalWidth != (computedValues.m_extent + computedValues.m_margins.m_start + computedValues.m_margins.m_end)
        && !isFloating() && !isInline() && !cb.isFlexibleBoxIncludingDeprecated()
#if ENABLE(MATHML)
        // RenderMathMLBlocks take the size of their content so we must not adjust the margin to fill the container size.
        && !cb.isRenderMathMLBlock()
#endif
        && !cb.isRenderGrid()
        ) {
        LayoutUnit newMarginTotal = containerLogicalWidth - computedValues.m_extent;
        bool hasInvertedDirection = cb.style().isLeftToRightDirection() != style().isLeftToRightDirection();
        if (hasInvertedDirection)
            computedValues.m_margins.m_start = newMarginTotal - computedValues.m_margins.m_end;
        else
            computedValues.m_margins.m_end = newMarginTotal - computedValues.m_margins.m_start;
    }
}

LayoutUnit RenderBox::fillAvailableMeasure(LayoutUnit availableLogicalWidth) const
{
    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    return fillAvailableMeasure(availableLogicalWidth, marginStart, marginEnd);
}

LayoutUnit RenderBox::fillAvailableMeasure(LayoutUnit availableLogicalWidth, LayoutUnit& marginStart, LayoutUnit& marginEnd) const
{
    bool isOrthogonalElement = isHorizontalWritingMode() != containingBlock()->isHorizontalWritingMode();
    LayoutUnit availableSizeForResolvingMargin = isOrthogonalElement ? containingBlockLogicalWidthForContent() : availableLogicalWidth;
    marginStart = minimumValueForLength(style().marginStart(), availableSizeForResolvingMargin);
    marginEnd = minimumValueForLength(style().marginEnd(), availableSizeForResolvingMargin);
    return availableLogicalWidth - marginStart - marginEnd;
}

LayoutUnit RenderBox::computeIntrinsicLogicalWidthUsing(Length logicalWidthLength, LayoutUnit availableLogicalWidth, LayoutUnit borderAndPadding) const
{
    if (logicalWidthLength.isFillAvailable())
        return std::max(borderAndPadding, fillAvailableMeasure(availableLogicalWidth));

    LayoutUnit minLogicalWidth;
    LayoutUnit maxLogicalWidth;
    if (!logicalWidthLength.isMinIntrinsic() && shouldComputeLogicalWidthFromAspectRatio()) {
        minLogicalWidth = maxLogicalWidth = computeLogicalWidthFromAspectRatioInternal() - borderAndPadding;
        if (firstChild()) {
            LayoutUnit minChildrenLogicalWidth;
            LayoutUnit maxChildrenLogicalWidth;
            computeIntrinsicKeywordLogicalWidths(minChildrenLogicalWidth, maxChildrenLogicalWidth);
            minLogicalWidth = std::max(minLogicalWidth, minChildrenLogicalWidth);
            maxLogicalWidth = std::max(maxLogicalWidth, maxChildrenLogicalWidth);
        }
    } else
        computeIntrinsicKeywordLogicalWidths(minLogicalWidth, maxLogicalWidth);

    if (logicalWidthLength.isMinContent() || logicalWidthLength.isMinIntrinsic())
        return minLogicalWidth + borderAndPadding;

    if (logicalWidthLength.isMaxContent())
        return maxLogicalWidth + borderAndPadding;

    if (logicalWidthLength.isFitContent()) {
        minLogicalWidth += borderAndPadding;
        maxLogicalWidth += borderAndPadding;
        return std::max(minLogicalWidth, std::min(maxLogicalWidth, fillAvailableMeasure(availableLogicalWidth)));
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutUnit RenderBox::computeLogicalWidthInFragmentUsing(SizeType widthType, Length logicalWidth, LayoutUnit availableLogicalWidth,
    const RenderBlock& cb, RenderFragmentContainer* fragment) const
{
    ASSERT(widthType == MinSize || widthType == MainOrPreferredSize || !logicalWidth.isAuto());
    if (widthType == MinSize && logicalWidth.isAuto())
        return adjustBorderBoxLogicalWidthForBoxSizing(0, logicalWidth.type());

    if (!logicalWidth.isIntrinsicOrAuto()) {
        // FIXME: If the containing block flow is perpendicular to our direction we need to use the available logical height instead.
        return adjustBorderBoxLogicalWidthForBoxSizing(valueForLength(logicalWidth, availableLogicalWidth), logicalWidth.type());
    }

    if (logicalWidth.isIntrinsic() || logicalWidth.isMinIntrinsic())
        return computeIntrinsicLogicalWidthUsing(logicalWidth, availableLogicalWidth, borderAndPaddingLogicalWidth());

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    LayoutUnit logicalWidthResult = fillAvailableMeasure(availableLogicalWidth, marginStart, marginEnd);

    if (shrinkToAvoidFloats() && cb.containsFloats())
        logicalWidthResult = std::min(logicalWidthResult, shrinkLogicalWidthToAvoidFloats(marginStart, marginEnd, cb, fragment));

    if (widthType == MainOrPreferredSize && sizesLogicalWidthToFitContent(widthType))
        return std::max(minPreferredLogicalWidth(), std::min(maxPreferredLogicalWidth(), logicalWidthResult));
    return logicalWidthResult;
}

bool RenderBox::columnFlexItemHasStretchAlignment() const
{
    // auto margins mean we don't stretch. Note that this function will only be
    // used for widths, so we don't have to check marginBefore/marginAfter.
    const auto& parentStyle = parent()->style();
    ASSERT(parentStyle.isColumnFlexDirection());
    if (style().marginStart().isAuto() || style().marginEnd().isAuto())
        return false;
    return style().resolvedAlignSelf(&parentStyle, containingBlock()->selfAlignmentNormalBehavior()).position() == ItemPosition::Stretch;
}

bool RenderBox::isStretchingColumnFlexItem() const
{
    if (parent()->isDeprecatedFlexibleBox() && parent()->style().boxOrient() == BoxOrient::Vertical && parent()->style().boxAlign() == BoxAlignment::Stretch)
        return true;

    // We don't stretch multiline flexboxes because they need to apply line spacing (align-content) first.
    if (parent()->isFlexibleBox() && parent()->style().flexWrap() == FlexWrap::NoWrap && parent()->style().isColumnFlexDirection() && columnFlexItemHasStretchAlignment())
        return true;
    return false;
}

// FIXME: Can/Should we move this inside specific layout classes (flex. grid)? Can we refactor columnFlexItemHasStretchAlignment logic?
bool RenderBox::hasStretchedLogicalHeight() const
{
    auto& style = this->style();
    if (!style.logicalHeight().isAuto() || style.marginBefore().isAuto() || style.marginAfter().isAuto())
        return false;
    RenderBlock* containingBlock = this->containingBlock();
    if (!containingBlock) {
        // We are evaluating align-self/justify-self, which default to 'normal' for the root element.
        // The 'normal' value behaves like 'start' except for Flexbox Items, which obviously should have a container.
        return false;
    }
    if (containingBlock->isHorizontalWritingMode() != isHorizontalWritingMode())
        return style.resolvedJustifySelf(&containingBlock->style(), containingBlock->selfAlignmentNormalBehavior(this)).position() == ItemPosition::Stretch;
    return style.resolvedAlignSelf(&containingBlock->style(), containingBlock->selfAlignmentNormalBehavior(this)).position() == ItemPosition::Stretch;
}

// FIXME: Can/Should we move this inside specific layout classes (flex. grid)? Can we refactor columnFlexItemHasStretchAlignment logic?
bool RenderBox::hasStretchedLogicalWidth(StretchingMode stretchingMode) const
{
    auto& style = this->style();
    if (!style.logicalWidth().isAuto() || style.marginStart().isAuto() || style.marginEnd().isAuto())
        return false;
    RenderBlock* containingBlock = this->containingBlock();
    if (!containingBlock) {
        // We are evaluating align-self/justify-self, which default to 'normal' for the root element.
        // The 'normal' value behaves like 'start' except for Flexbox Items, which obviously should have a container.
        return false;
    }
    auto normalItemPosition = stretchingMode == StretchingMode::Any ? containingBlock->selfAlignmentNormalBehavior(this) : ItemPosition::Normal;
    if (containingBlock->isHorizontalWritingMode() != isHorizontalWritingMode())
        return style.resolvedAlignSelf(&containingBlock->style(), normalItemPosition).position() == ItemPosition::Stretch;
    return style.resolvedJustifySelf(&containingBlock->style(), normalItemPosition).position() == ItemPosition::Stretch;
}

bool RenderBox::sizesLogicalWidthToFitContent(SizeType widthType) const
{
    // Marquees in WinIE are like a mixture of blocks and inline-blocks.  They size as though they're blocks,
    // but they allow text to sit on the same line as the marquee.
    if (isFloating() || (isInlineBlockOrInlineTable() && !isHTMLMarquee()))
        return true;

    if (isGridItem())
        return !hasStretchedLogicalWidth();

    // This code may look a bit strange.  Basically width:intrinsic should clamp the size when testing both
    // min-width and width.  max-width is only clamped if it is also intrinsic.
    Length logicalWidth = (widthType == MaxSize) ? style().logicalMaxWidth() : style().logicalWidth();
    if (logicalWidth.type() == LengthType::Intrinsic)
        return true;

    // Children of a horizontal marquee do not fill the container by default.
    // FIXME: Need to deal with MarqueeDirection::Auto value properly. It could be vertical.
    // FIXME: Think about block-flow here.  Need to find out how marquee direction relates to
    // block-flow (as well as how marquee overflow should relate to block flow).
    // https://bugs.webkit.org/show_bug.cgi?id=46472
    if (parent()->isHTMLMarquee()) {
        MarqueeDirection dir = parent()->style().marqueeDirection();
        if (dir == MarqueeDirection::Auto || dir == MarqueeDirection::Forward || dir == MarqueeDirection::Backward || dir == MarqueeDirection::Left || dir == MarqueeDirection::Right)
            return true;
    }

#if ENABLE(MATHML)
    // RenderMathMLBlocks take the size of their content, not of their container.
    if (parent()->isRenderMathMLBlock())
        return true;
#endif

    // Flexible box items should shrink wrap, so we lay them out at their intrinsic widths.
    // In the case of columns that have a stretch alignment, we layout at the stretched size
    // to avoid an extra layout when applying alignment.
    if (parent()->isFlexibleBox()) {
        // For multiline columns, we need to apply align-content first, so we can't stretch now.
        if (!parent()->style().isColumnFlexDirection() || parent()->style().flexWrap() != FlexWrap::NoWrap)
            return true;
        if (!columnFlexItemHasStretchAlignment())
            return true;
    }

    // Flexible horizontal boxes lay out children at their intrinsic widths.  Also vertical boxes
    // that don't stretch their kids lay out their children at their intrinsic widths.
    // FIXME: Think about block-flow here.
    // https://bugs.webkit.org/show_bug.cgi?id=46473
    if (parent()->isDeprecatedFlexibleBox() && (parent()->style().boxOrient() == BoxOrient::Horizontal || parent()->style().boxAlign() != BoxAlignment::Stretch))
        return true;

    // Button, input, select, textarea, and legend treat width value of 'auto' as 'intrinsic' unless it's in a
    // stretching column flexbox.
    // FIXME: Think about block-flow here.
    // https://bugs.webkit.org/show_bug.cgi?id=46473
    if (logicalWidth.isAuto() && !isStretchingColumnFlexItem() && element() && (is<HTMLInputElement>(*element()) || is<HTMLSelectElement>(*element()) || is<HTMLButtonElement>(*element()) || is<HTMLTextAreaElement>(*element()) || is<HTMLLegendElement>(*element())))
        return true;

    if (isHorizontalWritingMode() != containingBlock()->isHorizontalWritingMode())
        return true;

    return false;
}

void RenderBox::computeInlineDirectionMargins(const RenderBlock& containingBlock, LayoutUnit containerWidth, LayoutUnit childWidth, LayoutUnit& marginStart, LayoutUnit& marginEnd) const
{
    
    const RenderStyle& containingBlockStyle = containingBlock.style();
    Length marginStartLength = style().marginStartUsing(&containingBlockStyle);
    Length marginEndLength = style().marginEndUsing(&containingBlockStyle);

    if (isFloating() || isInline()) {
        // Inline blocks/tables and floats don't have their margins increased.
        marginStart = minimumValueForLength(marginStartLength, containerWidth);
        marginEnd = minimumValueForLength(marginEndLength, containerWidth);
        return;
    }

    if (containingBlock.isFlexibleBox()) {
        // We need to let flexbox handle the margin adjustment - otherwise, flexbox
        // will think we're wider than we actually are and calculate line sizes
        // wrong. See also http://dev.w3.org/csswg/css-flexbox/#auto-margins
        if (marginStartLength.isAuto())
            marginStartLength = Length(0, LengthType::Fixed);
        if (marginEndLength.isAuto())
            marginEndLength = Length(0, LengthType::Fixed);
    }

    // Case One: The object is being centered in the containing block's available logical width.
    if ((marginStartLength.isAuto() && marginEndLength.isAuto() && childWidth < containerWidth)
        || (!marginStartLength.isAuto() && !marginEndLength.isAuto() && containingBlock.style().textAlign() == TextAlignMode::WebKitCenter)) {
        // Other browsers center the margin box for align=center elements so we match them here.
        LayoutUnit marginStartWidth = minimumValueForLength(marginStartLength, containerWidth);
        LayoutUnit marginEndWidth = minimumValueForLength(marginEndLength, containerWidth);
        LayoutUnit centeredMarginBoxStart = std::max<LayoutUnit>(0, (containerWidth - childWidth - marginStartWidth - marginEndWidth) / 2);
        marginStart = centeredMarginBoxStart + marginStartWidth;
        marginEnd = containerWidth - childWidth - marginStart + marginEndWidth;
        return;
    } 
    
    // Case Two: The object is being pushed to the start of the containing block's available logical width.
    if (marginEndLength.isAuto() && childWidth < containerWidth) {
        marginStart = valueForLength(marginStartLength, containerWidth);
        marginEnd = containerWidth - childWidth - marginStart;
        return;
    } 
    
    // Case Three: The object is being pushed to the end of the containing block's available logical width.
    bool pushToEndFromTextAlign = !marginEndLength.isAuto() && ((!containingBlockStyle.isLeftToRightDirection() && containingBlockStyle.textAlign() == TextAlignMode::WebKitLeft)
        || (containingBlockStyle.isLeftToRightDirection() && containingBlockStyle.textAlign() == TextAlignMode::WebKitRight));
    if ((marginStartLength.isAuto() || pushToEndFromTextAlign) && childWidth < containerWidth) {
        marginEnd = valueForLength(marginEndLength, containerWidth);
        marginStart = containerWidth - childWidth - marginEnd;
        return;
    } 
    
    // Case Four: Either no auto margins, or our width is >= the container width (css2.1, 10.3.3).  In that case
    // auto margins will just turn into 0.
    marginStart = minimumValueForLength(marginStartLength, containerWidth);
    marginEnd = minimumValueForLength(marginEndLength, containerWidth);
}

RenderBoxFragmentInfo* RenderBox::renderBoxFragmentInfo(RenderFragmentContainer* fragment, RenderBoxFragmentInfoFlags cacheFlag) const
{
    // Make sure nobody is trying to call this with a null fragment.
    if (!fragment)
        return nullptr;

    // If we have computed our width in this fragment already, it will be cached, and we can
    // just return it.
    RenderBoxFragmentInfo* boxInfo = fragment->renderBoxFragmentInfo(this);
    if (boxInfo && cacheFlag == CacheRenderBoxFragmentInfo)
        return boxInfo;

    return nullptr;
}

static bool shouldFlipBeforeAfterMargins(const RenderStyle& containingBlockStyle, const RenderStyle* childStyle)
{
    ASSERT(containingBlockStyle.isHorizontalWritingMode() != childStyle->isHorizontalWritingMode());
    WritingMode childWritingMode = childStyle->writingMode();
    bool shouldFlip = false;
    switch (containingBlockStyle.writingMode()) {
    case WritingMode::TopToBottom:
        shouldFlip = (childWritingMode == WritingMode::RightToLeft);
        break;
    case WritingMode::BottomToTop:
        shouldFlip = (childWritingMode == WritingMode::RightToLeft);
        break;
    case WritingMode::RightToLeft:
        shouldFlip = (childWritingMode == WritingMode::BottomToTop);
        break;
    case WritingMode::LeftToRight:
        shouldFlip = (childWritingMode == WritingMode::BottomToTop);
        break;
    }

    if (!containingBlockStyle.isLeftToRightDirection())
        shouldFlip = !shouldFlip;

    return shouldFlip;
}

void RenderBox::cacheIntrinsicContentLogicalHeightForFlexItem(LayoutUnit height) const
{
    // FIXME: it should be enough with checking hasOverridingLogicalHeight() as this logic could be shared
    // by any layout system using overrides like grid or flex. However this causes a never ending sequence of calls
    // between layoutBlock() <-> relayoutToAvoidWidows().
    if (isFloatingOrOutOfFlowPositioned() || !parent() || !parent()->isFlexibleBox() || hasOverridingLogicalHeight() || shouldComputeLogicalHeightFromAspectRatio())
        return;
    downcast<RenderFlexibleBox>(parent())->setCachedChildIntrinsicContentLogicalHeight(*this, height);
}

void RenderBox::updateLogicalHeight()
{
    if (shouldApplySizeContainment(*this) && !isRenderGrid()) {
        // We need the exact width of border and padding here, yet we can't use borderAndPadding* interfaces.
        // Because these interfaces evetually call borderAfter/Before, and RenderBlock::borderBefore
        // adds extra border to fieldset by adding intrinsicBorderForFieldset which is not needed here.
        auto borderAndPadding = RenderBox::borderBefore() + RenderBox::paddingBefore() + RenderBox::borderAfter() + RenderBox::paddingAfter();
        setLogicalHeight(borderAndPadding + scrollbarLogicalHeight());
    }

    cacheIntrinsicContentLogicalHeightForFlexItem(contentLogicalHeight());
    auto computedValues = computeLogicalHeight(logicalHeight(), logicalTop());
    setLogicalHeight(computedValues.m_extent);
    setLogicalTop(computedValues.m_position);
    setMarginBefore(computedValues.m_margins.m_before);
    setMarginAfter(computedValues.m_margins.m_after);
}

RenderBox::LogicalExtentComputedValues RenderBox::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const
{
    LogicalExtentComputedValues computedValues;
    computedValues.m_extent = logicalHeight;
    computedValues.m_position = logicalTop;

    // Cell height is managed by the table and inline non-replaced elements do not support a height property.
    if (isTableCell() || (isInline() && !isReplacedOrInlineBlock()))
        return computedValues;

    Length h;
    if (isOutOfFlowPositioned())
        computePositionedLogicalHeight(computedValues);
    else {
        RenderBlock& cb = *containingBlock();
        bool hasPerpendicularContainingBlock = cb.isHorizontalWritingMode() != isHorizontalWritingMode();
    
        if (!hasPerpendicularContainingBlock) {
            bool shouldFlipBeforeAfter = cb.style().writingMode() != style().writingMode();
            computeBlockDirectionMargins(cb,
                shouldFlipBeforeAfter ? computedValues.m_margins.m_after : computedValues.m_margins.m_before,
                shouldFlipBeforeAfter ? computedValues.m_margins.m_before : computedValues.m_margins.m_after);
        }

        // For tables, calculate margins only.
        if (isTable()) {
            if (shouldComputeLogicalHeightFromAspectRatio())
                computedValues.m_extent = blockSizeFromAspectRatio(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent(), style().logicalAspectRatio(), style().boxSizingForAspectRatio(), logicalWidth());
            if (hasPerpendicularContainingBlock) {
                bool shouldFlipBeforeAfter = shouldFlipBeforeAfterMargins(cb.style(), &style());
                computeInlineDirectionMargins(cb, containingBlockLogicalWidthForContent(), computedValues.m_extent,
                    shouldFlipBeforeAfter ? computedValues.m_margins.m_after : computedValues.m_margins.m_before,
                    shouldFlipBeforeAfter ? computedValues.m_margins.m_before : computedValues.m_margins.m_after);
            }
            return computedValues;
        }

        // FIXME: Account for block-flow in flexible boxes.
        // https://bugs.webkit.org/show_bug.cgi?id=46418
        bool inHorizontalBox = parent()->isDeprecatedFlexibleBox() && parent()->style().boxOrient() == BoxOrient::Horizontal;
        bool stretching = parent()->style().boxAlign() == BoxAlignment::Stretch;
        bool treatAsReplaced = shouldComputeSizeAsReplaced() && (!inHorizontalBox || !stretching);
        bool checkMinMaxHeight = false;
        const auto& logicalHeightInUse = hasOverridingLogicalHeightLength() ? overridingLogicalHeightLength() : style().logicalHeight();

        // The parent box is flexing us, so it has increased or decreased our height.  We have to
        // grab our cached flexible height.
        // FIXME: Account for block-flow in flexible boxes.
        // https://bugs.webkit.org/show_bug.cgi?id=46418
        if (hasOverridingLogicalHeight() && (parent()->isFlexibleBoxIncludingDeprecated() || parent()->isRenderGrid())) {
            h = Length(overridingLogicalHeight(), LengthType::Fixed);
        } else if (treatAsReplaced)
            h = Length(computeReplacedLogicalHeight() + borderAndPaddingLogicalHeight(), LengthType::Fixed);
        else {
            h = logicalHeightInUse;
            checkMinMaxHeight = true;
        }

        // Block children of horizontal flexible boxes fill the height of the box.
        // FIXME: Account for block-flow in flexible boxes.
        // https://bugs.webkit.org/show_bug.cgi?id=46418
        if (h.isAuto() && is<RenderDeprecatedFlexibleBox>(*parent()) && parent()->style().boxOrient() == BoxOrient::Horizontal
                && downcast<RenderDeprecatedFlexibleBox>(*parent()).isStretchingChildren()) {
            h = Length(parentBox()->contentLogicalHeight() - marginBefore() - marginAfter(), LengthType::Fixed);
            checkMinMaxHeight = false;
        }

        LayoutUnit heightResult;
        if (checkMinMaxHeight) {
            // Callers passing LayoutUnit::max() for logicalHeight means an indefinite height, so
            // translate this to a nullopt intrinsic height for further logical height computations.
            std::optional<LayoutUnit> intrinsicHeight;
            if (computedValues.m_extent != LayoutUnit::max())
                intrinsicHeight = computedValues.m_extent;
            if (shouldComputeLogicalHeightFromAspectRatio()) {
                if (intrinsicHeight && style().boxSizingForAspectRatio() == BoxSizing::ContentBox)
                    *intrinsicHeight -= borderAndPaddingLogicalHeight();
                heightResult = blockSizeFromAspectRatio(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent(), style().logicalAspectRatio(), style().boxSizingForAspectRatio(), logicalWidth());
            } else {
                if (intrinsicHeight)
                    *intrinsicHeight -= borderAndPaddingLogicalHeight();
                heightResult = computeLogicalHeightUsing(MainOrPreferredSize, logicalHeightInUse, intrinsicHeight).value_or(computedValues.m_extent);
            }
            heightResult = constrainLogicalHeightByMinMax(heightResult, intrinsicHeight);
        } else {
            ASSERT(h.isFixed());
            heightResult = h.value();
        }

        computedValues.m_extent = heightResult;

        if (hasPerpendicularContainingBlock) {
            bool shouldFlipBeforeAfter = shouldFlipBeforeAfterMargins(cb.style(), &style());
            computeInlineDirectionMargins(cb, containingBlockLogicalWidthForContent(), heightResult,
                    shouldFlipBeforeAfter ? computedValues.m_margins.m_after : computedValues.m_margins.m_before,
                    shouldFlipBeforeAfter ? computedValues.m_margins.m_before : computedValues.m_margins.m_after);
        }
    }

    // WinIE quirk: The <html> block always fills the entire canvas in quirks mode.  The <body> always fills the
    // <html> block in quirks mode.  Only apply this quirk if the block is normal flow and no height
    // is specified. When we're printing, we also need this quirk if the body or root has a percentage 
    // height since we don't set a height in RenderView when we're printing. So without this quirk, the 
    // height has nothing to be a percentage of, and it ends up being 0. That is bad.
    bool paginatedContentNeedsBaseHeight = document().printing() && h.isPercentOrCalculated()
        && (isDocumentElementRenderer() || (isBody() && document().documentElement()->renderer()->style().logicalHeight().isPercentOrCalculated())) && !isInline();
    if (stretchesToViewport() || paginatedContentNeedsBaseHeight) {
        LayoutUnit margins = collapsedMarginBefore() + collapsedMarginAfter();
        LayoutUnit visibleHeight = view().pageOrViewLogicalHeight();
        if (isDocumentElementRenderer())
            computedValues.m_extent = std::max(computedValues.m_extent, visibleHeight - margins);
        else {
            LayoutUnit marginsBordersPadding = margins + parentBox()->marginBefore() + parentBox()->marginAfter() + parentBox()->borderAndPaddingLogicalHeight();
            computedValues.m_extent = std::max(computedValues.m_extent, visibleHeight - marginsBordersPadding);
        }
    }
    return computedValues;
}

LayoutUnit RenderBox::computeLogicalHeightWithoutLayout() const
{
    // FIXME:: We should probably return something other than just
    // border + padding, but for now we have no good way to do anything else
    // without layout, so we just use that.
    LogicalExtentComputedValues computedValues = computeLogicalHeight(borderAndPaddingLogicalHeight(), 0_lu);
    return computedValues.m_extent;
}

std::optional<LayoutUnit> RenderBox::computeLogicalHeightUsing(SizeType heightType, const Length& height, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    if (std::optional<LayoutUnit> logicalHeight = computeContentAndScrollbarLogicalHeightUsing(heightType, height, intrinsicContentHeight))
        return adjustBorderBoxLogicalHeightForBoxSizing(logicalHeight.value());
    return std::nullopt;
}

std::optional<LayoutUnit> RenderBox::computeContentLogicalHeight(SizeType heightType, const Length& height, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    if (std::optional<LayoutUnit> heightIncludingScrollbar = computeContentAndScrollbarLogicalHeightUsing(heightType, height, intrinsicContentHeight))
        return std::max<LayoutUnit>(0, adjustContentBoxLogicalHeightForBoxSizing(heightIncludingScrollbar) - scrollbarLogicalHeight());
    return std::nullopt;
}

std::optional<LayoutUnit> RenderBox::computeIntrinsicLogicalContentHeightUsing(Length logicalHeightLength, std::optional<LayoutUnit> intrinsicContentHeight, LayoutUnit borderAndPadding) const
{
    // FIXME: The CSS sizing spec is considering changing what min-content/max-content should resolve to.
    // If that happens, this code will have to change.
    if (logicalHeightLength.isMinContent() || logicalHeightLength.isMaxContent() || logicalHeightLength.isFitContent() || logicalHeightLength.isLegacyIntrinsic()) {
        if (intrinsicContentHeight && style().boxSizing() == BoxSizing::BorderBox)
            return intrinsicContentHeight.value() + borderAndPaddingLogicalHeight();
        return intrinsicContentHeight;
    }
    if (logicalHeightLength.isFillAvailable())
        return containingBlock()->availableLogicalHeight(ExcludeMarginBorderPadding) - borderAndPadding;
    ASSERT_NOT_REACHED();
    return 0_lu;
}

std::optional<LayoutUnit> RenderBox::computeContentAndScrollbarLogicalHeightUsing(SizeType heightType, const Length& height, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    if (height.isAuto())
        return heightType == MinSize ? std::optional<LayoutUnit>(0) : std::nullopt;
    // FIXME: The CSS sizing spec is considering changing what min-content/max-content should resolve to.
    // If that happens, this code will have to change.
    if (height.isIntrinsic() || height.isLegacyIntrinsic())
        return computeIntrinsicLogicalContentHeightUsing(height, intrinsicContentHeight, borderAndPaddingLogicalHeight());
    if (height.isFixed())
        return LayoutUnit(height.value());
    if (height.isPercentOrCalculated())
        return computePercentageLogicalHeight(height);
    return std::nullopt;
}

bool RenderBox::skipContainingBlockForPercentHeightCalculation(const RenderBox& containingBlock, bool isPerpendicularWritingMode) const
{
    // Flow threads for multicol or paged overflow should be skipped. They are invisible to the DOM,
    // and percent heights of children should be resolved against the multicol or paged container.
    if (containingBlock.isRenderFragmentedFlow() && !isPerpendicularWritingMode)
        return true;

    // Render view is not considered auto height.
    if (is<RenderView>(containingBlock))
        return false;

    // If the writing mode of the containing block is orthogonal to ours, it means
    // that we shouldn't skip anything, since we're going to resolve the
    // percentage height against a containing block *width*.
    if (isPerpendicularWritingMode)
        return false;
    
    // Anonymous blocks should not impede percentage resolution on a child.
    // Examples of such anonymous blocks are blocks wrapped around inlines that
    // have block siblings (from the CSS spec) and multicol flow threads (an
    // implementation detail). Another implementation detail, ruby runs, create
    // anonymous inline-blocks, so skip those too. All other types of anonymous
    // objects, such as table-cells and flexboxes, will be treated as if they were
    // non-anonymous.
    if (containingBlock.isAnonymous())
        return containingBlock.style().display() == DisplayType::Block || containingBlock.style().display() == DisplayType::InlineBlock;
    
    // For quirks mode, we skip most auto-height containing blocks when computing
    // percentages.
    return document().inQuirksMode() && !containingBlock.isTableCell() && !containingBlock.isOutOfFlowPositioned() && !containingBlock.isRenderGrid() && !containingBlock.isFlexibleBoxIncludingDeprecated() && containingBlock.style().logicalHeight().isAuto();
}

bool RenderBox::shouldTreatChildAsReplacedInTableCells() const
{
    if (isReplacedOrInlineBlock())
        return true;
    return element() && (element()->isFormControlElement() || is<HTMLImageElement>(element()));
}

static bool tableCellShouldHaveZeroInitialSize(const RenderBlock& block, const RenderBox& child, bool scrollsOverflowY)
{
    // Normally we would let the cell size intrinsically, but scrolling overflow has to be
    // treated differently, since WinIE lets scrolled overflow fragments shrink as needed.
    // While we can't get all cases right, we can at least detect when the cell has a specified
    // height or when the table has a specified height. In these cases we want to initially have
    // no size and allow the flexing of the table or the cell to its specified height to cause us
    // to grow to fill the space. This could end up being wrong in some cases, but it is
    // preferable to the alternative (sizing intrinsically and making the row end up too big).
    const RenderTableCell& cell = downcast<RenderTableCell>(block);
    return scrollsOverflowY && !child.shouldTreatChildAsReplacedInTableCells() && (!cell.style().logicalHeight().isAuto() || !cell.table()->style().logicalHeight().isAuto());
}

std::optional<LayoutUnit> RenderBox::computePercentageLogicalHeight(const Length& height, UpdatePercentageHeightDescendants updateDescendants) const
{
    std::optional<LayoutUnit> availableHeight;

    bool skippedAutoHeightContainingBlock = false;
    RenderBlock* cb = containingBlock();
    const RenderBox* containingBlockChild = this;
    LayoutUnit rootMarginBorderPaddingHeight;
    bool isHorizontal = isHorizontalWritingMode();
    while (cb && !is<RenderView>(*cb) && skipContainingBlockForPercentHeightCalculation(*cb, isHorizontal != cb->isHorizontalWritingMode())) {
        if (cb->isBody() || cb->isDocumentElementRenderer())
            rootMarginBorderPaddingHeight += cb->marginBefore() + cb->marginAfter() + cb->borderAndPaddingLogicalHeight();
        skippedAutoHeightContainingBlock = true;
        containingBlockChild = cb;
        cb = cb->containingBlock();
    }
    if (updateDescendants == UpdatePercentageHeightDescendants::Yes)
        cb->addPercentHeightDescendant(const_cast<RenderBox&>(*this));

    bool isOrthogonal = isHorizontal != cb->isHorizontalWritingMode();
    if (hasOverridingContainingBlockContentLogicalWidth() && isOrthogonal)
        availableHeight = overridingContainingBlockContentLogicalWidth();
    else if (hasOverridingContainingBlockContentLogicalHeight() && !isOrthogonal)
        availableHeight = overridingContainingBlockContentLogicalHeight();
    else if (isOrthogonal)
        availableHeight = containingBlockChild->containingBlockLogicalWidthForContent();
    else if (is<RenderTableCell>(*cb)) {
        if (!skippedAutoHeightContainingBlock) {
            // Table cells violate what the CSS spec says to do with heights. Basically we
            // don't care if the cell specified a height or not. We just always make ourselves
            // be a percentage of the cell's current content height.
            if (!cb->hasOverridingLogicalHeight())
                return tableCellShouldHaveZeroInitialSize(*cb, *this, scrollsOverflowY()) ? std::optional<LayoutUnit>(0) : std::nullopt;

            availableHeight = cb->overridingLogicalHeight() - cb->computedCSSPaddingBefore() - cb->computedCSSPaddingAfter() - cb->borderBefore() - cb->borderAfter();
        }
    } else
        availableHeight = cb->availableLogicalHeightForPercentageComputation();
    
    if (!availableHeight)
        return availableHeight;

    LayoutUnit result = valueForLength(height, availableHeight.value() - rootMarginBorderPaddingHeight + (isTable() && isOutOfFlowPositioned() ? cb->paddingBefore() + cb->paddingAfter() : 0_lu));
    
    // |overridingLogicalHeight| is the maximum height made available by the
    // cell to its percent height children when we decide they can determine the
    // height of the cell. If the percent height child is box-sizing:content-box
    // then we must subtract the border and padding from the cell's
    // |availableHeight| (given by |overridingLogicalHeight|) to arrive
    // at the child's computed height.
    bool subtractBorderAndPadding = isTable() || (is<RenderTableCell>(*cb) && !skippedAutoHeightContainingBlock && cb->hasOverridingLogicalHeight() && style().boxSizing() == BoxSizing::ContentBox);
    if (subtractBorderAndPadding) {
        result -= borderAndPaddingLogicalHeight();
        return std::max(0_lu, result);
    }
    return result;
}

LayoutUnit RenderBox::computeReplacedLogicalWidth(ShouldComputePreferred shouldComputePreferred) const
{
    return computeReplacedLogicalWidthRespectingMinMaxWidth(computeReplacedLogicalWidthUsing(MainOrPreferredSize, style().logicalWidth()), shouldComputePreferred);
}

LayoutUnit RenderBox::computeReplacedLogicalWidthRespectingMinMaxWidth(LayoutUnit logicalWidth, ShouldComputePreferred shouldComputePreferred) const
{
    auto& logicalMinWidth = style().logicalMinWidth();
    auto& logicalMaxWidth = style().logicalMaxWidth();
    bool useLogicalWidthForMinWidth = (shouldComputePreferred == ComputePreferred && logicalMinWidth.isPercentOrCalculated()) || logicalMinWidth.isUndefined();
    bool useLogicalWidthForMaxWidth = (shouldComputePreferred == ComputePreferred && logicalMaxWidth.isPercentOrCalculated()) || logicalMaxWidth.isUndefined();
    auto minLogicalWidth =  useLogicalWidthForMinWidth ? logicalWidth : computeReplacedLogicalWidthUsing(MinSize, logicalMinWidth);
    auto maxLogicalWidth =  useLogicalWidthForMaxWidth ? logicalWidth : computeReplacedLogicalWidthUsing(MaxSize, logicalMaxWidth);
    return std::max(minLogicalWidth, std::min(logicalWidth, maxLogicalWidth));
}

LayoutUnit RenderBox::computeReplacedLogicalWidthUsing(SizeType widthType, Length logicalWidth) const
{
    ASSERT(widthType == MinSize || widthType == MainOrPreferredSize || !logicalWidth.isAuto());
    if (widthType == MinSize && logicalWidth.isAuto())
        return adjustContentBoxLogicalWidthForBoxSizing(0, logicalWidth.type());

    switch (logicalWidth.type()) {
    case LengthType::Fixed:
        return adjustContentBoxLogicalWidthForBoxSizing(logicalWidth);
    case LengthType::MinContent:
    case LengthType::MaxContent: {
        // MinContent/MaxContent don't need the availableLogicalWidth argument.
        LayoutUnit availableLogicalWidth;
        return computeIntrinsicLogicalWidthUsing(logicalWidth, availableLogicalWidth, borderAndPaddingLogicalWidth()) - borderAndPaddingLogicalWidth();
    }
    case LengthType::FitContent:
    case LengthType::FillAvailable:
    case LengthType::Percent:
    case LengthType::Calculated: {
        LayoutUnit containerWidth;
        if (isOutOfFlowPositioned())
            containerWidth = containingBlockLogicalWidthForPositioned(downcast<RenderBoxModelObject>(*container()));
        else if (isHorizontalWritingMode() == containingBlock()->isHorizontalWritingMode())
            containerWidth = containingBlockLogicalWidthForContent();
        else
            containerWidth = perpendicularContainingBlockLogicalHeight();
        Length containerLogicalWidth = containingBlock()->style().logicalWidth();
        // FIXME: Handle cases when containing block width is calculated or viewport percent.
        // https://bugs.webkit.org/show_bug.cgi?id=91071
        if (logicalWidth.isIntrinsic())
            return computeIntrinsicLogicalWidthUsing(logicalWidth, containerWidth, borderAndPaddingLogicalWidth()) - borderAndPaddingLogicalWidth();
        if (containerWidth > 0 || (!containerWidth && (containerLogicalWidth.isFixed() || containerLogicalWidth.isPercentOrCalculated())))
            return adjustContentBoxLogicalWidthForBoxSizing(minimumValueForLength(logicalWidth, containerWidth), logicalWidth.type());
        return 0_lu;
    }
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::Auto:
    case LengthType::Content:
    case LengthType::Relative:
    case LengthType::Undefined:
        return intrinsicLogicalWidth();
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutUnit RenderBox::computeReplacedLogicalHeight(std::optional<LayoutUnit>) const
{
    return computeReplacedLogicalHeightRespectingMinMaxHeight(computeReplacedLogicalHeightUsing(MainOrPreferredSize, style().logicalHeight()));
}

static bool allowMinMaxPercentagesInAutoHeightBlocksQuirk()
{
#if PLATFORM(COCOA)
    return CocoaApplication::isIBooks();
#else
    return false;
#endif
}

void RenderBox::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    computePreferredLogicalWidths(style().logicalMinWidth(), style().logicalMaxWidth(), borderAndPaddingLogicalWidth());
    setPreferredLogicalWidthsDirty(false);
}

void RenderBox::computePreferredLogicalWidths(const Length& minWidth, const Length& maxWidth, LayoutUnit borderAndPadding)
{
    if (shouldComputeLogicalHeightFromAspectRatio()) {
        auto [logicalMinWidth, logicalMaxWidth] = computeMinMaxLogicalWidthFromAspectRatio();
        m_minPreferredLogicalWidth = std::clamp(m_minPreferredLogicalWidth, logicalMinWidth, logicalMaxWidth);
        m_maxPreferredLogicalWidth = std::clamp(m_maxPreferredLogicalWidth, logicalMinWidth, logicalMaxWidth);
    }

    if (maxWidth.isFixed()) {
        auto adjustContentBoxLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(maxWidth);
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidth);
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidth);
    }

    if (minWidth.isFixed() && minWidth.value() > 0) {
        auto adjustContentBoxLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(minWidth);
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidth);
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidth);
    }

    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;
}

bool RenderBox::replacedMinMaxLogicalHeightComputesAsNone(SizeType sizeType) const
{
    ASSERT(sizeType == MinSize || sizeType == MaxSize);
    
    auto logicalHeight = sizeType == MinSize ? style().logicalMinHeight() : style().logicalMaxHeight();
    auto initialLogicalHeight = sizeType == MinSize ? RenderStyle::initialMinSize() : RenderStyle::initialMaxSize();
    
    if (logicalHeight == initialLogicalHeight)
        return true;
    
    if (logicalHeight.isPercentOrCalculated() && hasOverridingContainingBlockContentLogicalHeight())
        return overridingContainingBlockContentLogicalHeight() == std::nullopt;

    // Make sure % min-height and % max-height resolve to none if the containing block has auto height.
    // Note that the "height" case for replaced elements was handled by hasReplacedLogicalHeight, which is why
    // min and max-height are the only ones handled here.
    // FIXME: For now we put in a quirk for iBooks until we can move them to viewport units.
    if (auto* cb = containingBlockForAutoHeightDetection(logicalHeight))
        return allowMinMaxPercentagesInAutoHeightBlocksQuirk() ? false : cb->hasAutoHeightOrContainingBlockWithAutoHeight();

    return false;
}

LayoutUnit RenderBox::computeReplacedLogicalHeightRespectingMinMaxHeight(LayoutUnit logicalHeight) const
{
    LayoutUnit minLogicalHeight;
    if (!replacedMinMaxLogicalHeightComputesAsNone(MinSize))
        minLogicalHeight = computeReplacedLogicalHeightUsing(MinSize, style().logicalMinHeight());
    LayoutUnit maxLogicalHeight = logicalHeight;
    if (!replacedMinMaxLogicalHeightComputesAsNone(MaxSize))
        maxLogicalHeight = computeReplacedLogicalHeightUsing(MaxSize, style().logicalMaxHeight());
    return std::max(minLogicalHeight, std::min(logicalHeight, maxLogicalHeight));
}

LayoutUnit RenderBox::computeReplacedLogicalHeightUsing(SizeType heightType, Length logicalHeight) const
{
    ASSERT(heightType == MinSize || heightType == MainOrPreferredSize || !logicalHeight.isAuto());
    if (heightType == MinSize && logicalHeight.isAuto())
        return adjustContentBoxLogicalHeightForBoxSizing(std::optional<LayoutUnit>(0));

    switch (logicalHeight.type()) {
    case LengthType::Fixed:
        return adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit(logicalHeight.value()));
    case LengthType::Percent:
    case LengthType::Calculated: {
        auto* container = isOutOfFlowPositioned() ? this->container() : containingBlock();
        while (container && container->isAnonymous()) {
            // Stop at rendering context root.
            if (is<RenderView>(*container))
                break;
            container = container->containingBlock();
        }
        bool hasPerpendicularContainingBlock = container->isHorizontalWritingMode() != isHorizontalWritingMode();
        std::optional<LayoutUnit> stretchedHeight;
        if (is<RenderBlock>(container)) {
            auto* block = downcast<RenderBlock>(container);
            block->addPercentHeightDescendant(*const_cast<RenderBox*>(this));
            if (block->isFlexItem() && downcast<RenderFlexibleBox>(block->parent())->useChildOverridingLogicalHeightForPercentageResolution(*block))
                stretchedHeight = block->overridingContentLogicalHeight();
            else if (block->isGridItem() && block->hasOverridingLogicalHeight() && !hasPerpendicularContainingBlock)
                stretchedHeight = block->overridingContentLogicalHeight();
        }

        // FIXME: This calculation is not patched for block-flow yet.
        // https://bugs.webkit.org/show_bug.cgi?id=46500
        if (container->isOutOfFlowPositioned()
            && container->style().height().isAuto()
            && !(container->style().top().isAuto() || container->style().bottom().isAuto())) {
            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(container->isRenderBlock());
            auto& block = downcast<RenderBlock>(*container);
            auto computedValues = block.computeLogicalHeight(block.logicalHeight(), 0);
            LayoutUnit newContentHeight = computedValues.m_extent - block.borderAndPaddingLogicalHeight() - block.scrollbarLogicalHeight();
            return adjustContentBoxLogicalHeightForBoxSizing(valueForLength(logicalHeight, newContentHeight));
        }
        
        LayoutUnit availableHeight;
        if (isOutOfFlowPositioned())
            availableHeight = containingBlockLogicalHeightForPositioned(downcast<RenderBoxModelObject>(*container));
        else if (stretchedHeight)
            availableHeight = stretchedHeight.value();
        else {
            availableHeight = hasPerpendicularContainingBlock ? containingBlockLogicalWidthForContent() : containingBlockLogicalHeightForContent(IncludeMarginBorderPadding);
            // It is necessary to use the border-box to match WinIE's broken
            // box model. This is essential for sizing inside
            // table cells using percentage heights.
            // FIXME: This needs to be made block-flow-aware. If the cell and image are perpendicular block-flows, this isn't right.
            // https://bugs.webkit.org/show_bug.cgi?id=46997
            while (container && !is<RenderView>(*container)
                && (container->style().logicalHeight().isAuto() || container->style().logicalHeight().isPercentOrCalculated())) {
                if (container->isTableCell()) {
                    // Don't let table cells squeeze percent-height replaced elements
                    // <http://bugs.webkit.org/show_bug.cgi?id=15359>
                    availableHeight = std::max(availableHeight, intrinsicLogicalHeight());
                    return valueForLength(logicalHeight, availableHeight - borderAndPaddingLogicalHeight());
                }
                downcast<RenderBlock>(*container).addPercentHeightDescendant(const_cast<RenderBox&>(*this));
                container = container->containingBlock();
            }
        }
        return adjustContentBoxLogicalHeightForBoxSizing(valueForLength(logicalHeight, availableHeight));
    }
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FitContent:
    case LengthType::FillAvailable:
        return adjustContentBoxLogicalHeightForBoxSizing(computeIntrinsicLogicalContentHeightUsing(logicalHeight, intrinsicLogicalHeight(), borderAndPaddingLogicalHeight()));
    default:
        return intrinsicLogicalHeight();
    }
}

LayoutUnit RenderBox::availableLogicalHeight(AvailableLogicalHeightType heightType) const
{
    return constrainContentBoxLogicalHeightByMinMax(availableLogicalHeightUsing(style().logicalHeight(), heightType), std::nullopt);
}

LayoutUnit RenderBox::availableLogicalHeightUsing(const Length& h, AvailableLogicalHeightType heightType) const
{
    // We need to stop here, since we don't want to increase the height of the table
    // artificially.  We're going to rely on this cell getting expanded to some new
    // height, and then when we lay out again we'll use the calculation below.
    if (isTableCell() && (h.isAuto() || h.isPercentOrCalculated())) {
        if (hasOverridingLogicalHeight())
            return overridingLogicalHeight() - computedCSSPaddingBefore() - computedCSSPaddingAfter() - borderBefore() - borderAfter();
        return logicalHeight() - borderAndPaddingLogicalHeight();
    }

    if (isFlexItem() && downcast<RenderFlexibleBox>(*parent()).useChildOverridingLogicalHeightForPercentageResolution(*this))
        return overridingContentLogicalHeight();

    if (shouldComputeLogicalHeightFromAspectRatio())
        return blockSizeFromAspectRatio(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent(), style().logicalAspectRatio(), style().boxSizingForAspectRatio(), logicalWidth());

    if (h.isPercentOrCalculated() && isOutOfFlowPositioned() && !isRenderFragmentedFlow()) {
        // FIXME: This is wrong if the containingBlock has a perpendicular writing mode.
        LayoutUnit availableHeight = containingBlockLogicalHeightForPositioned(*containingBlock());
        return adjustContentBoxLogicalHeightForBoxSizing(valueForLength(h, availableHeight));
    }

    if (std::optional<LayoutUnit> heightIncludingScrollbar = computeContentAndScrollbarLogicalHeightUsing(MainOrPreferredSize, h, std::nullopt))
        return std::max<LayoutUnit>(0, adjustContentBoxLogicalHeightForBoxSizing(heightIncludingScrollbar) - scrollbarLogicalHeight());

    // FIXME: Check logicalTop/logicalBottom here to correctly handle vertical writing-mode.
    // https://bugs.webkit.org/show_bug.cgi?id=46500
    if (is<RenderBlock>(*this) && isOutOfFlowPositioned() && style().height().isAuto() && !(style().top().isAuto() || style().bottom().isAuto())) {
        RenderBlock& block = const_cast<RenderBlock&>(downcast<RenderBlock>(*this));
        auto computedValues = block.computeLogicalHeight(block.logicalHeight(), 0);
        return computedValues.m_extent - block.borderAndPaddingLogicalHeight() - block.scrollbarLogicalHeight();
    }

    // FIXME: This is wrong if the containingBlock has a perpendicular writing mode.
    LayoutUnit availableHeight = containingBlockLogicalHeightForContent(heightType);
    if (heightType == ExcludeMarginBorderPadding) {
        // FIXME: Margin collapsing hasn't happened yet, so this incorrectly removes collapsed margins.
        availableHeight -= marginBefore() + marginAfter() + borderAndPaddingLogicalHeight();
    }
    return availableHeight;
}

void RenderBox::computeBlockDirectionMargins(const RenderBlock& containingBlock, LayoutUnit& marginBefore, LayoutUnit& marginAfter) const
{
    if (isTableCell()) {
        // FIXME: Not right if we allow cells to have different directionality than the table.  If we do allow this, though,
        // we may just do it with an extra anonymous block inside the cell.
        marginBefore = 0;
        marginAfter = 0;
        return;
    }

    // Margins are calculated with respect to the logical width of
    // the containing block (8.3)
    LayoutUnit cw = containingBlockLogicalWidthForContent();
    const RenderStyle& containingBlockStyle = containingBlock.style();
    marginBefore = minimumValueForLength(style().marginBeforeUsing(&containingBlockStyle), cw);
    marginAfter = minimumValueForLength(style().marginAfterUsing(&containingBlockStyle), cw);
}

void RenderBox::computeAndSetBlockDirectionMargins(const RenderBlock& containingBlock)
{
    LayoutUnit marginBefore;
    LayoutUnit marginAfter;
    computeBlockDirectionMargins(containingBlock, marginBefore, marginAfter);
    containingBlock.setMarginBeforeForChild(*this, marginBefore);
    containingBlock.setMarginAfterForChild(*this, marginAfter);
}

LayoutUnit RenderBox::containingBlockLogicalWidthForPositioned(const RenderBoxModelObject& containingBlock, RenderFragmentContainer* fragment, bool checkForPerpendicularWritingMode) const
{
    if (checkForPerpendicularWritingMode && containingBlock.isHorizontalWritingMode() != isHorizontalWritingMode())
        return containingBlockLogicalHeightForPositioned(containingBlock, false);

    if (hasOverridingContainingBlockContentLogicalWidth()) {
        if (auto width = overridingContainingBlockContentLogicalWidth())
            return width.value();
    }

    if (is<RenderBox>(containingBlock)) {
        bool isFixedPosition = isFixedPositioned();

        RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
        if (!fragmentedFlow) {
            if (isFixedPosition && is<RenderView>(containingBlock))
                return downcast<RenderView>(containingBlock).clientLogicalWidthForFixedPosition();

            return downcast<RenderBox>(containingBlock).clientLogicalWidth();
        }

        if (!is<RenderBlock>(containingBlock))
            return downcast<RenderBox>(containingBlock).clientLogicalWidth();

        const RenderBlock& cb = downcast<RenderBlock>(containingBlock);
        RenderBoxFragmentInfo* boxInfo = nullptr;
        if (!fragment) {
            if (is<RenderFragmentedFlow>(containingBlock) && !checkForPerpendicularWritingMode)
                return downcast<RenderFragmentedFlow>(containingBlock).contentLogicalWidthOfFirstFragment();
            if (isWritingModeRoot()) {
                LayoutUnit cbPageOffset = cb.offsetFromLogicalTopOfFirstPage();
                RenderFragmentContainer* cbFragment = cb.fragmentAtBlockOffset(cbPageOffset);
                if (cbFragment)
                    boxInfo = cb.renderBoxFragmentInfo(cbFragment);
            }
        } else if (fragmentedFlow->isHorizontalWritingMode() == containingBlock.isHorizontalWritingMode()) {
            RenderFragmentContainer* containingBlockFragment = cb.clampToStartAndEndFragments(fragment);
            boxInfo = cb.renderBoxFragmentInfo(containingBlockFragment);
        }
        return (boxInfo) ? std::max<LayoutUnit>(0, cb.clientLogicalWidth() - (cb.logicalWidth() - boxInfo->logicalWidth())) : cb.clientLogicalWidth();
    }

    ASSERT(containingBlock.isInFlowPositioned());

    const auto& flow = downcast<RenderInline>(containingBlock);
    LegacyInlineFlowBox* first = flow.firstLineBox();
    LegacyInlineFlowBox* last = flow.lastLineBox();

    // If the containing block is empty, return a width of 0.
    if (!first || !last)
        return 0;

    LayoutUnit fromLeft;
    LayoutUnit fromRight;
    if (containingBlock.style().isLeftToRightDirection()) {
        fromLeft = first->logicalLeft() + first->borderLogicalLeft();
        fromRight = last->logicalLeft() + last->logicalWidth() - last->borderLogicalRight();
    } else {
        fromRight = first->logicalLeft() + first->logicalWidth() - first->borderLogicalRight();
        fromLeft = last->logicalLeft() + last->borderLogicalLeft();
    }

    return std::max<LayoutUnit>(0, fromRight - fromLeft);
}

LayoutUnit RenderBox::containingBlockLogicalHeightForPositioned(const RenderBoxModelObject& containingBlock, bool checkForPerpendicularWritingMode) const
{
    if (checkForPerpendicularWritingMode && containingBlock.isHorizontalWritingMode() != isHorizontalWritingMode())
        return containingBlockLogicalWidthForPositioned(containingBlock, nullptr, false);

    if (hasOverridingContainingBlockContentLogicalHeight()) {
        if (auto height = overridingContainingBlockContentLogicalHeight())
            return height.value();
    }

    if (containingBlock.isBox()) {
        bool isFixedPosition = isFixedPositioned();

        if (isFixedPosition && is<RenderView>(containingBlock))
            return downcast<RenderView>(containingBlock).clientLogicalHeightForFixedPosition();

        const RenderBlock& cb = is<RenderBlock>(containingBlock) ? downcast<RenderBlock>(containingBlock) : *containingBlock.containingBlock();
        LayoutUnit result = cb.clientLogicalHeight();
        RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
        if (fragmentedFlow && is<RenderFragmentedFlow>(containingBlock) && fragmentedFlow->isHorizontalWritingMode() == containingBlock.isHorizontalWritingMode())
            return downcast<RenderFragmentedFlow>(containingBlock).contentLogicalHeightOfFirstFragment();
        return result;
    }
        
    ASSERT(containingBlock.isInFlowPositioned());

    const auto& flow = downcast<RenderInline>(containingBlock);
    LegacyInlineFlowBox* first = flow.firstLineBox();
    LegacyInlineFlowBox* last = flow.lastLineBox();

    // If the containing block is empty, return a height of 0.
    if (!first || !last)
        return 0;

    LayoutUnit heightResult;
    LayoutRect boundingBox = flow.linesBoundingBox();
    if (containingBlock.isHorizontalWritingMode())
        heightResult = boundingBox.height();
    else
        heightResult = boundingBox.width();
    heightResult -= (containingBlock.borderBefore() + containingBlock.borderAfter());
    return heightResult;
}

// FIXME: evaluate whether this should be a method of RenderObject instead.
static inline bool isOrthogonal(const RenderObject& renderer, const RenderObject& ancestor)
{
    return renderer.isHorizontalWritingMode() != ancestor.isHorizontalWritingMode();
}

static void computeInlineStaticDistance(Length& logicalLeft, Length& logicalRight, const RenderBox* child, const RenderBoxModelObject& containerBlock, LayoutUnit containerLogicalWidth, RenderFragmentContainer* fragment)
{
    if (!logicalLeft.isAuto() || !logicalRight.isAuto())
        return;

    RenderObject* parent = child->parent();
    TextDirection parentDirection = parent->style().direction();

    // This method is using enclosingBox() which is wrong for absolutely
    // positioned grid items, as they rely on the grid area. So for grid items if
    // both "left" and "right" properties are "auto", we can consider that one of
    // them (depending on the direction) is simply "0".
    if (parent->isRenderGrid() && parent == child->containingBlock()) {
        if (parentDirection == TextDirection::LTR)
            logicalLeft.setValue(LengthType::Fixed, 0);
        else
            logicalRight.setValue(LengthType::Fixed, 0);
        return;
    }

    // For orthogonal flows we don't care whether the parent is LTR or RTL because it does not affect the position in our inline axis.
    if (parentDirection == TextDirection::LTR || isOrthogonal(*child, *parent)) {
        LayoutUnit staticPosition = isOrthogonal(*child, *parent) ? child->layer()->staticBlockPosition() - containerBlock.borderBefore() : child->layer()->staticInlinePosition() - containerBlock.borderLogicalLeft();
        for (auto* current = parent; current && current != &containerBlock; current = current->container()) {
            if (!is<RenderBox>(*current))
                continue;
            const auto& renderBox = downcast<RenderBox>(*current);
            staticPosition += isOrthogonal(*child, *parent) ? renderBox.logicalTop() : renderBox.logicalLeft();
            if (renderBox.isInFlowPositioned())
                staticPosition += renderBox.isHorizontalWritingMode() ? renderBox.offsetForInFlowPosition().width() : renderBox.offsetForInFlowPosition().height();
            if (fragment && is<RenderBlock>(*current)) {
                const RenderBlock& currentBlock = downcast<RenderBlock>(*current);
                fragment = currentBlock.clampToStartAndEndFragments(fragment);
                RenderBoxFragmentInfo* boxInfo = currentBlock.renderBoxFragmentInfo(fragment);
                if (boxInfo)
                    staticPosition += boxInfo->logicalLeft();
            }
        }
        logicalLeft.setValue(LengthType::Fixed, staticPosition);
    } else {
        ASSERT(!isOrthogonal(*child, *parent));
        LayoutUnit staticPosition = child->layer()->staticInlinePosition() + containerLogicalWidth + containerBlock.borderLogicalLeft();
        auto& enclosingBox = parent->enclosingBox();
        if (&enclosingBox != &containerBlock && containerBlock.isDescendantOf(&enclosingBox)) {
            logicalRight.setValue(LengthType::Fixed, staticPosition);
            return;
        }
        staticPosition -= enclosingBox.logicalWidth();
        for (const RenderElement* current = &enclosingBox; current; current = current->container()) {
            if (!is<RenderBox>(*current))
                continue;

            if (current != &containerBlock) {
                auto& renderBox = downcast<RenderBox>(*current);
                staticPosition -= renderBox.logicalLeft();
                if (renderBox.isInFlowPositioned())
                    staticPosition -= renderBox.isHorizontalWritingMode() ? renderBox.offsetForInFlowPosition().width() : renderBox.offsetForInFlowPosition().height();
            }
            if (fragment && is<RenderBlock>(*current)) {
                auto& currentBlock = downcast<RenderBlock>(*current);
                fragment = currentBlock.clampToStartAndEndFragments(fragment);
                RenderBoxFragmentInfo* boxInfo = currentBlock.renderBoxFragmentInfo(fragment);
                if (boxInfo) {
                    if (current != &containerBlock)
                        staticPosition -= currentBlock.logicalWidth() - (boxInfo->logicalLeft() + boxInfo->logicalWidth());
                    if (current == &enclosingBox)
                        staticPosition += enclosingBox.logicalWidth() - boxInfo->logicalWidth();
                }
            }
            if (current == &containerBlock)
                break;
        }
        logicalRight.setValue(LengthType::Fixed, staticPosition);
    }
}

void RenderBox::computePositionedLogicalWidth(LogicalExtentComputedValues& computedValues, RenderFragmentContainer* fragment) const
{
    if (isReplacedOrInlineBlock()) {
        // FIXME: Positioned replaced elements inside a flow thread are not working properly
        // with variable width fragments (see https://bugs.webkit.org/show_bug.cgi?id=69896 ).
        computePositionedLogicalWidthReplaced(computedValues);
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
    const RenderBoxModelObject& containerBlock = downcast<RenderBoxModelObject>(*container());
    
    const LayoutUnit containerLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock, fragment);

    // Use the container block's direction except when calculating the static distance
    // This conforms with the reference results for abspos-replaced-width-margin-000.htm
    // of the CSS 2.1 test suite
    TextDirection containerDirection = containerBlock.style().direction();

    bool isHorizontal = isHorizontalWritingMode();
    const LayoutUnit bordersPlusPadding = borderAndPaddingLogicalWidth();
    const Length marginLogicalLeft = isHorizontal ? style().marginLeft() : style().marginTop();
    const Length marginLogicalRight = isHorizontal ? style().marginRight() : style().marginBottom();

    Length logicalLeftLength = style().logicalLeft();
    Length logicalRightLength = style().logicalRight();

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
    computeInlineStaticDistance(logicalLeftLength, logicalRightLength, this, containerBlock, containerLogicalWidth, fragment);
    
    // Calculate constraint equation values for 'width' case.
    computePositionedLogicalWidthUsing(MainOrPreferredSize, style().logicalWidth(), containerBlock, containerDirection,
                                       containerLogicalWidth, bordersPlusPadding,
                                       logicalLeftLength, logicalRightLength, marginLogicalLeft, marginLogicalRight,
                                       computedValues);

    LayoutUnit transferredMinSize = LayoutUnit::min();
    LayoutUnit transferredMaxSize = LayoutUnit::max();
    if (shouldComputeLogicalHeightFromAspectRatio())
        std::tie(transferredMinSize, transferredMaxSize) = computeMinMaxLogicalWidthFromAspectRatio();

    LogicalExtentComputedValues maxValues;
    maxValues.m_extent = LayoutUnit::max();
    // Calculate constraint equation values for 'max-width' case.
    if (!style().logicalMaxWidth().isUndefined()) {
        computePositionedLogicalWidthUsing(MaxSize, style().logicalMaxWidth(), containerBlock, containerDirection,
                                           containerLogicalWidth, bordersPlusPadding,
                                           logicalLeftLength, logicalRightLength, marginLogicalLeft, marginLogicalRight,
                                           maxValues);
    }
    if (transferredMaxSize < maxValues.m_extent) {
        computePositionedLogicalWidthUsing(MaxSize, Length(transferredMaxSize, LengthType::Fixed), containerBlock, containerDirection,
            containerLogicalWidth, bordersPlusPadding,
            logicalLeftLength, logicalRightLength, marginLogicalLeft, marginLogicalRight,
            maxValues);
    }
    if (computedValues.m_extent > maxValues.m_extent) {
        computedValues.m_extent = maxValues.m_extent;
        computedValues.m_position = maxValues.m_position;
        computedValues.m_margins.m_start = maxValues.m_margins.m_start;
        computedValues.m_margins.m_end = maxValues.m_margins.m_end;
    }

    LogicalExtentComputedValues minValues;
    minValues.m_extent = LayoutUnit::min();
    // Calculate constraint equation values for 'min-width' case.
    if (!style().logicalMinWidth().isZero() || style().logicalMinWidth().isIntrinsic()) {
        computePositionedLogicalWidthUsing(MinSize, style().logicalMinWidth(), containerBlock, containerDirection,
            containerLogicalWidth, bordersPlusPadding,
            logicalLeftLength, logicalRightLength, marginLogicalLeft, marginLogicalRight,
            minValues);
    }
    if (transferredMinSize > minValues.m_extent) {
        computePositionedLogicalWidthUsing(MinSize, Length(transferredMinSize, LengthType::Fixed), containerBlock, containerDirection,
                                           containerLogicalWidth, bordersPlusPadding,
                                           logicalLeftLength, logicalRightLength, marginLogicalLeft, marginLogicalRight,
                                           minValues);
    }
    if (computedValues.m_extent < minValues.m_extent) {
        computedValues.m_extent = minValues.m_extent;
        computedValues.m_position = minValues.m_position;
        computedValues.m_margins.m_start = minValues.m_margins.m_start;
        computedValues.m_margins.m_end = minValues.m_margins.m_end;
    }

    computedValues.m_extent += bordersPlusPadding;
    if (is<RenderBox>(containerBlock)) {
        auto& containingBox = downcast<RenderBox>(containerBlock);
        if (containingBox.shouldPlaceVerticalScrollbarOnLeft() && isHorizontalWritingMode())
            computedValues.m_position += containingBox.verticalScrollbarWidth();
    }
    
    // Adjust logicalLeft if we need to for the flipped version of our writing mode in fragments.
    // FIXME: Add support for other types of objects as containerBlock, not only RenderBlock.
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (fragmentedFlow && !fragment && isWritingModeRoot() && isHorizontalWritingMode() == containerBlock.isHorizontalWritingMode() && is<RenderBlock>(containerBlock)) {
        ASSERT(containerBlock.canHaveBoxInfoInFragment());
        LayoutUnit logicalLeftPos = computedValues.m_position;
        const RenderBlock& renderBlock = downcast<RenderBlock>(containerBlock);
        LayoutUnit cbPageOffset = renderBlock.offsetFromLogicalTopOfFirstPage();
        RenderFragmentContainer* cbFragment = renderBlock.fragmentAtBlockOffset(cbPageOffset);
        if (cbFragment) {
            RenderBoxFragmentInfo* boxInfo = renderBlock.renderBoxFragmentInfo(cbFragment);
            if (boxInfo) {
                logicalLeftPos += boxInfo->logicalLeft();
                computedValues.m_position = logicalLeftPos;
            }
        }
    }
}

static void computeLogicalLeftPositionedOffset(LayoutUnit& logicalLeftPos, const RenderBox* child, LayoutUnit logicalWidthValue, const RenderBoxModelObject& containerBlock, LayoutUnit containerLogicalWidth, bool logicalLeftIsAuto, bool logicalRightIsAuto)
{
    auto logicalLeftAndRightAreAuto = logicalLeftIsAuto && logicalRightIsAuto;
    bool isOverconstrained = !logicalLeftIsAuto && !logicalRightIsAuto && !child->style().logicalWidth().isAuto();
    // Deal with differing writing modes here. Our offset needs to be in the containing block's coordinate space. If the containing block is flipped
    // along this axis, then we need to flip the coordinate. Auto positioned items do not need this correction as it was properly handled in
    // computeInlineStaticDistance().
    if (isOrthogonal(*child, containerBlock) && !logicalLeftAndRightAreAuto && !isOverconstrained && containerBlock.style().isFlippedBlocksWritingMode()) {
        logicalLeftPos = containerLogicalWidth - logicalWidthValue - logicalLeftPos;
        logicalLeftPos += (child->isHorizontalWritingMode() ? containerBlock.borderRight() : containerBlock.borderBottom());
    } else
        logicalLeftPos += (child->isHorizontalWritingMode() ? containerBlock.borderLeft() : containerBlock.borderTop());
}

void RenderBox::computePositionedLogicalWidthUsing(SizeType widthType, Length logicalWidth, const RenderBoxModelObject& containerBlock, TextDirection containerDirection,
                                                   LayoutUnit containerLogicalWidth, LayoutUnit bordersPlusPadding,
                                                   Length logicalLeft, Length logicalRight, Length marginLogicalLeft, Length marginLogicalRight,
                                                   LogicalExtentComputedValues& computedValues) const
{
    ASSERT(widthType == MinSize || widthType == MainOrPreferredSize || !logicalWidth.isAuto());
    auto originalLogicalWidthType = logicalWidth.type();
    if (widthType == MinSize && logicalWidth.isAuto()) {
        if (shouldComputeLogicalWidthFromAspectRatio()) {
            LayoutUnit minLogicalWidth;
            LayoutUnit maxLogicalWidth;
            computeIntrinsicLogicalWidths(minLogicalWidth, maxLogicalWidth);
            logicalWidth = Length(minLogicalWidth, LengthType::Fixed);
        } else
            logicalWidth = Length(0, LengthType::Fixed);
    } else if (widthType == MainOrPreferredSize && logicalWidth.isAuto() && shouldComputeLogicalWidthFromAspectRatio())
        logicalWidth = Length(computeLogicalWidthFromAspectRatio(), LengthType::Fixed);
    else if (logicalWidth.isIntrinsic())
        logicalWidth = Length(computeIntrinsicLogicalWidthUsing(logicalWidth, containerLogicalWidth, bordersPlusPadding) - bordersPlusPadding, LengthType::Fixed);

    // 'left' and 'right' cannot both be 'auto' because one would of been
    // converted to the static position already
    ASSERT(!(logicalLeft.isAuto() && logicalRight.isAuto()));

    LayoutUnit logicalLeftValue;

    const LayoutUnit containerRelativeLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock, nullptr, false);

    bool logicalWidthIsAuto = logicalWidth.isIntrinsicOrAuto() && !shouldComputeLogicalWidthFromAspectRatio();
    bool logicalLeftIsAuto = logicalLeft.isAuto();
    bool logicalRightIsAuto = logicalRight.isAuto();
    LayoutUnit& marginLogicalLeftValue = style().isLeftToRightDirection() ? computedValues.m_margins.m_start : computedValues.m_margins.m_end;
    LayoutUnit& marginLogicalRightValue = style().isLeftToRightDirection() ? computedValues.m_margins.m_end : computedValues.m_margins.m_start;

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

        logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);
        computedValues.m_extent = adjustContentBoxLogicalWidthForBoxSizing(valueForLength(logicalWidth, containerLogicalWidth), originalLogicalWidthType);

        const LayoutUnit availableSpace = containerLogicalWidth - (logicalLeftValue + computedValues.m_extent + valueForLength(logicalRight, containerLogicalWidth) + bordersPlusPadding);

        // Margins are now the only unknown
        if (marginLogicalLeft.isAuto() && marginLogicalRight.isAuto()) {
            // Both margins auto, solve for equality
            if (availableSpace >= 0) {
                marginLogicalLeftValue = availableSpace / 2; // split the difference
                marginLogicalRightValue = availableSpace - marginLogicalLeftValue; // account for odd valued differences
            } else {
                // Use the containing block's direction rather than the parent block's
                // per CSS 2.1 reference test abspos-non-replaced-width-margin-000.
                if (containerDirection == TextDirection::LTR) {
                    marginLogicalLeftValue = 0;
                    marginLogicalRightValue = availableSpace; // will be negative
                } else {
                    marginLogicalLeftValue = availableSpace; // will be negative
                    marginLogicalRightValue = 0;
                }
            }
        } else if (marginLogicalLeft.isAuto()) {
            // Solve for left margin
            marginLogicalRightValue = valueForLength(marginLogicalRight, containerRelativeLogicalWidth);
            marginLogicalLeftValue = availableSpace - marginLogicalRightValue;
        } else if (marginLogicalRight.isAuto()) {
            // Solve for right margin
            marginLogicalLeftValue = valueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
            marginLogicalRightValue = availableSpace - marginLogicalLeftValue;
        } else {
            // Over-constrained, solve for left if direction is RTL
            marginLogicalLeftValue = valueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
            marginLogicalRightValue = valueForLength(marginLogicalRight, containerRelativeLogicalWidth);

            // Use the containing block's direction rather than the parent block's
            // per CSS 2.1 reference test abspos-non-replaced-width-margin-000.
            if (!isOrthogonal(*this, containerBlock) && containerDirection == TextDirection::RTL)
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
        marginLogicalLeftValue = minimumValueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
        marginLogicalRightValue = minimumValueForLength(marginLogicalRight, containerRelativeLogicalWidth);

        const LayoutUnit availableSpace = containerLogicalWidth - (marginLogicalLeftValue + marginLogicalRightValue + bordersPlusPadding);

        // FIXME: Is there a faster way to find the correct case?
        // Use rule/case that applies.
        if (logicalLeftIsAuto && logicalWidthIsAuto && !logicalRightIsAuto) {
            // RULE 1: (use shrink-to-fit for width, and solve of left)
            LayoutUnit logicalRightValue = valueForLength(logicalRight, containerLogicalWidth);

            // FIXME: would it be better to have shrink-to-fit in one step?
            LayoutUnit preferredWidth = maxPreferredLogicalWidth() - bordersPlusPadding;
            LayoutUnit preferredMinWidth = minPreferredLogicalWidth() - bordersPlusPadding;
            LayoutUnit availableWidth = availableSpace - logicalRightValue;
            computedValues.m_extent = std::min(std::max(preferredMinWidth, availableWidth), preferredWidth);
            logicalLeftValue = availableSpace - (computedValues.m_extent + logicalRightValue);
        } else if (!logicalLeftIsAuto && logicalWidthIsAuto && logicalRightIsAuto) {
            // RULE 3: (use shrink-to-fit for width, and no need solve of right)
            logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);

            // FIXME: would it be better to have shrink-to-fit in one step?
            LayoutUnit preferredWidth = maxPreferredLogicalWidth() - bordersPlusPadding;
            LayoutUnit preferredMinWidth = minPreferredLogicalWidth() - bordersPlusPadding;
            LayoutUnit availableWidth = availableSpace - logicalLeftValue;
            computedValues.m_extent = std::min(std::max(preferredMinWidth, availableWidth), preferredWidth);
        } else if (logicalLeftIsAuto && !logicalWidthIsAuto && !logicalRightIsAuto) {
            // RULE 4: (solve for left)
            computedValues.m_extent = adjustContentBoxLogicalWidthForBoxSizing(valueForLength(logicalWidth, containerLogicalWidth), originalLogicalWidthType);
            logicalLeftValue = availableSpace - (computedValues.m_extent + valueForLength(logicalRight, containerLogicalWidth));
        } else if (!logicalLeftIsAuto && logicalWidthIsAuto && !logicalRightIsAuto) {
            // RULE 5: (solve for width)
            logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);
            computedValues.m_extent = availableSpace - (logicalLeftValue + valueForLength(logicalRight, containerLogicalWidth));
        } else if (!logicalLeftIsAuto && !logicalWidthIsAuto && logicalRightIsAuto) {
            // RULE 6: (no need solve for right)
            logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);
            computedValues.m_extent = adjustContentBoxLogicalWidthForBoxSizing(valueForLength(logicalWidth, containerLogicalWidth), originalLogicalWidthType);
        }
    }

    // Use computed values to calculate the horizontal position.

    // FIXME: This hack is needed to calculate the  logical left position for a 'rtl' relatively
    // positioned, inline because right now, it is using the logical left position
    // of the first line box when really it should use the last line box.  When
    // this is fixed elsewhere, this block should be removed.
    if (is<RenderInline>(containerBlock) && !containerBlock.style().isLeftToRightDirection()) {
        const auto& flow = downcast<RenderInline>(containerBlock);
        LegacyInlineFlowBox* firstLine = flow.firstLineBox();
        LegacyInlineFlowBox* lastLine = flow.lastLineBox();
        if (firstLine && lastLine && firstLine != lastLine) {
            computedValues.m_position = logicalLeftValue + marginLogicalLeftValue + lastLine->borderLogicalLeft() + (lastLine->logicalLeft() - firstLine->logicalLeft());
            return;
        }
    }

    computedValues.m_position = logicalLeftValue + marginLogicalLeftValue;
    computeLogicalLeftPositionedOffset(computedValues.m_position, this, computedValues.m_extent + bordersPlusPadding, containerBlock, containerLogicalWidth, style().logicalLeft().isAuto(), style().logicalRight().isAuto());
}

static void computeBlockStaticDistance(Length& logicalTop, Length& logicalBottom, const RenderBox* child, const RenderBoxModelObject& containerBlock)
{
    if (!logicalTop.isAuto() || !logicalBottom.isAuto())
        return;
    
    RenderObject* parent = child->parent();

    // The static positions from the child's layer are relative to the container block's coordinate space (which is determined
    // by the writing mode and text direction), meaning that for orthogonal flows the logical top of the child (which depends on
    // the child's writing mode) is retrieved from the static inline position instead of the static block position.
    LayoutUnit staticLogicalTop = isOrthogonal(*child, *parent) ? child->layer()->staticInlinePosition() - containerBlock.borderLogicalLeft() : child->layer()->staticBlockPosition() - containerBlock.borderBefore();
    for (RenderElement* container = child->parent(); container && container != &containerBlock; container = container->container()) {
        if (!is<RenderBox>(*container))
            continue;
        const auto& renderBox = downcast<RenderBox>(*container);
        if (!is<RenderTableRow>(renderBox))
            staticLogicalTop += isOrthogonal(*child, *parent) ? renderBox.logicalLeft() : renderBox.logicalTop();
        if (renderBox.isInFlowPositioned())
            staticLogicalTop += renderBox.isHorizontalWritingMode() ? renderBox.offsetForInFlowPosition().height() : renderBox.offsetForInFlowPosition().width();
    }

    // If the parent is RTL then we need to flip the coordinate by setting the logical bottom instead of the logical top. That only needs
    // to be done in case of orthogonal writing modes, for horizontal ones the text direction of the parent does not affect the block position.
    if (parent->style().direction() != TextDirection::LTR && isOrthogonal(*child, *parent))
        logicalBottom.setValue(LengthType::Fixed, staticLogicalTop);
    else
        logicalTop.setValue(LengthType::Fixed, staticLogicalTop);
}

void RenderBox::computePositionedLogicalHeight(LogicalExtentComputedValues& computedValues) const
{
    if (isReplacedOrInlineBlock()) {
        computePositionedLogicalHeightReplaced(computedValues);
        return;
    }

    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.6.4 "Absolutely positioned, non-replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-non-replaced-height>
    // (block-style-comments in this function and in computePositionedLogicalHeightUsing()
    // correspond to text from the spec)


    // We don't use containingBlock(), since we may be positioned by an enclosing relpositioned inline.
    const RenderBoxModelObject& containerBlock = downcast<RenderBoxModelObject>(*container());

    const LayoutUnit containerLogicalHeight = containingBlockLogicalHeightForPositioned(containerBlock);

    const RenderStyle& styleToUse = style();
    const LayoutUnit bordersPlusPadding = borderAndPaddingLogicalHeight();
    const Length marginBefore = styleToUse.marginBefore();
    const Length marginAfter = styleToUse.marginAfter();
    Length logicalTopLength = styleToUse.logicalTop();
    Length logicalBottomLength = styleToUse.logicalBottom();

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

    // Calculate constraint equation values for 'height' case.
    LayoutUnit logicalHeight = computedValues.m_extent;
    computePositionedLogicalHeightUsing(MainOrPreferredSize, styleToUse.logicalHeight(), containerBlock, containerLogicalHeight, bordersPlusPadding, logicalHeight,
                                        logicalTopLength, logicalBottomLength, marginBefore, marginAfter,
                                        computedValues);

    // Avoid doing any work in the common case (where the values of min-height and max-height are their defaults).
    // see FIXME 2

    // Calculate constraint equation values for 'max-height' case.
    if (!styleToUse.logicalMaxHeight().isUndefined()) {
        LogicalExtentComputedValues maxValues;

        computePositionedLogicalHeightUsing(MaxSize, styleToUse.logicalMaxHeight(), containerBlock, containerLogicalHeight, bordersPlusPadding, logicalHeight,
                                            logicalTopLength, logicalBottomLength, marginBefore, marginAfter,
                                            maxValues);

        if (computedValues.m_extent > maxValues.m_extent) {
            computedValues.m_extent = maxValues.m_extent;
            computedValues.m_position = maxValues.m_position;
            computedValues.m_margins.m_before = maxValues.m_margins.m_before;
            computedValues.m_margins.m_after = maxValues.m_margins.m_after;
        }
    }

    // Calculate constraint equation values for 'min-height' case.
    Length logicalMinHeight = styleToUse.logicalMinHeight();
    if (logicalMinHeight.isAuto() || !logicalMinHeight.isZero() || logicalMinHeight.isIntrinsic()) {
        LogicalExtentComputedValues minValues;

        computePositionedLogicalHeightUsing(MinSize, styleToUse.logicalMinHeight(), containerBlock, containerLogicalHeight, bordersPlusPadding, logicalHeight,
                                            logicalTopLength, logicalBottomLength, marginBefore, marginAfter,
                                            minValues);

        if (computedValues.m_extent < minValues.m_extent) {
            computedValues.m_extent = minValues.m_extent;
            computedValues.m_position = minValues.m_position;
            computedValues.m_margins.m_before = minValues.m_margins.m_before;
            computedValues.m_margins.m_after = minValues.m_margins.m_after;
        }
    }

    // Set final height value.
    computedValues.m_extent += bordersPlusPadding;
    
    // Adjust logicalTop if we need to for perpendicular writing modes in fragments.
    // FIXME: Add support for other types of objects as containerBlock, not only RenderBlock.
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (fragmentedFlow && isHorizontalWritingMode() != containerBlock.isHorizontalWritingMode() && is<RenderBlock>(containerBlock)) {
        ASSERT(containerBlock.canHaveBoxInfoInFragment());
        LayoutUnit logicalTopPos = computedValues.m_position;
        const RenderBlock& renderBox = downcast<RenderBlock>(containerBlock);
        LayoutUnit cbPageOffset = renderBox.offsetFromLogicalTopOfFirstPage() - logicalLeft();
        RenderFragmentContainer* cbFragment = renderBox.fragmentAtBlockOffset(cbPageOffset);
        if (cbFragment) {
            RenderBoxFragmentInfo* boxInfo = renderBox.renderBoxFragmentInfo(cbFragment);
            if (boxInfo) {
                logicalTopPos += boxInfo->logicalLeft();
                computedValues.m_position = logicalTopPos;
            }
        }
    }
}

// The |containerLogicalHeightForPositioned| is already aware of orthogonal flows.
// The logicalTop concept is confusing here. It's the logical top from the child's POV. This means that is the physical
// y if the child is vertical or the physical x if the child is horizontal.
static void computeLogicalTopPositionedOffset(LayoutUnit& logicalTopPos, const RenderBox* child, LayoutUnit logicalHeightValue, const RenderBoxModelObject& containerBlock, LayoutUnit containerLogicalHeightForPositioned)
{
    auto logicalTopAndBottomAreAuto = child->style().logicalTop().isAuto() && child->style().logicalBottom().isAuto();
    auto haveOrthogonalWritingModes = isOrthogonal(*child, containerBlock);
    auto haveFlippedBlockAxis = child->style().isFlippedBlocksWritingMode() != containerBlock.style().isFlippedBlocksWritingMode();

    // Deal with differing writing modes here.  Our offset needs to be in the containing block's coordinate space. If the containing block is flipped
    // along this axis, then we need to flip the coordinate.  This can only happen if the containing block is both a flipped mode and perpendicular to us.
    if ((haveOrthogonalWritingModes && !logicalTopAndBottomAreAuto && child->style().isFlippedBlocksWritingMode())
        || (haveFlippedBlockAxis && !haveOrthogonalWritingModes))
        logicalTopPos = containerLogicalHeightForPositioned - logicalHeightValue - logicalTopPos;

    // Our offset is from the logical bottom edge in a flipped environment, e.g., right for vertical-rl and bottom for horizontal-bt.
    if (containerBlock.style().isFlippedBlocksWritingMode() && !haveOrthogonalWritingModes) {
        if (child->isHorizontalWritingMode())
            logicalTopPos += containerBlock.borderBottom();
        else
            logicalTopPos += containerBlock.borderRight();
    } else {
        if (child->isHorizontalWritingMode())
            logicalTopPos += containerBlock.borderTop();
        else
            logicalTopPos += containerBlock.borderLeft();
    }
}

void RenderBox::computePositionedLogicalHeightUsing(SizeType heightType, Length logicalHeightLength, const RenderBoxModelObject& containerBlock,
                                                    LayoutUnit containerLogicalHeight, LayoutUnit bordersPlusPadding, LayoutUnit logicalHeight,
                                                    Length logicalTop, Length logicalBottom, Length marginBefore, Length marginAfter,
                                                    LogicalExtentComputedValues& computedValues) const
{
    ASSERT(heightType == MinSize || heightType == MainOrPreferredSize || !logicalHeightLength.isAuto());
    if (heightType == MinSize && logicalHeightLength.isAuto()) {
        if (shouldComputeLogicalHeightFromAspectRatio())
            logicalHeightLength = Length(logicalHeight, LengthType::Fixed);
        else
            logicalHeightLength = Length(0, LengthType::Fixed);
    }

    // 'top' and 'bottom' cannot both be 'auto' because 'top would of been
    // converted to the static position in computePositionedLogicalHeight()
    ASSERT(!(logicalTop.isAuto() && logicalBottom.isAuto()));

    LayoutUnit logicalHeightValue;
    LayoutUnit contentLogicalHeight = logicalHeight - bordersPlusPadding;

    const LayoutUnit containerRelativeLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock, nullptr, false);

    LayoutUnit logicalTopValue;

    bool fromAspectRatio = heightType == MainOrPreferredSize && shouldComputeLogicalHeightFromAspectRatio();
    bool logicalHeightIsAuto = logicalHeightLength.isAuto() && !fromAspectRatio;
    bool logicalTopIsAuto = logicalTop.isAuto();
    bool logicalBottomIsAuto = logicalBottom.isAuto();

    // Height is never unsolved for tables.
    LayoutUnit resolvedLogicalHeight;
    if (isTable()) {
        resolvedLogicalHeight = contentLogicalHeight;
        logicalHeightIsAuto = false;
    } else {
        if (logicalHeightLength.isIntrinsic())
            resolvedLogicalHeight = adjustContentBoxLogicalHeightForBoxSizing(computeIntrinsicLogicalContentHeightUsing(logicalHeightLength, contentLogicalHeight, bordersPlusPadding).value());
        else if (fromAspectRatio) {
            resolvedLogicalHeight = blockSizeFromAspectRatio(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent(), style().logicalAspectRatio(), style().boxSizingForAspectRatio(), logicalWidth());
            resolvedLogicalHeight = std::max(LayoutUnit(), resolvedLogicalHeight - bordersPlusPadding);
        } else
            resolvedLogicalHeight = adjustContentBoxLogicalHeightForBoxSizing(valueForLength(logicalHeightLength, containerLogicalHeight));
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

        logicalHeightValue = resolvedLogicalHeight;
        logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);

        const LayoutUnit availableSpace = containerLogicalHeight - (logicalTopValue + logicalHeightValue + valueForLength(logicalBottom, containerLogicalHeight) + bordersPlusPadding);

        // Margins are now the only unknown
        if (marginBefore.isAuto() && marginAfter.isAuto()) {
            // Both margins auto, solve for equality
            // NOTE: This may result in negative values.
            computedValues.m_margins.m_before = availableSpace / 2; // split the difference
            computedValues.m_margins.m_after = availableSpace - computedValues.m_margins.m_before; // account for odd valued differences
        } else if (marginBefore.isAuto()) {
            // Solve for top margin
            computedValues.m_margins.m_after = valueForLength(marginAfter, containerRelativeLogicalWidth);
            computedValues.m_margins.m_before = availableSpace - computedValues.m_margins.m_after;
        } else if (marginAfter.isAuto()) {
            // Solve for bottom margin
            computedValues.m_margins.m_before = valueForLength(marginBefore, containerRelativeLogicalWidth);
            computedValues.m_margins.m_after = availableSpace - computedValues.m_margins.m_before;
        } else {
            // Over-constrained, (no need solve for bottom)
            computedValues.m_margins.m_before = valueForLength(marginBefore, containerRelativeLogicalWidth);
            computedValues.m_margins.m_after = valueForLength(marginAfter, containerRelativeLogicalWidth);
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
        computedValues.m_margins.m_before = minimumValueForLength(marginBefore, containerRelativeLogicalWidth);
        computedValues.m_margins.m_after = minimumValueForLength(marginAfter, containerRelativeLogicalWidth);

        const LayoutUnit availableSpace = containerLogicalHeight - (computedValues.m_margins.m_before + computedValues.m_margins.m_after + bordersPlusPadding);

        // Use rule/case that applies.
        if (logicalTopIsAuto && logicalHeightIsAuto && !logicalBottomIsAuto) {
            // RULE 1: (height is content based, solve of top)
            logicalHeightValue = contentLogicalHeight;
            logicalTopValue = availableSpace - (logicalHeightValue + valueForLength(logicalBottom, containerLogicalHeight));
        } else if (!logicalTopIsAuto && logicalHeightIsAuto && logicalBottomIsAuto) {
            // RULE 3: (height is content based, no need solve of bottom)
            logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
            logicalHeightValue = contentLogicalHeight;
        } else if (logicalTopIsAuto && !logicalHeightIsAuto && !logicalBottomIsAuto) {
            // RULE 4: (solve of top)
            logicalHeightValue = resolvedLogicalHeight;
            logicalTopValue = availableSpace - (logicalHeightValue + valueForLength(logicalBottom, containerLogicalHeight));
        } else if (!logicalTopIsAuto && logicalHeightIsAuto && !logicalBottomIsAuto) {
            // RULE 5: (solve of height)
            logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
            logicalHeightValue = std::max<LayoutUnit>(0, availableSpace - (logicalTopValue + valueForLength(logicalBottom, containerLogicalHeight)));
        } else if (!logicalTopIsAuto && !logicalHeightIsAuto && logicalBottomIsAuto) {
            // RULE 6: (no need solve of bottom)
            logicalHeightValue = resolvedLogicalHeight;
            logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
        }
    }
    computedValues.m_extent = logicalHeightValue;

    // Use computed values to calculate the vertical position.
    computedValues.m_position = logicalTopValue + computedValues.m_margins.m_before;
    computeLogicalTopPositionedOffset(computedValues.m_position, this, logicalHeightValue + bordersPlusPadding, containerBlock, containerLogicalHeight);
}

void RenderBox::computePositionedLogicalWidthReplaced(LogicalExtentComputedValues& computedValues) const
{
    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.3.8 "Absolutely positioned, replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-replaced-width>
    // (block-style-comments in this function correspond to text from the spec and
    // the numbers correspond to numbers in spec)

    // We don't use containingBlock(), since we may be positioned by an enclosing
    // relative positioned inline.
    const RenderBoxModelObject& containerBlock = downcast<RenderBoxModelObject>(*container());

    const LayoutUnit containerLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock);
    const LayoutUnit containerRelativeLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock, nullptr, false);

    // To match WinIE, in quirks mode use the parent's 'direction' property
    // instead of the container block's.
    TextDirection containerDirection = containerBlock.style().direction();

    // Variables to solve.
    bool isHorizontal = isHorizontalWritingMode();
    Length logicalLeft = style().logicalLeft();
    Length logicalRight = style().logicalRight();
    Length marginLogicalLeft = isHorizontal ? style().marginLeft() : style().marginTop();
    Length marginLogicalRight = isHorizontal ? style().marginRight() : style().marginBottom();
    LayoutUnit& marginLogicalLeftAlias = style().isLeftToRightDirection() ? computedValues.m_margins.m_start : computedValues.m_margins.m_end;
    LayoutUnit& marginLogicalRightAlias = style().isLeftToRightDirection() ? computedValues.m_margins.m_end : computedValues.m_margins.m_start;

    /*-----------------------------------------------------------------------*\
     * 1. The used value of 'width' is determined as for inline replaced
     *    elements.
    \*-----------------------------------------------------------------------*/
    // NOTE: This value of width is final in that the min/max width calculations
    // are dealt with in computeReplacedWidth().  This means that the steps to produce
    // correct max/min in the non-replaced version, are not necessary.
    computedValues.m_extent = computeReplacedLogicalWidth() + borderAndPaddingLogicalWidth();

    const LayoutUnit availableSpace = containerLogicalWidth - computedValues.m_extent;

    /*-----------------------------------------------------------------------*\
     * 2. If both 'left' and 'right' have the value 'auto', then if 'direction'
     *    of the containing block is 'ltr', set 'left' to the static position;
     *    else if 'direction' is 'rtl', set 'right' to the static position.
    \*-----------------------------------------------------------------------*/
    // see FIXME 1
    computeInlineStaticDistance(logicalLeft, logicalRight, this, containerBlock, containerLogicalWidth, nullptr); // FIXME: Pass the fragment.

    /*-----------------------------------------------------------------------*\
     * 3. If 'left' or 'right' are 'auto', replace any 'auto' on 'margin-left'
     *    or 'margin-right' with '0'.
    \*-----------------------------------------------------------------------*/
    if (logicalLeft.isAuto() || logicalRight.isAuto()) {
        if (marginLogicalLeft.isAuto())
            marginLogicalLeft.setValue(LengthType::Fixed, 0);
        if (marginLogicalRight.isAuto())
            marginLogicalRight.setValue(LengthType::Fixed, 0);
    }

    /*-----------------------------------------------------------------------*\
     * 4. If at this point both 'margin-left' and 'margin-right' are still
     *    'auto', solve the equation under the extra constraint that the two
     *    margins must get equal values, unless this would make them negative,
     *    in which case when the direction of the containing block is 'ltr'
     *    ('rtl'), set 'margin-left' ('margin-right') to zero and solve for
     *    'margin-right' ('margin-left').
    \*-----------------------------------------------------------------------*/
    LayoutUnit logicalLeftValue;
    LayoutUnit logicalRightValue;

    if (marginLogicalLeft.isAuto() && marginLogicalRight.isAuto()) {
        // 'left' and 'right' cannot be 'auto' due to step 3
        ASSERT(!(logicalLeft.isAuto() && logicalRight.isAuto()));

        logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);
        logicalRightValue = valueForLength(logicalRight, containerLogicalWidth);

        LayoutUnit difference = availableSpace - (logicalLeftValue + logicalRightValue);
        if (difference > 0) {
            marginLogicalLeftAlias = difference / 2; // split the difference
            marginLogicalRightAlias = difference - marginLogicalLeftAlias; // account for odd valued differences
        } else {
            // Use the containing block's direction rather than the parent block's
            // per CSS 2.1 reference test abspos-replaced-width-margin-000.
            if (containerDirection == TextDirection::LTR) {
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
        marginLogicalLeftAlias = valueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
        marginLogicalRightAlias = valueForLength(marginLogicalRight, containerRelativeLogicalWidth);
        logicalRightValue = valueForLength(logicalRight, containerLogicalWidth);

        // Solve for 'left'
        logicalLeftValue = availableSpace - (logicalRightValue + marginLogicalLeftAlias + marginLogicalRightAlias);
    } else if (logicalRight.isAuto()) {
        marginLogicalLeftAlias = valueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
        marginLogicalRightAlias = valueForLength(marginLogicalRight, containerRelativeLogicalWidth);
        logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);

        // Solve for 'right'
        logicalRightValue = availableSpace - (logicalLeftValue + marginLogicalLeftAlias + marginLogicalRightAlias);
    } else if (marginLogicalLeft.isAuto()) {
        marginLogicalRightAlias = valueForLength(marginLogicalRight, containerRelativeLogicalWidth);
        logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);
        logicalRightValue = valueForLength(logicalRight, containerLogicalWidth);

        // Solve for 'margin-left'
        marginLogicalLeftAlias = availableSpace - (logicalLeftValue + logicalRightValue + marginLogicalRightAlias);
    } else if (marginLogicalRight.isAuto()) {
        marginLogicalLeftAlias = valueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
        logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);
        logicalRightValue = valueForLength(logicalRight, containerLogicalWidth);

        // Solve for 'margin-right'
        marginLogicalRightAlias = availableSpace - (logicalLeftValue + logicalRightValue + marginLogicalLeftAlias);
    } else {
        // Nothing is 'auto', just calculate the values.
        marginLogicalLeftAlias = valueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
        marginLogicalRightAlias = valueForLength(marginLogicalRight, containerRelativeLogicalWidth);
        logicalRightValue = valueForLength(logicalRight, containerLogicalWidth);
        logicalLeftValue = valueForLength(logicalLeft, containerLogicalWidth);
        // If the containing block is right-to-left, then push the left position as far to the right as possible
        if (containerDirection == TextDirection::RTL) {
            int totalLogicalWidth = computedValues.m_extent + logicalLeftValue + logicalRightValue +  marginLogicalLeftAlias + marginLogicalRightAlias;
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
    if (is<RenderInline>(containerBlock) && !containerBlock.style().isLeftToRightDirection()) {
        const auto& flow = downcast<RenderInline>(containerBlock);
        LegacyInlineFlowBox* firstLine = flow.firstLineBox();
        LegacyInlineFlowBox* lastLine = flow.lastLineBox();
        if (firstLine && lastLine && firstLine != lastLine) {
            computedValues.m_position = logicalLeftValue + marginLogicalLeftAlias + lastLine->borderLogicalLeft() + (lastLine->logicalLeft() - firstLine->logicalLeft());
            return;
        }
    }

    LayoutUnit logicalLeftPos = logicalLeftValue + marginLogicalLeftAlias;
    // Border and padding have already been included in computedValues.m_extent.
    computeLogicalLeftPositionedOffset(logicalLeftPos, this, computedValues.m_extent, containerBlock, containerLogicalWidth, style().logicalLeft().isAuto(), style().logicalRight().isAuto());
    computedValues.m_position = logicalLeftPos;
}

void RenderBox::computePositionedLogicalHeightReplaced(LogicalExtentComputedValues& computedValues) const
{
    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.6.5 "Absolutely positioned, replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-replaced-height>
    // (block-style-comments in this function correspond to text from the spec and
    // the numbers correspond to numbers in spec)

    // We don't use containingBlock(), since we may be positioned by an enclosing relpositioned inline.
    const RenderBoxModelObject& containerBlock = downcast<RenderBoxModelObject>(*container());

    const LayoutUnit containerLogicalHeight = containingBlockLogicalHeightForPositioned(containerBlock);
    const LayoutUnit containerRelativeLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock, nullptr, false);

    // Variables to solve.
    Length marginBefore = style().marginBefore();
    Length marginAfter = style().marginAfter();
    LayoutUnit& marginBeforeAlias = computedValues.m_margins.m_before;
    LayoutUnit& marginAfterAlias = computedValues.m_margins.m_after;

    Length logicalTop = style().logicalTop();
    Length logicalBottom = style().logicalBottom();

    /*-----------------------------------------------------------------------*\
     * 1. The used value of 'height' is determined as for inline replaced
     *    elements.
    \*-----------------------------------------------------------------------*/
    // NOTE: This value of height is final in that the min/max height calculations
    // are dealt with in computeReplacedHeight().  This means that the steps to produce
    // correct max/min in the non-replaced version, are not necessary.
    computedValues.m_extent = computeReplacedLogicalHeight() + borderAndPaddingLogicalHeight();
    const LayoutUnit availableSpace = containerLogicalHeight - computedValues.m_extent;

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
            marginBefore.setValue(LengthType::Fixed, 0);
        if (marginAfter.isAuto())
            marginAfter.setValue(LengthType::Fixed, 0);
    }

    /*-----------------------------------------------------------------------*\
     * 4. If at this point both 'margin-top' and 'margin-bottom' are still
     *    'auto', solve the equation under the extra constraint that the two
     *    margins must get equal values.
    \*-----------------------------------------------------------------------*/
    LayoutUnit logicalTopValue;
    LayoutUnit logicalBottomValue;

    if (marginBefore.isAuto() && marginAfter.isAuto()) {
        // 'top' and 'bottom' cannot be 'auto' due to step 2 and 3 combined.
        ASSERT(!(logicalTop.isAuto() || logicalBottom.isAuto()));

        logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
        logicalBottomValue = valueForLength(logicalBottom, containerLogicalHeight);

        LayoutUnit difference = availableSpace - (logicalTopValue + logicalBottomValue);
        // NOTE: This may result in negative values.
        marginBeforeAlias =  difference / 2; // split the difference
        marginAfterAlias = difference - marginBeforeAlias; // account for odd valued differences

    /*-----------------------------------------------------------------------*\
     * 5. If at this point there is only one 'auto' left, solve the equation
     *    for that value.
    \*-----------------------------------------------------------------------*/
    } else if (logicalTop.isAuto()) {
        marginBeforeAlias = valueForLength(marginBefore, containerRelativeLogicalWidth);
        marginAfterAlias = valueForLength(marginAfter, containerRelativeLogicalWidth);
        logicalBottomValue = valueForLength(logicalBottom, containerLogicalHeight);

        // Solve for 'top'
        logicalTopValue = availableSpace - (logicalBottomValue + marginBeforeAlias + marginAfterAlias);
    } else if (logicalBottom.isAuto()) {
        marginBeforeAlias = valueForLength(marginBefore, containerRelativeLogicalWidth);
        marginAfterAlias = valueForLength(marginAfter, containerRelativeLogicalWidth);
        logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);

        // Solve for 'bottom'
        // NOTE: It is not necessary to solve for 'bottom' because we don't ever
        // use the value.
    } else if (marginBefore.isAuto()) {
        marginAfterAlias = valueForLength(marginAfter, containerRelativeLogicalWidth);
        logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
        logicalBottomValue = valueForLength(logicalBottom, containerLogicalHeight);

        // Solve for 'margin-top'
        marginBeforeAlias = availableSpace - (logicalTopValue + logicalBottomValue + marginAfterAlias);
    } else if (marginAfter.isAuto()) {
        marginBeforeAlias = valueForLength(marginBefore, containerRelativeLogicalWidth);
        logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
        logicalBottomValue = valueForLength(logicalBottom, containerLogicalHeight);

        // Solve for 'margin-bottom'
        marginAfterAlias = availableSpace - (logicalTopValue + logicalBottomValue + marginBeforeAlias);
    } else {
        // Nothing is 'auto', just calculate the values.
        marginBeforeAlias = valueForLength(marginBefore, containerRelativeLogicalWidth);
        marginAfterAlias = valueForLength(marginAfter, containerRelativeLogicalWidth);
        logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
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
    // Border and padding have already been included in computedValues.m_extent.
    computeLogicalTopPositionedOffset(logicalTopPos, this, computedValues.m_extent, containerBlock, containerLogicalHeight);
    computedValues.m_position = logicalTopPos;
}

VisiblePosition RenderBox::positionForPoint(const LayoutPoint& point, const RenderFragmentContainer* fragment)
{
    // no children...return this render object's element, if there is one, and offset 0
    if (!firstChild())
        return createVisiblePosition(nonPseudoElement() ? firstPositionInOrBeforeNode(nonPseudoElement()) : Position());

    if (isTable() && nonPseudoElement()) {
        LayoutUnit right = contentWidth() + horizontalBorderAndPaddingExtent();
        LayoutUnit bottom = contentHeight() + verticalBorderAndPaddingExtent();
        
        if (point.x() < 0 || point.x() > right || point.y() < 0 || point.y() > bottom) {
            if (point.x() <= right / 2)
                return createVisiblePosition(firstPositionInOrBeforeNode(nonPseudoElement()));
            return createVisiblePosition(lastPositionInOrAfterNode(nonPseudoElement()));
        }
    }

    // Pass off to the closest child.
    LayoutUnit minDist = LayoutUnit::max();
    RenderBox* closestRenderer = nullptr;
    LayoutPoint adjustedPoint = point;
    if (isTableRow())
        adjustedPoint.moveBy(location());

    for (auto& renderer : childrenOfType<RenderBox>(*this)) {
        if (is<RenderFragmentedFlow>(*this)) {
            ASSERT(fragment);
            if (!downcast<RenderFragmentedFlow>(*this).objectShouldFragmentInFlowFragment(&renderer, fragment))
                continue;
        }

        if ((!renderer.firstChild() && !renderer.isInline() && !is<RenderBlockFlow>(renderer))
            || renderer.style().visibility() != Visibility::Visible)
            continue;

        LayoutUnit top = renderer.borderTop() + renderer.paddingTop() + (is<RenderTableRow>(*this) ? 0_lu : renderer.y());
        LayoutUnit bottom = top + renderer.contentHeight();
        LayoutUnit left = renderer.borderLeft() + renderer.paddingLeft() + (is<RenderTableRow>(*this) ? 0_lu : renderer.x());
        LayoutUnit right = left + renderer.contentWidth();
        
        if (point.x() <= right && point.x() >= left && point.y() <= top && point.y() >= bottom) {
            if (is<RenderTableRow>(renderer))
                return renderer.positionForPoint(point + adjustedPoint - renderer.locationOffset(), fragment);
            return renderer.positionForPoint(point - renderer.locationOffset(), fragment);
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
            closestRenderer = &renderer;
            minDist = dist;
        }
    }
    
    if (closestRenderer)
        return closestRenderer->positionForPoint(adjustedPoint - closestRenderer->locationOffset(), fragment);
    
    return createVisiblePosition(firstPositionInOrBeforeNode(nonPseudoElement()));
}

bool RenderBox::shrinkToAvoidFloats() const
{
    // Floating objects don't shrink.  Objects that don't avoid floats don't shrink.  Marquees don't shrink.
    if ((isInline() && !isHTMLMarquee()) || !avoidsFloats() || isFloating())
        return false;
    
    // Only auto width objects can possibly shrink to avoid floats.
    return style().width().isAuto();
}

bool RenderBox::createsNewFormattingContext() const
{
    return isInlineBlockOrInlineTable() || isFloatingOrOutOfFlowPositioned() || hasPotentiallyScrollableOverflow() || isFlexItemIncludingDeprecated()
        || isTableCell() || isTableCaption() || isFieldset() || isWritingModeRoot() || isDocumentElementRenderer() || isRenderFragmentedFlow()
        || style().containsLayout() || paintContainmentApplies() || isGridItem() || style().specifiesColumns() || style().columnSpan() == ColumnSpan::All || style().display() == DisplayType::FlowRoot;
}

bool RenderBox::avoidsFloats() const
{
    return isReplacedOrInlineBlock() || isHR() || isLegend() || isFieldset() || createsNewFormattingContext();
}

void RenderBox::addVisualEffectOverflow()
{
    bool hasBoxShadow = style().boxShadow();
    bool hasBorderImageOutsets = style().hasBorderImageOutsets();
    bool hasOutline = outlineStyleForRepaint().hasOutlineInVisualOverflow();
    if (!hasBoxShadow && !hasBorderImageOutsets && !hasOutline)
        return;

    addVisualOverflow(applyVisualEffectOverflow(borderBoxRect()));

    if (auto* fragmentedFlow = enclosingFragmentedFlow())
        fragmentedFlow->addFragmentsVisualEffectOverflow(this);
}

LayoutRect RenderBox::applyVisualEffectOverflow(const LayoutRect& borderBox) const
{
    bool isFlipped = style().isFlippedBlocksWritingMode();
    bool isHorizontal = isHorizontalWritingMode();
    
    LayoutUnit overflowMinX = borderBox.x();
    LayoutUnit overflowMaxX = borderBox.maxX();
    LayoutUnit overflowMinY = borderBox.y();
    LayoutUnit overflowMaxY = borderBox.maxY();
    
    // Compute box-shadow overflow first.
    if (style().boxShadow()) {
        auto shadowExtent = style().boxShadowExtent();

        // In flipped blocks writing modes such as vertical-rl, the physical right shadow value is actually at the lower x-coordinate.
        overflowMinX = borderBox.x() + ((!isFlipped || isHorizontal) ? shadowExtent.left() : -shadowExtent.right());
        overflowMaxX = borderBox.maxX() + ((!isFlipped || isHorizontal) ? shadowExtent.right() : -shadowExtent.left());
        overflowMinY = borderBox.y() + ((!isFlipped || !isHorizontal) ? shadowExtent.top() : -shadowExtent.bottom());
        overflowMaxY = borderBox.maxY() + ((!isFlipped || !isHorizontal) ? shadowExtent.bottom() : -shadowExtent.top());
    }

    // Now compute border-image-outset overflow.
    if (style().hasBorderImageOutsets()) {
        auto borderOutsets = style().borderImageOutsets();
        
        // In flipped blocks writing modes, the physical sides are inverted. For example in vertical-rl, the right
        // border is at the lower x coordinate value.
        overflowMinX = std::min(overflowMinX, borderBox.x() - ((!isFlipped || isHorizontal) ? borderOutsets.left() : borderOutsets.right()));
        overflowMaxX = std::max(overflowMaxX, borderBox.maxX() + ((!isFlipped || isHorizontal) ? borderOutsets.right() : borderOutsets.left()));
        overflowMinY = std::min(overflowMinY, borderBox.y() - ((!isFlipped || !isHorizontal) ? borderOutsets.top() : borderOutsets.bottom()));
        overflowMaxY = std::max(overflowMaxY, borderBox.maxY() + ((!isFlipped || !isHorizontal) ? borderOutsets.bottom() : borderOutsets.top()));
    }

    if (outlineStyleForRepaint().hasOutlineInVisualOverflow()) {
        LayoutUnit outlineSize { outlineStyleForRepaint().outlineSize() };
        overflowMinX = std::min(overflowMinX, borderBox.x() - outlineSize);
        overflowMaxX = std::max(overflowMaxX, borderBox.maxX() + outlineSize);
        overflowMinY = std::min(overflowMinY, borderBox.y() - outlineSize);
        overflowMaxY = std::max(overflowMaxY, borderBox.maxY() + outlineSize);
    }
    // Add in the final overflow with shadows and outsets combined.
    return LayoutRect(overflowMinX, overflowMinY, overflowMaxX - overflowMinX, overflowMaxY - overflowMinY);
}

void RenderBox::addOverflowFromChild(const RenderBox* child, const LayoutSize& delta)
{
    // Never allow flow threads to propagate overflow up to a parent.
    if (child->isRenderFragmentedFlow())
        return;

    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (fragmentedFlow)
        fragmentedFlow->addFragmentsOverflowFromChild(this, child, delta);

    // Only propagate layout overflow from the child if the child isn't clipping its overflow.  If it is, then
    // its overflow is internal to it, and we don't care about it. layoutOverflowRectForPropagation takes care of this
    // and just propagates the border box rect instead.
    LayoutRect childLayoutOverflowRect = child->layoutOverflowRectForPropagation(&style());
    childLayoutOverflowRect.move(delta);
    addLayoutOverflow(childLayoutOverflowRect);

    if (paintContainmentApplies())
        return;

    // Add in visual overflow from the child.  Even if the child clips its overflow, it may still
    // have visual overflow of its own set from box shadows or reflections. It is unnecessary to propagate this
    // overflow if we are clipping our own overflow.
    if (child->hasSelfPaintingLayer() || hasPotentiallyScrollableOverflow())
        return;
    LayoutRect childVisualOverflowRect = child->visualOverflowRectForPropagation(&style());
    childVisualOverflowRect.move(delta);
    addVisualOverflow(childVisualOverflowRect);
}

void RenderBox::addLayoutOverflow(const LayoutRect& rect)
{
    LayoutRect clientBox = flippedClientBoxRect();
    if (clientBox.contains(rect) || rect.isEmpty())
        return;
    
    // For overflow clip objects, we don't want to propagate overflow into unreachable areas.
    LayoutRect overflowRect(rect);
    if (hasPotentiallyScrollableOverflow() || isRenderView()) {
        // Overflow is in the block's coordinate space and thus is flipped for horizontal-bt and vertical-rl 
        // writing modes. At this stage that is actually a simplification, since we can treat horizontal-tb/bt as the same
        // and vertical-lr/rl as the same.
        bool hasTopOverflow = isTopLayoutOverflowAllowed();
        bool hasLeftOverflow = isLeftLayoutOverflowAllowed();

        if (!hasTopOverflow)
            overflowRect.shiftYEdgeTo(std::max(overflowRect.y(), clientBox.y()));
        else
            overflowRect.shiftMaxYEdgeTo(std::min(overflowRect.maxY(), clientBox.maxY()));
        if (!hasLeftOverflow)
            overflowRect.shiftXEdgeTo(std::max(overflowRect.x(), clientBox.x()));
        else
            overflowRect.shiftMaxXEdgeTo(std::min(overflowRect.maxX(), clientBox.maxX()));
        
        // Now re-test with the adjusted rectangle and see if it has become unreachable or fully
        // contained.
        if (clientBox.contains(overflowRect) || overflowRect.isEmpty())
            return;
    }

    if (!m_overflow)
        m_overflow = adoptRef(new RenderOverflow(clientBox, borderBoxRect()));
    
    m_overflow->addLayoutOverflow(overflowRect);
}

void RenderBox::addVisualOverflow(const LayoutRect& rect)
{
    LayoutRect borderBox = borderBoxRect();
    if (borderBox.contains(rect) || rect.isEmpty())
        return;
        
    if (!m_overflow)
        m_overflow = adoptRef(new RenderOverflow(flippedClientBoxRect(), borderBox));
    
    m_overflow->addVisualOverflow(rect);
}

void RenderBox::clearOverflow()
{
    m_overflow = nullptr;
    RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (fragmentedFlow)
        fragmentedFlow->clearFragmentsOverflow(this);
}
    
bool RenderBox::percentageLogicalHeightIsResolvable() const
{
    // Do this to avoid duplicating all the logic that already exists when computing
    // an actual percentage height.
    Length fakeLength(100, LengthType::Percent);
    return computePercentageLogicalHeight(fakeLength) != std::nullopt;
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
    return !style().logicalHeight().isIntrinsicOrAuto()
        || (!style().logicalMaxHeight().isIntrinsicOrAuto() && !style().logicalMaxHeight().isUndefined() && (!style().logicalMaxHeight().isPercentOrCalculated() || percentageLogicalHeightIsResolvable()))
        || (!style().logicalMinHeight().isIntrinsicOrAuto() && style().logicalMinHeight().isPositive() && (!style().logicalMinHeight().isPercentOrCalculated() || percentageLogicalHeightIsResolvable()));
}

bool RenderBox::isUnsplittableForPagination() const
{
    return isReplacedOrInlineBlock()
        || hasUnsplittableScrollingOverflow()
        || (parent() && isWritingModeRoot())
        || (isFloating() && style().styleType() == PseudoId::FirstLetter && style().initialLetterDrop() > 0)
        || shouldApplySizeContainment(*this);
}

LayoutUnit RenderBox::lineHeight(bool /*firstLine*/, LineDirectionMode direction, LinePositionMode /*linePositionMode*/) const
{
    if (isReplacedOrInlineBlock())
        return direction == HorizontalLine ? m_marginBox.top() + height() + m_marginBox.bottom() : m_marginBox.right() + width() + m_marginBox.left();
    return 0;
}

LayoutUnit RenderBox::baselinePosition(FontBaseline baselineType, bool /*firstLine*/, LineDirectionMode direction, LinePositionMode /*linePositionMode*/) const
{
    if (isReplacedOrInlineBlock()) {
        auto result = roundToInt(direction == HorizontalLine ? m_marginBox.top() + height() + m_marginBox.bottom() : m_marginBox.right() + width() + m_marginBox.left());
        if (baselineType == AlphabeticBaseline)
            return result;
        return result - result / 2;
    }
    return 0;
}

RenderLayer* RenderBox::enclosingFloatPaintingLayer() const
{
    for (auto& box : lineageOfType<RenderBox>(*this)) {
        if (box.layer() && box.layer()->isSelfPaintingLayer())
            return box.layer();
    }
    return nullptr;
}

LayoutRect RenderBox::logicalVisualOverflowRectForPropagation(const RenderStyle* parentStyle) const
{
    LayoutRect rect = visualOverflowRectForPropagation(parentStyle);
    if (!parentStyle->isHorizontalWritingMode())
        return rect.transposedRect();
    return rect;
}

LayoutRect RenderBox::visualOverflowRectForPropagation(const RenderStyle* parentStyle) const
{
    // If the writing modes of the child and parent match, then we don't have to 
    // do anything fancy. Just return the result.
    LayoutRect rect = visualOverflowRect();
    if (parentStyle->writingMode() == style().writingMode())
        return rect;
    
    // We are putting ourselves into our parent's coordinate space.  If there is a flipped block mismatch
    // in a particular axis, then we have to flip the rect along that axis.
    if (style().writingMode() == WritingMode::RightToLeft || parentStyle->writingMode() == WritingMode::RightToLeft)
        rect.setX(width() - rect.maxX());
    else if (style().writingMode() == WritingMode::BottomToTop || parentStyle->writingMode() == WritingMode::BottomToTop)
        rect.setY(height() - rect.maxY());

    return rect;
}

LayoutRect RenderBox::logicalLayoutOverflowRectForPropagation(const RenderStyle* parentStyle) const
{
    LayoutRect rect = layoutOverflowRectForPropagation(parentStyle);
    if (!parentStyle->isHorizontalWritingMode())
        return rect.transposedRect();
    return rect;
}

LayoutRect RenderBox::layoutOverflowRectForPropagation(const RenderStyle* parentStyle) const
{
    // Only propagate interior layout overflow if we don't completely clip it.
    LayoutRect rect = borderBoxRect();
    if (!shouldApplyLayoutContainment(*this)) {
        if (style().overflowX() == Overflow::Clip && style().overflowY() == Overflow::Visible) {
            LayoutRect clippedOverflowRect = layoutOverflowRect();
            clippedOverflowRect.setX(rect.x());
            clippedOverflowRect.setWidth(rect.width());
            rect.unite(clippedOverflowRect);
        } else if (style().overflowY() == Overflow::Clip && style().overflowX() == Overflow::Visible) {
            LayoutRect clippedOverflowRect = layoutOverflowRect();
            clippedOverflowRect.setY(rect.y());
            clippedOverflowRect.setHeight(rect.height());
            rect.unite(clippedOverflowRect);
        } else if (!hasNonVisibleOverflow())
            rect.unite(layoutOverflowRect());
    }

    bool hasTransform = this->hasTransform();
    // While a stickily positioned renderer is also inflow positioned, they stretch the overflow rect with their inflow geometry
    // (as opposed to the paint geometry) because they are not stationary.
    bool paintGeometryAffectsLayoutOverflow = hasTransform || (isInFlowPositioned() && !isStickilyPositioned());
    if (paintGeometryAffectsLayoutOverflow) {
        // If we are relatively positioned or if we have a transform, then we have to convert
        // this rectangle into physical coordinates, apply relative positioning and transforms
        // to it, and then convert it back.
        // It ensures that the overflow rect tracks the paint geometry and not the inflow layout position.
        flipForWritingMode(rect);
        
        if (hasTransform && hasLayer())
            rect = layer()->currentTransform().mapRect(rect);

        if (isInFlowPositioned())
            rect.move(offsetForInFlowPosition());
        
        // Now we need to flip back.
        flipForWritingMode(rect);
    }
    
    // If the writing modes of the child and parent match, then we don't have to 
    // do anything fancy. Just return the result.
    if (parentStyle->writingMode() == style().writingMode())
        return rect;
    
    // We are putting ourselves into our parent's coordinate space.  If there is a flipped block mismatch
    // in a particular axis, then we have to flip the rect along that axis.
    if (style().writingMode() == WritingMode::RightToLeft || parentStyle->writingMode() == WritingMode::RightToLeft)
        rect.setX(width() - rect.maxX());
    else if (style().writingMode() == WritingMode::BottomToTop || parentStyle->writingMode() == WritingMode::BottomToTop)
        rect.setY(height() - rect.maxY());

    return rect;
}

LayoutRect RenderBox::flippedClientBoxRect() const
{
    // Because of the special coordinate system used for overflow rectangles (not quite logical, not
    // quite physical), we need to flip the block progression coordinate in vertical-rl and
    // horizontal-bt writing modes. Apart from that, this method does the same as clientBoxRect().

    LayoutUnit left = borderLeft();
    LayoutUnit top = borderTop();
    LayoutUnit right = borderRight();
    LayoutUnit bottom = borderBottom();
    // Calculate physical padding box.
    LayoutRect rect(left, top, width() - left - right, height() - top - bottom);
    // Flip block progression axis if writing mode is vertical-rl or horizontal-bt.
    flipForWritingMode(rect);
    // Subtract space occupied by scrollbars. They are at their physical edge in this coordinate
    // system, so order is important here: first flip, then subtract scrollbars.
    if (shouldPlaceVerticalScrollbarOnLeft() && isHorizontalWritingMode())
        rect.move(verticalScrollbarWidth(), 0);
    rect.contract(verticalScrollbarWidth(), horizontalScrollbarHeight());
    return rect;
}

LayoutUnit RenderBox::offsetLeft() const
{
    return adjustedPositionRelativeToOffsetParent(topLeftLocation()).x();
}

LayoutUnit RenderBox::offsetTop() const
{
    return adjustedPositionRelativeToOffsetParent(topLeftLocation()).y();
}

LayoutPoint RenderBox::flipForWritingModeForChild(const RenderBox& child, const LayoutPoint& point) const
{
    if (!style().isFlippedBlocksWritingMode())
        return point;
    
    // The child is going to add in its x() and y(), so we have to make sure it ends up in
    // the right place.
    if (isHorizontalWritingMode())
        return LayoutPoint(point.x(), point.y() + height() - child.height() - (2 * child.y()));
    return LayoutPoint(point.x() + width() - child.width() - (2 * child.x()), point.y());
}

void RenderBox::flipForWritingMode(LayoutRect& rect) const
{
    if (!style().isFlippedBlocksWritingMode())
        return;

    if (isHorizontalWritingMode())
        rect.setY(height() - rect.maxY());
    else
        rect.setX(width() - rect.maxX());
}

LayoutUnit RenderBox::flipForWritingMode(LayoutUnit position) const
{
    if (!style().isFlippedBlocksWritingMode())
        return position;
    return logicalHeight() - position;
}

LayoutPoint RenderBox::flipForWritingMode(const LayoutPoint& position) const
{
    if (!style().isFlippedBlocksWritingMode())
        return position;
    return isHorizontalWritingMode() ? LayoutPoint(position.x(), height() - position.y()) : LayoutPoint(width() - position.x(), position.y());
}

LayoutSize RenderBox::flipForWritingMode(const LayoutSize& offset) const
{
    if (!style().isFlippedBlocksWritingMode())
        return offset;
    return isHorizontalWritingMode() ? LayoutSize(offset.width(), height() - offset.height()) : LayoutSize(width() - offset.width(), offset.height());
}

FloatPoint RenderBox::flipForWritingMode(const FloatPoint& position) const
{
    if (!style().isFlippedBlocksWritingMode())
        return position;
    return isHorizontalWritingMode() ? FloatPoint(position.x(), height() - position.y()) : FloatPoint(width() - position.x(), position.y());
}

void RenderBox::flipForWritingMode(FloatRect& rect) const
{
    if (!style().isFlippedBlocksWritingMode())
        return;

    if (isHorizontalWritingMode())
        rect.setY(height() - rect.maxY());
    else
        rect.setX(width() - rect.maxX());
}

LayoutPoint RenderBox::topLeftLocation() const
{
    if (!view().frameView().hasFlippedBlockRenderers())
        return location();
    
    RenderBlock* containerBlock = containingBlock();
    if (!containerBlock || containerBlock == this)
        return location();
    return containerBlock->flipForWritingModeForChild(*this, location());
}

LayoutSize RenderBox::topLeftLocationOffset() const
{
    if (!view().frameView().hasFlippedBlockRenderers())
        return locationOffset();

    RenderBlock* containerBlock = containingBlock();
    if (!containerBlock || containerBlock == this)
        return locationOffset();
    
    LayoutRect rect(frameRect());
    containerBlock->flipForWritingMode(rect); // FIXME: This is wrong if we are an absolutely positioned object enclosed by a relative-positioned inline.
    return LayoutSize(rect.x(), rect.y());
}

void RenderBox::applyTopLeftLocationOffsetWithFlipping(LayoutPoint& point) const
{
    RenderBlock* containerBlock = containingBlock();
    if (!containerBlock || containerBlock == this) {
        point.move(m_frameRect.x(), m_frameRect.y());
        return;
    }
    
    LayoutRect rect(frameRect());
    containerBlock->flipForWritingMode(rect); // FIXME: This is wrong if we are an absolutely positioned object  enclosed by a relative-positioned inline.
    point.move(rect.x(), rect.y());
}

bool RenderBox::shouldIgnoreAspectRatio() const
{
    return !style().hasAspectRatio() || isTablePart();
}

static inline bool shouldComputeLogicalWidthFromAspectRatioAndInsets(const RenderBox& renderer)
{
    if (!renderer.isOutOfFlowPositioned())
        return false;

    auto& style = renderer.style();
    if (!style.logicalWidth().isAuto()) {
        // Not applicable for aspect ratio computation.
        return false;
    }
    // When both left and right are set, the out-of-flow positioned box is horizontally constrained and aspect ratio for the logical width is not applicable.
    auto hasConstrainedWidth = (!style.logicalLeft().isAuto() && !style.logicalRight().isAuto()) || renderer.intrinsicLogicalWidth();
    if (hasConstrainedWidth)
        return false;

    // When both top and bottom are set, the out-of-flow positioned box is vertically constrained and it can be used as if it had a non-auto height value.
    auto hasConstrainedHeight = !style.logicalTop().isAuto() && !style.logicalBottom().isAuto();
    if (!hasConstrainedHeight)
        return false;
    // FIXME: This could probably be omitted and let the callers handle the height check (as they seem to be doing anyway).
    return style.logicalHeight().isAuto();
}

bool RenderBox::shouldComputeLogicalHeightFromAspectRatio() const
{
    if (shouldIgnoreAspectRatio())
        return false;

    if (shouldComputeLogicalWidthFromAspectRatioAndInsets(*this))
        return false;

    auto h = style().logicalHeight();
    return h.isAuto() || h.isIntrinsic() || (!isOutOfFlowPositioned() && h.isPercentOrCalculated() && !percentageLogicalHeightIsResolvable());
}

bool RenderBox::shouldComputeLogicalWidthFromAspectRatio() const
{
    if (shouldIgnoreAspectRatio())
        return false;

    if (isGridItem()) {
        if (shouldComputeSizeAsReplaced()) {
            if (hasStretchedLogicalWidth() && hasStretchedLogicalHeight())
                return false;
        } else if (hasStretchedLogicalWidth(StretchingMode::Explicit))
            return false;
    }

    auto isResolvablePercentageHeight = [&] {
        return style().logicalHeight().isPercentOrCalculated() && (isOutOfFlowPositioned() || percentageLogicalHeightIsResolvable());
    };
    return hasOverridingLogicalHeight() || shouldComputeLogicalWidthFromAspectRatioAndInsets(*this) || style().logicalHeight().isFixed() || isResolvablePercentageHeight();
}

LayoutUnit RenderBox::computeLogicalWidthFromAspectRatioInternal() const
{
    ASSERT(shouldComputeLogicalWidthFromAspectRatio());
    auto computedValues = computeLogicalHeight(logicalHeight(), logicalTop());
    LayoutUnit logicalHeightforAspectRatio = computedValues.m_extent;

    return inlineSizeFromAspectRatio(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent(), style().logicalAspectRatio(), style().boxSizingForAspectRatio(), logicalHeightforAspectRatio);
}

LayoutUnit RenderBox::computeLogicalWidthFromAspectRatio(RenderFragmentContainer* fragment) const
{
    auto logicalWidth = computeLogicalWidthFromAspectRatioInternal();
    LayoutUnit containerWidthInInlineDirection = std::max<LayoutUnit>(0, containingBlockLogicalWidthForContentInFragment(fragment));
    return constrainLogicalWidthInFragmentByMinMax(logicalWidth, containerWidthInInlineDirection, *containingBlock(), fragment, AllowIntrinsic::No);
}

std::pair<LayoutUnit, LayoutUnit> RenderBox::computeMinMaxLogicalWidthFromAspectRatio() const
{
    LayoutUnit blockMinSize = constrainLogicalHeightByMinMax(LayoutUnit(), std::nullopt);
    LayoutUnit blockMaxSize = constrainLogicalHeightByMinMax(LayoutUnit::max(), std::nullopt);
    LayoutUnit transferredMinSize = LayoutUnit();
    LayoutUnit transferredMaxSize = LayoutUnit::max();
    if (blockMinSize > LayoutUnit())
        transferredMinSize = inlineSizeFromAspectRatio(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent(), style().logicalAspectRatio(), style().boxSizingForAspectRatio(), blockMinSize);
    if (blockMaxSize != LayoutUnit::max())
        transferredMaxSize = inlineSizeFromAspectRatio(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent(), style().logicalAspectRatio(), style().boxSizingForAspectRatio(), blockMaxSize);
    // Minimum size wins over maximum size.
    transferredMaxSize = std::max(transferredMaxSize, transferredMinSize);
    return { transferredMinSize, transferredMaxSize };
}

bool RenderBox::hasRelativeDimensions() const
{
    return style().height().isPercentOrCalculated() || style().width().isPercentOrCalculated()
        || style().maxHeight().isPercentOrCalculated() || style().maxWidth().isPercentOrCalculated()
        || style().minHeight().isPercentOrCalculated() || style().minWidth().isPercentOrCalculated();
}

bool RenderBox::hasRelativeLogicalHeight() const
{
    return style().logicalHeight().isPercentOrCalculated()
        || style().logicalMinHeight().isPercentOrCalculated()
        || style().logicalMaxHeight().isPercentOrCalculated();
}

bool RenderBox::hasRelativeLogicalWidth() const
{
    return style().logicalWidth().isPercentOrCalculated()
        || style().logicalMinWidth().isPercentOrCalculated()
        || style().logicalMaxWidth().isPercentOrCalculated();
}

LayoutUnit RenderBox::offsetFromLogicalTopOfFirstPage() const
{
    auto* layoutState = view().frameView().layoutContext().layoutState();
    if ((layoutState && !layoutState->isPaginated()) || (!layoutState && !enclosingFragmentedFlow()))
        return 0;

    RenderBlock* containerBlock = containingBlock();
    return containerBlock->offsetFromLogicalTopOfFirstPage() + logicalTop();
}

LayoutBoxExtent RenderBox::scrollPaddingForViewportRect(const LayoutRect& viewportRect)
{
    // We are using minimumValueForLength here, because scroll-padding values might be "auto". WebKit currently
    // interprets "auto" as 0. See: https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-padding
    const auto& padding = style().scrollPadding();
    return LayoutBoxExtent(
        minimumValueForLength(padding.top(), viewportRect.height()), minimumValueForLength(padding.right(), viewportRect.width()),
        minimumValueForLength(padding.bottom(), viewportRect.height()), minimumValueForLength(padding.left(), viewportRect.width()));
}

LayoutUnit synthesizedBaselineFromBorderBox(const RenderBox& box, LineDirectionMode direction)
{
    return direction == HorizontalLine ? box.height() : box.width();
}

} // namespace WebCore
