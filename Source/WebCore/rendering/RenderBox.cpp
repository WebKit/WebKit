/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2005-2025 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2005-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2015-2019 Google Inc. All rights reserved.
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

#include "AnchorPositionEvaluator.h"
#include "BackgroundPainter.h"
#include "BorderPainter.h"
#include "BorderShape.h"
#include "ContainerNodeInlines.h"
#include "CSSFontSelector.h"
#include "Document.h"
#include "EditingInlines.h"
#include "EventHandler.h"
#include "FloatQuad.h"
#include "FloatRoundedRect.h"
#include "FontBaseline.h"
#include "GraphicsContext.h"
#include "HTMLBodyElement.h"
#include "HTMLButtonElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLegendElement.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "HitTestResult.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLineBox.h"
#include "InlineRunAndOffset.h"
#include "LayoutIntegrationLineLayout.h"
#include "LegacyRenderSVGResourceClipper.h"
#include "LocalFrame.h"
#include "LocalFrameViewInlines.h"
#include "Page.h"
#include "PaintInfo.h"
#include "PositionedLayoutConstraints.h"
#include "RenderBlockInlines.h"
#include "RenderBoxFragmentInfo.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderElementInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderFragmentContainer.h"
#include "RenderGeometryMap.h"
#include "RenderGrid.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerInlines.h"
#include "RenderLayerScrollableArea.h"
#include "RenderLayoutState.h"
#include "RenderListMarker.h"
#include "RenderMathMLBlock.h"
#include "RenderMultiColumnFlow.h"
#include "RenderObjectInlines.h"
#include "RenderSVGResourceClipper.h"
#include "RenderTableCellInlines.h"
#include "RenderTableCell.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "ResizeObserverSize.h"
#include "SVGClipPathElement.h"
#include "SVGElementTypeHelpers.h"
#include "ScrollAnimator.h"
#include "ScrollbarTheme.h"
#include "ScrollbarsController.h"
#include "Settings.h"
#include "StyleBoxShadow.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "TransformOperationData.h"
#include "TransformState.h"
#include <algorithm>
#include <math.h>
#include <wtf/Assertions.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderBox);

struct SameSizeAsRenderBox : public RenderBoxModelObject {
    virtual ~SameSizeAsRenderBox() = default;
    LayoutRect frameRect;
    LayoutBoxExtent marginBox;
    LayoutUnit preferredLogicalWidths[2];
    void* pointers[1];
};

static_assert(sizeof(RenderBox) == sizeof(SameSizeAsRenderBox), "RenderBox should stay small");

using namespace HTMLNames;

using OverrideSizeMap = SingleThreadWeakHashMap<const RenderBox, LayoutUnit>;
static OverrideSizeMap* gOverridingLogicalHeightMap = nullptr;
static OverrideSizeMap* gOverridingLogicalWidthMap = nullptr;

using OverridingPreferredSizeMap = SingleThreadWeakHashMap<const RenderBox, Style::PreferredSize>;
static OverridingPreferredSizeMap* gOverridingLogicalHeightMapForFlexBasisComputation = nullptr;
static OverridingPreferredSizeMap* gOverridingLogicalWidthMapForFlexBasisComputation = nullptr;

// FIXME: We should store these based on physical direction.
using OverrideOptionalSizeMap = SingleThreadWeakHashMap<const RenderBox, RenderBox::GridAreaSize>;
static OverrideOptionalSizeMap* gGridAreaContentLogicalHeightMap = nullptr;
static OverrideOptionalSizeMap* gGridAreaContentLogicalWidthMap = nullptr;

// Size of border belt for autoscroll. When mouse pointer in border belt,
// autoscroll is started.
static const int autoscrollBeltSize = 20;
static const unsigned backgroundObscurationTestMaxDepth = 4;

bool RenderBox::s_hadNonVisibleOverflow = false;

RenderBox::RenderBox(Type type, Element& element, RenderStyle&& style, OptionSet<TypeFlag> flags, TypeSpecificFlags typeSpecificFlags)
    : RenderBoxModelObject(type, element, WTFMove(style), flags | TypeFlag::IsBox, typeSpecificFlags)
{
    ASSERT(isRenderBox());
}

RenderBox::RenderBox(Type type, Document& document, RenderStyle&& style, OptionSet<TypeFlag> flags, TypeSpecificFlags typeSpecificFlags)
    : RenderBoxModelObject(type, document, WTFMove(style), flags | TypeFlag::IsBox, typeSpecificFlags)
{
    ASSERT(isRenderBox());
}

// Do not add any code in below destructor. Add it to willBeDestroyed() instead.
RenderBox::~RenderBox() = default;

void RenderBox::willBeDestroyed()
{
    if (frame().eventHandler().autoscrollRenderer() == this)
        frame().eventHandler().stopAutoscrollTimer(true);

    if (hasInitializedStyle()) {
        if (style().hasSnapPosition())
            view().unregisterBoxWithScrollSnapPositions(*this);
        if (style().containerType() != ContainerType::Normal)
            view().unregisterContainerQueryBox(*this);
        if (!style().anchorNames().isNone())
            view().unregisterAnchor(*this);
        if (!style().positionTryFallbacks().isNone())
            view().unregisterPositionTryBox(*this);
        if (Style::AnchorPositionEvaluator::isAnchorPositioned(style())) // Keep this in sync with styleDidChange().
            layoutContext().unregisterAnchorScrollAdjusterFor(*this);
        if (hasPotentiallyScrollableOverflow()) // Keep this in sync with AnchorScrollAdjuster::addSnapshot() calls.
            layoutContext().removeScrollerFromAnchorScrollAdjusters(*this);
    }

    RenderBoxModelObject::willBeDestroyed();
}

RenderFragmentContainer* RenderBox::clampToStartAndEndFragments(RenderFragmentContainer* fragment) const
{
    CheckedPtr fragmentedFlow = enclosingFragmentedFlow();

    ASSERT(isRenderView() || (fragment && fragmentedFlow));
    if (isRenderView())
        return fragment;

    // We need to clamp to the block, since we want any lines or blocks that overflow out of the
    // logical top or logical bottom of the block to size as though the border box in the first and
    // last fragments extended infinitely. Otherwise the lines are going to size according to the fragments
    // they overflow into, which makes no sense when this block doesn't exist in |fragment| at all.
    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (!fragmentedFlow->getFragmentRangeForBox(*this, startFragment, endFragment))
        return fragment;

    if (fragment->logicalTopForFragmentedFlowContent() < startFragment->logicalTopForFragmentedFlowContent())
        return startFragment;
    if (fragment->logicalTopForFragmentedFlowContent() > endFragment->logicalTopForFragmentedFlowContent())
        return endFragment;

    return fragment;
}

bool RenderBox::hasFragmentRangeInFragmentedFlow() const
{
    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow(); fragmentedFlow && fragmentedFlow->hasValidFragmentInfo())
        return fragmentedFlow->hasCachedFragmentRangeForBox(*this);

    return false;
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

void RenderBox::removeFloatingAndInvalidateForLayout()
{
    ASSERT(isFloating());

    if (renderTreeBeingDestroyed())
        return;

    if (auto* ancestor = outermostBlockContainingFloatingObject(*this)) {
        ancestor->markSiblingsWithFloatsForLayout(this);
        ancestor->markAllDescendantsWithFloatsForLayout(this, false);
    }
}

void RenderBox::removeFloatingOrOutOfFlowChildFromBlockLists()
{
    ASSERT(!renderTreeBeingDestroyed());

    if (isFloating())
        return removeFloatingAndInvalidateForLayout();

    if (isOutOfFlowPositioned())
        return RenderBlock::removeOutOfFlowBox(*this);

    ASSERT_NOT_REACHED();
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
            if (Style::hasEntirelyFixedBackground(oldStyle->backgroundLayers()) != Style::hasEntirelyFixedBackground(newStyle.backgroundLayers()))
                view().compositor().rootLayerConfigurationChanged();
        }
        
        // When a layout hint happens and an object's position style changes, we have to do a layout
        // to dirty the render tree using the old position value now.
        if (diff == StyleDifference::Layout && parent() && oldStyle->position() != newStyle.position()) {
            if (!oldStyle->hasOutOfFlowPosition() && newStyle.hasOutOfFlowPosition()) {
                // We are about to go out of flow. Before that takes place, we need to mark the
                // current containing block chain for preferred widths recalculation.
                setNeedsLayoutAndPreferredWidthsUpdate();
                if (CheckedPtr flexContainer = dynamicDowncast<RenderFlexibleBox>(parent())) {
                    flexContainer->clearCachedFlexItemIntrinsicContentLogicalHeight(*this);
                    flexContainer->clearCachedMainSizeForFlexItem(*this);
                }
                if (isInTopLayerOrBackdrop(style(), element())) {
                    // Since top layer's containing block is driven by the associated element's state (see Element::isInTopLayerOrBackdrop)
                    // and this state is set before styleWillChange call, dirtying ancestors starting from _this_ fails to mark the current ancestor chain properly.
                    parent()->setChildNeedsLayout();
                }
            } else
                scheduleLayout(markContainingBlocksForLayout());
            
            if (oldStyle->position() != PositionType::Static && newStyle.hasOutOfFlowPosition())
                parent()->setChildNeedsLayout();
            if (isFloating() && !isOutOfFlowPositioned() && newStyle.hasOutOfFlowPosition())
                removeFloatingOrOutOfFlowChildFromBlockLists();
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

    if (newStyle.containerType() != ContainerType::Normal)
        view().registerContainerQueryBox(*this);
    else if (oldStyle && oldStyle->containerType() != ContainerType::Normal)
        view().unregisterContainerQueryBox(*this);

    if (!newStyle.positionTryFallbacks().isNone() && newStyle.hasOutOfFlowPosition())
        view().registerPositionTryBox(*this);
    else if (oldStyle && !oldStyle->positionTryFallbacks().isNone() && oldStyle->hasOutOfFlowPosition())
        view().unregisterPositionTryBox(*this);

    if (oldStyle && Style::AnchorPositionEvaluator::isAnchorPositioned(newStyle) != Style::AnchorPositionEvaluator::isAnchorPositioned(*oldStyle))
        view().frameView().clearCachedHasAnchorPositionedViewportConstrainedObjects();

    RenderBoxModelObject::styleWillChange(diff, newStyle);
}

void RenderBox::invalidateAncestorBackgroundObscurationStatus()
{
    auto parentToInvalidate = parent();
    for (unsigned i = 0; i < backgroundObscurationTestMaxDepth && parentToInvalidate; ++i) {
        parentToInvalidate->invalidateBackgroundObscurationStatus();
        parentToInvalidate = parentToInvalidate->parent();
    }
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
        RenderBlock::removePercentHeightDescendant(*this);

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
    if (hasNonVisibleOverflow() && layer() && oldStyle && oldStyle->usedZoom() != newStyle.usedZoom()) {
        if (CheckedPtr scrollableArea = layer()->scrollableArea()) {
            ScrollPosition scrollPosition = scrollableArea->scrollPosition();
            float zoomScaleFactor = newStyle.usedZoom() / oldStyle->usedZoom();
            scrollPosition.scale(zoomScaleFactor);
            scrollableArea->setPostLayoutScrollPosition(scrollPosition);
        }
    }

    if (layer() && oldStyle && oldStyle->shouldPlaceVerticalScrollbarOnLeft() != newStyle.shouldPlaceVerticalScrollbarOnLeft()) {
        if (CheckedPtr scrollableArea = layer()->scrollableArea())
            scrollableArea->scrollbarsController().scrollbarLayoutDirectionChanged(shouldPlaceVerticalScrollbarOnLeft() ? UserInterfaceLayoutDirection::RTL : UserInterfaceLayoutDirection::LTR);
    }

    bool isDocElementRenderer = isDocumentElementRenderer();

    if (layer() && oldStyle && oldStyle->scrollbarWidth() != newStyle.scrollbarWidth()) {
        if (isDocElementRenderer)
            view().frameView().scrollbarWidthChanged(Style::toPlatform(newStyle.scrollbarWidth()));
        else if (CheckedPtr scrollableArea = layer()->scrollableArea())
            scrollableArea->scrollbarWidthChanged(Style::toPlatform(newStyle.scrollbarWidth()));
    }

    if (layer() && oldStyle && oldStyle->scrollbarColor() != newStyle.scrollbarColor()) {
        std::optional<ScrollbarColor> scrollbarColor;

        if (auto value = newStyle.scrollbarColor().tryValue()) {
            scrollbarColor = ScrollbarColor {
                .thumbColor = newStyle.colorResolvingCurrentColor(value->thumb),
                .trackColor = newStyle.colorResolvingCurrentColor(value->track)
            };
        }

        if (isDocElementRenderer)
            view().frameView().scrollbarColorDidChange(scrollbarColor);
        else if (auto* scrollableArea = layer()->scrollableArea())
            scrollableArea->scrollbarColorDidChange(scrollbarColor);
    }

#if ENABLE(DARK_MODE_CSS)
    if (layer() && oldStyle && oldStyle->colorScheme() != newStyle.colorScheme()) {
        if (CheckedPtr scrollableArea = layer()->scrollableArea())
            scrollableArea->invalidateScrollbars();
    }
#endif

    // Our opaqueness might have changed without triggering layout.
    if (diff >= StyleDifference::Repaint && diff <= StyleDifference::RepaintLayer)
        invalidateAncestorBackgroundObscurationStatus();

    bool isBodyRenderer = isBody();

    if (isDocElementRenderer || isBodyRenderer) {
        view().frameView().recalculateScrollbarOverlayStyle();
        
        if (diff != StyleDifference::Equal)
            view().compositor().rootOrBodyStyleChanged(*this, oldStyle);
    }

    if ((oldStyle && !oldStyle->shapeOutside().isNone()) || !style().shapeOutside().isNone())
        updateShapeOutsideInfoAfterStyleChange(style(), oldStyle);
    updateGridPositionAfterStyleChange(style(), oldStyle);

    // Changing the position from/to absolute can potentially create/remove flex/grid items, as absolutely positioned
    // children of a flex/grid box are out-of-flow, and thus, not flex/grid items. This means that we need to clear
    // any override content size set by our container, because it would likely be incorrect after the style change.
    if (isOutOfFlowPositioned() && parent() && parent()->style().isDisplayFlexibleBoxIncludingDeprecatedOrGridFormattingContextBox())
        clearOverridingSize();

    if (oldStyle && oldStyle->hasOutOfFlowPosition() != style().hasOutOfFlowPosition()) {
        clearGridAreaContentSize();
        if (auto* containingBlock = this->containingBlock(); containingBlock && oldStyle->hasOutOfFlowPosition()) {
            // When going from out-of-flow to inflow, the containing block gains new descendant content and its preferred width becomes invalid.
            containingBlock->setNeedsLayoutAndPreferredWidthsUpdate();
        }
    }
    if (oldStyle && Style::AnchorPositionEvaluator::isAnchorPositioned(*oldStyle)
        && !Style::AnchorPositionEvaluator::isAnchorPositioned(style())) {
        // Only out-of-flow code manages these, so if we're changing we need to clear them.
        // Keep this in sync with willBeDestroyed().
        layoutContext().unregisterAnchorScrollAdjusterFor(*this);
    }

    if ((layer() && diff == StyleDifference::Layout && hasNonVisibleOverflow())
        || (oldStyle && oldStyle->isOverflowVisible() != style().isOverflowVisible()))
        layoutContext().invalidateAnchorDependenciesForScroller(*this);
}

static bool hasEquivalentGridPositioningStyle(const RenderStyle& style, const RenderStyle& oldStyle)
{
    return oldStyle.gridItemColumnStart() == style.gridItemColumnStart()
        && oldStyle.gridItemColumnEnd() == style.gridItemColumnEnd()
        && oldStyle.gridItemRowStart() == style.gridItemRowStart()
        && oldStyle.gridItemRowEnd() == style.gridItemRowEnd()
        && oldStyle.order() == style.order()
        && oldStyle.hasOutOfFlowPosition() == style.hasOutOfFlowPosition()
        && (oldStyle.gridTemplateColumns().subgrid == style.gridTemplateColumns().subgrid || style.gridTemplateColumns().orderedNamedLines.map.isEmpty())
        && (oldStyle.gridTemplateRows().subgrid == style.gridTemplateRows().subgrid || style.gridTemplateRows().orderedNamedLines.map.isEmpty());
}

void RenderBox::updateGridPositionAfterStyleChange(const RenderStyle& style, const RenderStyle* oldStyle)
{
    if (!oldStyle)
        return;
    CheckedPtr parentGrid = dynamicDowncast<RenderGrid>(parent());
    if (!parentGrid)
        return;

    // Positioned items don't participate on the layout of the grid,
    // so we don't need to mark the grid as dirty if they change positions.
    if ((oldStyle->hasOutOfFlowPosition() && style.hasOutOfFlowPosition()) || hasEquivalentGridPositioningStyle(style, *oldStyle))
        return;

    // It should be possible to not dirty the grid in some cases (like moving an
    // explicitly placed grid item).
    // For now, it's more simple to just always recompute the grid.
    parentGrid->setNeedsItemPlacement();
}

void RenderBox::updateShapeOutsideInfoAfterStyleChange(const RenderStyle& style, const RenderStyle* oldStyle)
{
    Style::ShapeOutside shapeOutside = style.shapeOutside();
    Style::ShapeOutside oldShapeOutside = oldStyle ? oldStyle->shapeOutside() : RenderStyle::initialShapeOutside();

    Style::ShapeMargin shapeMargin = style.shapeMargin();
    Style::ShapeMargin oldShapeMargin = oldStyle ? oldStyle->shapeMargin() : RenderStyle::initialShapeMargin();

    Style::ShapeImageThreshold shapeImageThreshold = style.shapeImageThreshold();
    Style::ShapeImageThreshold oldShapeImageThreshold = oldStyle ? oldStyle->shapeImageThreshold() : RenderStyle::initialShapeImageThreshold();

    // FIXME: A future optimization would do a deep comparison for equality. (bug 100811)
    if (shapeOutside == oldShapeOutside && shapeMargin == oldShapeMargin && shapeImageThreshold == oldShapeImageThreshold)
        return;

    if (shapeOutside.isNone())
        removeShapeOutsideInfo();
    else
        ensureShapeOutsideInfo().markShapeAsDirty();

    if (!shapeOutside.isNone() || shapeOutside != oldShapeOutside)
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
            // (4) No containment is set either on the body or on the html document element.
            auto& documentElement = *document().documentElement();
            auto& documentElementRenderer = *documentElement.renderer();
            if (is<HTMLHtmlElement>(documentElement)
                && document().body() == element()
                && documentElementRenderer.effectiveOverflowX() == Overflow::Visible
                && styleToUse.usedContain().isNone()
                && documentElementRenderer.style().usedContain().isNone()) {
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
                issueRepaint(visualOverflowRect(), ClipRepaintToLayer::Yes, ForceRepaint::Yes);
                issueRepaint(layoutOverflowRect(), ClipRepaintToLayer::Yes, ForceRepaint::Yes);
            }
            setHasNonVisibleOverflow();
        }
    }
    setHasTransformRelatedProperty(computeHasTransformRelatedProperty(styleToUse));
    setHasReflection(!styleToUse.boxReflect().isNone());
}

bool RenderBox::computeHasTransformRelatedProperty(const RenderStyle& styleToUse) const
{
    if (styleToUse.hasTransformRelatedProperty())
        return true;

    if (!settings().css3DTransformBackfaceVisibilityInteroperabilityEnabled())
        return false;

    if (styleToUse.backfaceVisibility() != BackfaceVisibility::Hidden)
        return false;

    if (!element())
        return false;

    auto* parent = element()->parentElement();
    if (!parent)
        return false;

    auto* parentRenderer = parent->renderer();
    if (!parentRenderer)
        return false;

    return parentRenderer->style().preserves3D();
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

    LayoutStateMaintainer statePusher(*this, locationOffset(), writingMode().isBlockFlipped());
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
    if (writingMode().isLogicalLeftInlineStart()) {
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
    CheckedPtr scrollableArea = layer() ? layer()->scrollableArea() : nullptr;
    return (hasNonVisibleOverflow() && scrollableArea) ? scrollableArea->scrollPosition().x() : 0;
}

int RenderBox::scrollTop() const
{
    CheckedPtr scrollableArea = layer() ? layer()->scrollableArea() : nullptr;
    return (hasNonVisibleOverflow() && scrollableArea) ? scrollableArea->scrollPosition().y() : 0;
}

bool RenderBox::shouldResetLogicalHeightBeforeLayout() const
{
    auto* flexBoxParent = dynamicDowncast<RenderFlexibleBox>(parent());
    return flexBoxParent && flexBoxParent->shouldResetFlexItemLogicalHeightBeforeLayout();
}

void RenderBox::resetLogicalHeightBeforeLayoutIfNeeded()
{
    if (shouldResetLogicalHeightBeforeLayout())
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
    CheckedPtr scrollableArea = layer()->scrollableArea();
    ASSERT(scrollableArea);
    setupWheelEventMonitor(*scrollableArea);
    scrollableArea->scrollToXPosition(newLeft, options);
}

void RenderBox::setScrollTop(int newTop, const ScrollPositionChangeOptions& options)
{
    if (!hasPotentiallyScrollableOverflow() || !layer())
        return;
    CheckedPtr scrollableArea = layer()->scrollableArea();
    ASSERT(scrollableArea);
    setupWheelEventMonitor(*scrollableArea);
    scrollableArea->scrollToYPosition(newTop, options);
}

void RenderBox::setScrollPosition(const ScrollPosition& position, const ScrollPositionChangeOptions& options)
{
    if (!hasPotentiallyScrollableOverflow() || !layer())
        return;
    CheckedPtr scrollableArea = layer()->scrollableArea();
    ASSERT(scrollableArea);
    setupWheelEventMonitor(*scrollableArea);
    scrollableArea->setScrollPosition(position, options);
}

void RenderBox::boundingRects(Vector<LayoutRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    rects.append({ accumulatedOffset, size() });
}

void RenderBox::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow(); fragmentedFlow && fragmentedFlow->absoluteQuadsForBox(quads, wasFixed, *this))
        return;

    auto localRect = FloatRect { 0, 0, width(), height() };
    quads.append(localToAbsoluteQuad(localRect, UseTransforms, wasFixed));
}

void RenderBox::applyTransform(TransformationMatrix& t, const RenderStyle& style, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    style.applyTransform(t, TransformOperationData(boundingBox, this), options);
}

void RenderBox::constrainLogicalMinMaxSizesByAspectRatio(LayoutUnit& computedMinSize, LayoutUnit& computedMaxSize, LayoutUnit computedSize, MinimumSizeIsAutomaticContentBased minimumSizeType, ConstrainDimension dimension) const
{
    // TODO: Here we use isSpecified() to present the definite value. This is not quite correct, for the definite value should also include
    // a size of the initial containing block and the “stretch-fit” sizing of non-replaced blocks if they have definite values.
    // See https://www.w3.org/TR/css-sizing-3/#definite
    const RenderStyle& styleToUse = style();
    ASSERT(styleToUse.hasAspectRatio() || isRenderReplacedWithIntrinsicRatio());
    auto logicalSize = dimension == ConstrainDimension::Width ? styleToUse.logicalWidth() : styleToUse.logicalHeight();
    // https://www.w3.org/TR/css-sizing-4/#aspect-ratio-minimum
    if (minimumSizeType == MinimumSizeIsAutomaticContentBased::Yes) {
        // Only use Automatic Content-based Minimum Sizes in the ratio-dependent axis.
        if (logicalSize.isSpecified())
            computedMinSize = std::min(computedMinSize, computedSize);
        computedMinSize = std::min(computedMinSize, computedMaxSize);
    }

    if (logicalSize.isSpecified())
        return;

    // Sizing constraints in either axis (the origin axis) should be transferred through the preferred aspect ratio. See https://www.w3.org/TR/css-sizing-4/#aspect-ratio-size-transfers
    bool shouldCheckTransferredMinSize = dimension == ConstrainDimension::Width ? !styleToUse.logicalMinWidth().isSpecified() : !styleToUse.logicalMinHeight().isSpecified();
    bool shouldCheckTransferredMaxSize = dimension == ConstrainDimension::Width ? !styleToUse.logicalMaxWidth().isSpecified() : !styleToUse.logicalMaxHeight().isSpecified();
    if (!shouldCheckTransferredMaxSize && !shouldCheckTransferredMinSize)
        return;

    auto [transferredLogicalMinSize, transferredLogicalMaxSize] = dimension == ConstrainDimension::Width ? computeMinMaxLogicalWidthFromAspectRatio() : computeMinMaxLogicalHeightFromAspectRatio();
    if (shouldCheckTransferredMaxSize && transferredLogicalMaxSize != LayoutUnit::max()) {
        // The transferred max size should be floored by the definite minimum size.
        if (!shouldCheckTransferredMinSize && minimumSizeType == MinimumSizeIsAutomaticContentBased::No)
            transferredLogicalMaxSize = std::max(transferredLogicalMaxSize, computedMinSize);
        computedMaxSize = std::min(computedMaxSize, transferredLogicalMaxSize);
        if (minimumSizeType == MinimumSizeIsAutomaticContentBased::Yes)
            computedMinSize = std::min(computedMinSize, computedMaxSize);
    }

    if (shouldCheckTransferredMinSize && transferredLogicalMinSize > LayoutUnit()) {
        // The transferred min size should be capped by the definite maximum size.
        if (!shouldCheckTransferredMaxSize)
            transferredLogicalMinSize = std::min(transferredLogicalMinSize, computedMaxSize);
        computedMinSize = std::max(computedMinSize, transferredLogicalMinSize);
    }
}

LayoutUnit RenderBox::constrainLogicalWidthByMinMax(LayoutUnit logicalWidth, LayoutUnit availableWidth, const RenderBlock& cb, AllowIntrinsic allowIntrinsic) const
{
    auto& styleToUse = style();
    auto computedMaxWidth = LayoutUnit::max();
    if (!styleToUse.logicalMaxWidth().isNone() && (allowIntrinsic == AllowIntrinsic::Yes || !styleToUse.logicalMaxWidth().isIntrinsic()))
        computedMaxWidth = computeLogicalWidthUsing(styleToUse.logicalMaxWidth(), availableWidth, cb);

    if (allowIntrinsic == AllowIntrinsic::No && styleToUse.logicalMinWidth().isIntrinsic())
        return std::min(logicalWidth, computedMaxWidth);

    auto logicalMinWidth = styleToUse.logicalMinWidth();
    auto minimumSizeType = MinimumSizeIsAutomaticContentBased::No;
    LayoutUnit computedMinWidth;
    if (logicalMinWidth.isAuto() && shouldComputeLogicalWidthFromAspectRatio() && (styleToUse.logicalWidth().isAuto() || styleToUse.logicalWidth().isMinContent() || styleToUse.logicalWidth().isMaxContent()) && !is<RenderReplaced>(*this) && effectiveOverflowInlineDirection() == Overflow::Visible) {
        // The automatic minimum size in the ratio-dependent axis is  its min-content size. See https://www.w3.org/TR/css-sizing-4/#aspect-ratio-minimum
        logicalMinWidth = CSS::Keyword::MinContent { };
        minimumSizeType = MinimumSizeIsAutomaticContentBased::Yes;
    }
    computedMinWidth = computeLogicalWidthUsing(logicalMinWidth, availableWidth, cb);

    if (styleToUse.hasAspectRatio())
        constrainLogicalMinMaxSizesByAspectRatio(computedMinWidth, computedMaxWidth, logicalWidth, minimumSizeType, ConstrainDimension::Width);

    logicalWidth = std::min(logicalWidth, computedMaxWidth);
    return std::max(logicalWidth, computedMinWidth);
}

LayoutUnit RenderBox::constrainLogicalHeightByMinMax(LayoutUnit logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    auto& styleToUse = style();
    std::optional<LayoutUnit> computedLogicalMaxHeight;
    if (!styleToUse.logicalMaxHeight().isNone())
        computedLogicalMaxHeight = computeLogicalHeightUsing(styleToUse.logicalMaxHeight(), intrinsicContentHeight);

    auto logicalMinHeight = styleToUse.logicalMinHeight();
    auto minimumSizeType = MinimumSizeIsAutomaticContentBased::No;
    if (logicalMinHeight.isAuto() && shouldComputeLogicalHeightFromAspectRatio() && intrinsicContentHeight && !is<RenderReplaced>(*this) && effectiveOverflowBlockDirection() == Overflow::Visible) {
        auto heightFromAspectRatio = blockSizeFromAspectRatio(borderAndPaddingLogicalWidth(), borderAndPaddingLogicalHeight(), style().logicalAspectRatio(), style().boxSizingForAspectRatio(), logicalWidth(), style().aspectRatio(), isRenderReplaced()) - borderAndPaddingLogicalHeight();
        if (firstChild())
            heightFromAspectRatio = std::max(heightFromAspectRatio, *intrinsicContentHeight);
        logicalMinHeight = Style::MinimumSize::Fixed { heightFromAspectRatio };
        minimumSizeType = MinimumSizeIsAutomaticContentBased::Yes;
    }
    if (logicalMinHeight.isMinContent() || logicalMinHeight.isMaxContent())
        logicalMinHeight = CSS::Keyword::Auto { };
    auto computedLogicalMinHeight = computeLogicalHeightUsing(logicalMinHeight, intrinsicContentHeight);
    auto maxHeight = computedLogicalMaxHeight.value_or(LayoutUnit::max());
    auto minHeight = computedLogicalMinHeight.value_or(LayoutUnit());
    if (styleToUse.hasAspectRatio())
        constrainLogicalMinMaxSizesByAspectRatio(minHeight, maxHeight, logicalHeight, minimumSizeType, ConstrainDimension::Height);
    logicalHeight = std::min(logicalHeight, maxHeight);
    return std::max(logicalHeight, minHeight);
}

LayoutUnit RenderBox::constrainContentBoxLogicalHeightByMinMax(LayoutUnit logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    // If the min/max height and logical height are both percentages we take advantage of already knowing the current resolved percentage height
    // to avoid recursing up through our containing blocks again to determine it.
    auto& styleToUse = style();

    auto percentageLogicalHeight = styleToUse.logicalHeight().tryPercentage();

    auto& logicalMaxHeight = styleToUse.logicalMaxHeight();
    if (!logicalMaxHeight.isNone()) {
        if (auto percentageLogicalMaxHeight = logicalMaxHeight.tryPercentage(); percentageLogicalMaxHeight && percentageLogicalHeight) {
            auto availableLogicalHeight = logicalHeight / percentageLogicalHeight->value * 100;
            logicalHeight = std::min(logicalHeight, Style::evaluate<LayoutUnit>(*percentageLogicalMaxHeight, LayoutUnit(availableLogicalHeight)));
        } else {
            if (auto computedContentLogicalMaxHeight = computeContentLogicalHeight(logicalMaxHeight, intrinsicContentHeight))
                logicalHeight = std::min(logicalHeight, *computedContentLogicalMaxHeight);
        }
    }

    auto& logicalMinHeight = styleToUse.logicalMinHeight();
    if (auto percentageLogicalMinHeight = logicalMinHeight.tryPercentage(); percentageLogicalMinHeight && percentageLogicalHeight) {
        auto availableLogicalHeight = logicalHeight / percentageLogicalHeight->value * 100;
        logicalHeight = std::max(logicalHeight, Style::evaluate<LayoutUnit>(*percentageLogicalMinHeight, LayoutUnit(availableLogicalHeight)));
    } else {
        if (auto computedContentLogicalMinHeight = computeContentLogicalHeight(logicalMinHeight, intrinsicContentHeight))
            logicalHeight = std::max(logicalHeight, *computedContentLogicalMinHeight);
    }

    return logicalHeight;
}

// FIXME: Despite the name, this returns rounded borders based on the padding box, which seems wrong.
LayoutRoundedRect::Radii RenderBox::borderRadii() const
{
    auto borderShape = BorderShape::shapeForBorderRect(style(), paddingBoxRectIncludingScrollbar());
    return borderShape.deprecatedRoundedRect().radii();
}

LayoutRect RenderBox::paddingBoxRect() const
{
    auto offsetForScrollbar = 0_lu;
    auto verticalScrollbarWidth = 0_lu;
    auto horizontalScrollbarHeight = 0_lu;
    if (hasNonVisibleOverflow()) {
        verticalScrollbarWidth = this->verticalScrollbarWidth();
        offsetForScrollbar = shouldPlaceVerticalScrollbarOnLeft() ? verticalScrollbarWidth : 0_lu;
        horizontalScrollbarHeight = this->horizontalScrollbarHeight();
    }

    auto borderWidths = this->borderWidths();
    return LayoutRect {
        borderWidths.left() + offsetForScrollbar,
        borderWidths.top(),
        width() - borderWidths.left() - borderWidths.right() - verticalScrollbarWidth,
        height() - borderWidths.top() - borderWidths.bottom() - horizontalScrollbarHeight
    };
}

LayoutPoint RenderBox::contentBoxLocation() const
{
    LayoutUnit verticalScrollbarSpace = (shouldPlaceVerticalScrollbarOnLeft() || style().scrollbarGutter().isStableBothEdges()) ? verticalScrollbarWidth() : 0;
    LayoutUnit horizontalScrollbarSpace = style().scrollbarGutter().isStableBothEdges() ? horizontalScrollbarHeight() : 0;
    return { borderLeft() + paddingLeft() + verticalScrollbarSpace, borderTop() + paddingTop() + horizontalScrollbarSpace };
}

FloatRect RenderBox::referenceBoxRect(CSSBoxType boxType) const
{
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

    ASSERT_NOT_REACHED();
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

LayoutRect RenderBox::localOutlineBoundsRepaintRect() const
{
    auto box = borderBoundingBox();
    return applyVisualEffectOverflow(box);
}

LayoutRect RenderBox::outlineBoundsForRepaint(const RenderLayerModelObject* repaintContainer, const RenderGeometryMap* geometryMap) const
{
    auto box = localOutlineBoundsRepaintRect();

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

int RenderBox::reflectionOffset() const
{
    auto reflection = style().boxReflect().tryReflection();
    if (!reflection)
        return 0;
    if (reflection->direction == ReflectionDirection::Left || reflection->direction == ReflectionDirection::Right)
        return Style::evaluate<int>(reflection->offset, borderBoxRect().width(), Style::ZoomNeeded { });
    return Style::evaluate<int>(reflection->offset, borderBoxRect().height(), Style::ZoomNeeded { });
}

LayoutRect RenderBox::reflectedRect(const LayoutRect& r) const
{
    auto reflection = style().boxReflect().tryReflection();
    if (!reflection)
        return LayoutRect();

    LayoutRect box = borderBoxRect();
    LayoutRect result = r;
    switch (reflection->direction) {
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

bool RenderBox::fixedElementLaysOutRelativeToFrame(const LocalFrameView& frameView) const
{
    return isFixedPositioned() && container()->isRenderView() && frameView.fixedElementsLayoutRelativeToFrame();
}

bool RenderBox::includeVerticalScrollbarSize() const
{
    return hasNonVisibleOverflow() && layer() && !layer()->hasOverlayScrollbars()
        && (style().overflowY() == Overflow::Scroll || style().overflowY() == Overflow::Auto
            || (style().overflowY() == Overflow::Hidden && !style().scrollbarGutter().isAuto()));
}

bool RenderBox::includeHorizontalScrollbarSize() const
{
    return hasNonVisibleOverflow() && layer() && !layer()->hasOverlayScrollbars()
        && (style().overflowX() == Overflow::Scroll || style().overflowX() == Overflow::Auto
            || (style().overflowX() == Overflow::Hidden && !style().scrollbarGutter().isAuto()));
}

int RenderBox::verticalScrollbarWidth() const
{
    CheckedPtr scrollableArea = layer() ? layer()->scrollableArea() : nullptr;
    if (!scrollableArea)
        return 0;
    return includeVerticalScrollbarSize() ? scrollableArea->verticalScrollbarWidth(OverlayScrollbarSizeRelevancy::IgnoreOverlayScrollbarSize, isHorizontalWritingMode()) : 0;
}

int RenderBox::horizontalScrollbarHeight() const
{
    CheckedPtr scrollableArea = layer() ? layer()->scrollableArea() : nullptr;
    if (!scrollableArea)
        return 0;
    return includeHorizontalScrollbarSize() ? scrollableArea->horizontalScrollbarHeight(OverlayScrollbarSizeRelevancy::IgnoreOverlayScrollbarSize, isHorizontalWritingMode()) : 0;
}

int RenderBox::intrinsicScrollbarLogicalWidthIncludingGutter() const
{
    if (!hasNonVisibleOverflow())
        return 0;

    auto shouldIncludeScrollbarGutter = [](Style::ScrollbarGutter gutter, bool hasVisibleOverflow, Overflow overflow) {
        return (overflow == Overflow::Auto && (!gutter.isAuto() || hasVisibleOverflow)) || (overflow == Overflow::Hidden && !gutter.isAuto());
    };

    if (isHorizontalWritingMode() && ((style().overflowY() == Overflow::Scroll || shouldIncludeScrollbarGutter(style().scrollbarGutter(), hasScrollableOverflowY(), style().overflowY())) && !canUseOverlayScrollbars()))
        return style().scrollbarGutter().isStableBothEdges() ? verticalScrollbarWidth() * 2 : verticalScrollbarWidth();

    if (!isHorizontalWritingMode() && ((style().overflowX() == Overflow::Scroll || shouldIncludeScrollbarGutter(style().scrollbarGutter(), hasScrollableOverflowX(), style().overflowX())) && !canUseOverlayScrollbars()))
        return style().scrollbarGutter().isStableBothEdges() ? horizontalScrollbarHeight() * 2 : horizontalScrollbarHeight();

    return 0;
}

bool RenderBox::scrollLayer(ScrollDirection direction, ScrollGranularity granularity, unsigned stepCount, Element** stopElement)
{
    CheckedPtr scrollableArea = layer() ? layer()->scrollableArea() : nullptr;
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
    
    if (CheckedPtr scrollableArea = layer() ? layer()->scrollableArea() : nullptr) {
#if PLATFORM(COCOA)
        // On Mac only we reset the inline direction position when doing a document scroll (e.g., hitting Home/End).
        if (granularity == ScrollGranularity::Document)
            scrolled = scrollableArea->scroll(logicalToPhysical(ScrollInlineDirectionBackward, isHorizontalWritingMode(), writingMode().isBlockFlipped()), ScrollGranularity::Document, stepCount);
#endif
        if (scrollableArea->scroll(logicalToPhysical(direction, isHorizontalWritingMode(), writingMode().isBlockFlipped()), granularity, stepCount))
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

    if (style().resize() != Style::Resize::None)
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
    while (renderer) {
        if (auto* box = dynamicDowncast<RenderBox>(*renderer); box && box->canAutoscroll())
            break;
        if (is<RenderView>(*renderer) && renderer->document().ownerElement())
            renderer = renderer->document().ownerElement()->renderer();
        else
            renderer = renderer->parent();
    }

    return dynamicDowncast<RenderBox>(renderer);
}

void RenderBox::panScroll(const IntPoint& source)
{
    if (CheckedPtr scrollableArea = layer() ? layer()->scrollableArea() : nullptr)
        scrollableArea->panScrollFromPoint(source);
}

bool RenderBox::canUseOverlayScrollbars() const
{
    return !style().usesLegacyScrollbarStyle() && ScrollbarTheme::theme().usesOverlayScrollbars();
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

bool RenderBox::shouldInvalidatePreferredWidths() const
{
    return style().paddingStart().isPercentOrCalculated() || style().paddingEnd().isPercentOrCalculated() || (style().hasAspectRatio() && (hasRelativeLogicalHeight() || (isFlexItem() && hasStretchedLogicalHeight())));
}

ScrollPosition RenderBox::scrollPosition() const
{
    if (!hasPotentiallyScrollableOverflow())
        return { 0, 0 };

    ASSERT(hasLayer());
    CheckedPtr scrollableArea = layer()->scrollableArea();
    if (!scrollableArea)
        return { 0, 0 };

    return scrollableArea->scrollPosition();
}

ScrollPosition RenderBox::constrainedScrollPosition() const
{
    if (!hasPotentiallyScrollableOverflow())
        return { 0, 0 };

    ASSERT(hasLayer());
    auto* scrollableArea = layer()->scrollableArea();
    if (!scrollableArea)
        return { 0, 0 };

    return scrollableArea->constrainedScrollPosition(scrollableArea->scrollPosition());
}

LayoutSize RenderBox::cachedSizeForOverflowClip() const
{
    ASSERT(hasNonVisibleOverflow());
    ASSERT(hasLayer());
    return layer()->size();
}

bool RenderBox::applyCachedClipAndScrollPosition(RepaintRects& rects, const RenderLayerModelObject* container, VisibleRectContext context) const
{
    flipForWritingMode(rects);

    if (context.options.contains(VisibleRectContext::Option::ApplyCompositedContainerScrolls) || this != container || !usesCompositedScrolling())
        rects.moveBy(-scrollPosition()); // For overflow:auto/scroll/hidden.

    // Do not clip scroll layer contents to reduce the number of repaints while scrolling.
    if ((!context.options.contains(VisibleRectContext::Option::ApplyCompositedClips) && usesCompositedScrolling())
        || (!context.options.contains(VisibleRectContext::Option::ApplyContainerClip) && this == container)) {
        flipForWritingMode(rects);
        return true;
    }

    // height() is inaccurate if we're in the middle of a layout of this RenderBox, so use the
    // layer's size instead. Even if the layer's size is wrong, the layer itself will repaint
    // anyway if its size does change.
    LayoutRect clipRect(LayoutPoint(), cachedSizeForOverflowClip());
    if (effectiveOverflowX() == Overflow::Visible)
        clipRect.expandToInfiniteX();
    if (effectiveOverflowY() == Overflow::Visible)
        clipRect.expandToInfiniteY();

    if (context.scrollMargin && (isScrollContainerX() || isScrollContainerY())) {
        auto borderWidths = this->borderWidths();
        clipRect.contract(borderWidths);
        auto scrollMarginEdges = LayoutBoxExtent {
            Style::evaluate<LayoutUnit>(context.scrollMargin->top(), clipRect.height(), Style::ZoomNeeded { }),
            Style::evaluate<LayoutUnit>(context.scrollMargin->right(), clipRect.width(), Style::ZoomNeeded { }),
            Style::evaluate<LayoutUnit>(context.scrollMargin->bottom(), clipRect.height(), Style::ZoomNeeded { }),
            Style::evaluate<LayoutUnit>(context.scrollMargin->left(), clipRect.width(), Style::ZoomNeeded { })
        };

        clipRect.expand(scrollMarginEdges);
    }

    bool intersects;
    if (context.options.contains(VisibleRectContext::Option::UseEdgeInclusiveIntersection))
        intersects = rects.edgeInclusiveIntersect(clipRect);
    else
        intersects = rects.intersect(clipRect);

    flipForWritingMode(rects);
    return intersects;
}

LayoutUnit RenderBox::minPreferredLogicalWidth() const
{
    if (needsPreferredLogicalWidthsUpdate()) {
        SetLayoutNeededForbiddenScope layoutForbiddenScope(*this);
        const_cast<RenderBox&>(*this).computePreferredLogicalWidths();
    }
    return m_minPreferredLogicalWidth;
}

LayoutUnit RenderBox::maxPreferredLogicalWidth() const
{
    if (needsPreferredLogicalWidthsUpdate()) {
        SetLayoutNeededForbiddenScope layoutForbiddenScope(*this);
        const_cast<RenderBox&>(*this).computePreferredLogicalWidths();
    }
    return m_maxPreferredLogicalWidth;
}

void RenderBox::setOverridingBorderBoxLogicalHeight(LayoutUnit height)
{
    if (!gOverridingLogicalHeightMap)
        gOverridingLogicalHeightMap = new OverrideSizeMap();
    gOverridingLogicalHeightMap->set(*this, height);
}

void RenderBox::setOverridingBorderBoxLogicalWidth(LayoutUnit width)
{
    if (!gOverridingLogicalWidthMap)
        gOverridingLogicalWidthMap = new OverrideSizeMap();
    gOverridingLogicalWidthMap->set(*this, width);
}

void RenderBox::clearOverridingBorderBoxLogicalHeight()
{
    if (gOverridingLogicalHeightMap)
        gOverridingLogicalHeightMap->remove(*this);
}

void RenderBox::clearOverridingBorderBoxLogicalWidth()
{
    if (gOverridingLogicalWidthMap)
        gOverridingLogicalWidthMap->remove(*this);
}

void RenderBox::clearOverridingSize()
{
    clearOverridingBorderBoxLogicalHeight();
    clearOverridingBorderBoxLogicalWidth();
}

std::optional<LayoutUnit> RenderBox::overridingBorderBoxLogicalWidth() const
{
    if (!gOverridingLogicalWidthMap)
        return { };
    if (auto result = gOverridingLogicalWidthMap->find(*this); result != gOverridingLogicalWidthMap->end())
        return result->value;
    return { };
}

std::optional<LayoutUnit> RenderBox::overridingBorderBoxLogicalHeight() const
{
    if (!gOverridingLogicalHeightMap)
        return { };
    if (auto result = gOverridingLogicalHeightMap->find(*this); result != gOverridingLogicalHeightMap->end())
        return result->value;
    return { };
}

std::optional<RenderBox::GridAreaSize> RenderBox::gridAreaContentWidth(WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return gridAreaContentLogicalWidth();
    return gridAreaContentLogicalHeight();
}

std::optional<RenderBox::GridAreaSize> RenderBox::gridAreaContentHeight(WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return gridAreaContentLogicalHeight();
    return gridAreaContentLogicalWidth();
}

std::optional<RenderBox::GridAreaSize> RenderBox::gridAreaContentLogicalWidth() const
{
    if (!gGridAreaContentLogicalWidthMap)
        return { };
    if (auto result = gGridAreaContentLogicalWidthMap->find(*this); result != gGridAreaContentLogicalWidthMap->end())
        return result->value;
    return { };
}

std::optional<RenderBox::GridAreaSize> RenderBox::gridAreaContentLogicalHeight() const
{
    if (!gGridAreaContentLogicalHeightMap)
        return { };
    if (auto result = gGridAreaContentLogicalHeightMap->find(*this); result != gGridAreaContentLogicalHeightMap->end())
        return result->value;
    return { };
}

void RenderBox::setGridAreaContentLogicalWidth(GridAreaSize logicalWidth)
{
    if (!gGridAreaContentLogicalWidthMap)
        gGridAreaContentLogicalWidthMap = new OverrideOptionalSizeMap;
    gGridAreaContentLogicalWidthMap->set(*this, logicalWidth);
}

void RenderBox::setGridAreaContentLogicalHeight(GridAreaSize logicalHeight)
{
    if (!gGridAreaContentLogicalHeightMap)
        gGridAreaContentLogicalHeightMap = new OverrideOptionalSizeMap;
    gGridAreaContentLogicalHeightMap->set(*this, logicalHeight);
}

void RenderBox::clearGridAreaContentSize()
{
    if (gGridAreaContentLogicalWidthMap)
        gGridAreaContentLogicalWidthMap->remove(*this);
    clearGridAreaContentLogicalHeight();
}

void RenderBox::clearGridAreaContentLogicalHeight()
{
    if (gGridAreaContentLogicalHeightMap)
        gGridAreaContentLogicalHeightMap->remove(*this);
}

std::optional<Style::PreferredSize> RenderBox::overridingLogicalHeightForFlexBasisComputation() const
{
    if (!gOverridingLogicalHeightMapForFlexBasisComputation)
        return { };
    if (auto result = gOverridingLogicalHeightMapForFlexBasisComputation->find(*this); result != gOverridingLogicalHeightMapForFlexBasisComputation->end())
        return result->value;
    return { };
}

void RenderBox::setOverridingBorderBoxLogicalHeightForFlexBasisComputation(const Style::PreferredSize& logicalHeight)
{
    if (!gOverridingLogicalHeightMapForFlexBasisComputation)
        gOverridingLogicalHeightMapForFlexBasisComputation = new OverridingPreferredSizeMap();
    gOverridingLogicalHeightMapForFlexBasisComputation->set(*this, logicalHeight);
}

void RenderBox::clearOverridingLogicalHeightForFlexBasisComputation()
{
    if (gOverridingLogicalHeightMapForFlexBasisComputation)
        gOverridingLogicalHeightMapForFlexBasisComputation->remove(*this);
}

std::optional<Style::PreferredSize> RenderBox::overridingLogicalWidthForFlexBasisComputation() const
{
    if (!gOverridingLogicalWidthMapForFlexBasisComputation)
        return { };
    if (auto result = gOverridingLogicalWidthMapForFlexBasisComputation->find(*this); result != gOverridingLogicalWidthMapForFlexBasisComputation->end())
        return result->value;
    return { };
}

void RenderBox::setOverridingBorderBoxLogicalWidthForFlexBasisComputation(const Style::PreferredSize& logicalWidth)
{
    if (!gOverridingLogicalWidthMapForFlexBasisComputation)
        gOverridingLogicalWidthMapForFlexBasisComputation = new OverridingPreferredSizeMap();
    gOverridingLogicalWidthMapForFlexBasisComputation->set(*this, logicalWidth);
}

void RenderBox::clearOverridingLogicalWidthForFlexBasisComputation()
{
    if (gOverridingLogicalWidthMapForFlexBasisComputation)
        gOverridingLogicalWidthMapForFlexBasisComputation->remove(*this);
}

void RenderBox::markMarginAsTrimmed(Style::MarginTrimSide newTrimmedMargin)
{
    auto& rareData = ensureRareData();
    rareData.trimmedMargins = rareData.trimmedMargins | newTrimmedMargin;
}

void RenderBox::clearTrimmedMarginsMarkings()
{
    ASSERT(hasRareData());
    ensureRareData().trimmedMargins = { };
}

bool RenderBox::hasTrimmedMargin(std::optional<Style::MarginTrimSide> marginTrimType) const
{
    if (!isInFlow())
        return false;
    if (!hasRareData())
        return false;
    return marginTrimType ? rareData().trimmedMargins.contains(*marginTrimType) : !rareData().trimmedMargins.isEmpty();
}

LayoutUnit RenderBox::adjustBorderBoxLogicalWidthForBoxSizing(const Style::Length<CSS::Nonnegative, float>& logicalWidth) const
{
    auto width = LayoutUnit { logicalWidth.resolveZoom(Style::ZoomNeeded { }) };
    auto bordersPlusPadding = borderAndPaddingLogicalWidth();
    if (style().boxSizing() == BoxSizing::ContentBox)
        return width + bordersPlusPadding;
    return std::max(width, bordersPlusPadding);
}


LayoutUnit RenderBox::adjustBorderBoxLogicalWidthForBoxSizing(const Style::Length<CSS::NonnegativeUnzoomed, float>& logicalWidth) const
{
    auto width = LayoutUnit { logicalWidth.resolveZoom(style().usedZoomForLength()) };
    auto bordersPlusPadding = borderAndPaddingLogicalWidth();
    if (style().boxSizing() == BoxSizing::ContentBox)
        return width + bordersPlusPadding;
    return std::max(width, bordersPlusPadding);
}

LayoutUnit RenderBox::adjustBorderBoxLogicalWidthForBoxSizing(LayoutUnit computedLogicalWidth) const
{
    auto bordersAndPadding = borderAndPaddingLogicalWidth();
    if (style().boxSizing() == BoxSizing::ContentBox)
        return computedLogicalWidth + bordersAndPadding;
    return std::max(computedLogicalWidth, bordersAndPadding);
}

LayoutUnit RenderBox::adjustBorderBoxLogicalHeightForBoxSizing(LayoutUnit height) const
{
    LayoutUnit bordersPlusPadding = borderAndPaddingLogicalHeight();
    if (style().boxSizing() == BoxSizing::ContentBox)
        return height + bordersPlusPadding;
    return std::max(height, bordersPlusPadding);
}

LayoutUnit RenderBox::adjustContentBoxLogicalWidthForBoxSizing(const Style::Length<CSS::Nonnegative, float>& logicalWidth) const
{
    auto width = LayoutUnit { logicalWidth.resolveZoom(Style::ZoomNeeded { }) };
    if (style().boxSizing() == BoxSizing::ContentBox)
        return std::max(0_lu, width);
    return std::max(0_lu, width - borderAndPaddingLogicalWidth());
}

LayoutUnit RenderBox::adjustContentBoxLogicalWidthForBoxSizing(const Style::Length<CSS::NonnegativeUnzoomed, float>& logicalWidth) const
{
    auto width = LayoutUnit { logicalWidth.resolveZoom(style().usedZoomForLength()) };
    if (style().boxSizing() == BoxSizing::ContentBox)
        return std::max(0_lu, width);
    return std::max(0_lu, width - borderAndPaddingLogicalWidth());
}

LayoutUnit RenderBox::adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit computedLogicalWidth) const
{
    if (style().boxSizing() == BoxSizing::ContentBox)
        return std::max(0_lu, computedLogicalWidth);
    return std::max(0_lu, computedLogicalWidth - borderAndPaddingLogicalWidth());
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

LayoutUnit RenderBox::adjustIntrinsicLogicalHeightForBoxSizing(LayoutUnit height) const
{
    if (style().boxSizing() == BoxSizing::BorderBox)
        return height + borderAndPaddingLogicalHeight();
    return height;
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
    auto offsetFromHitTestRoot = toLayoutSize(accumulatedOffset + location());
    auto hitTestLocationInLocalCoordinates = hitTestLocation.point() - offsetFromHitTestRoot;

    auto hitsClipContent = [&](Element& element) -> bool {
        if (CheckedPtr clipper = dynamicDowncast<RenderSVGResourceClipper>(element.renderer()))
            return clipper->hitTestClipContent( FloatRect { borderBoxRect() }, hitTestLocationInLocalCoordinates);
        CheckedRef clipper = downcast<LegacyRenderSVGResourceClipper>(*element.renderer());
        return clipper->hitTestClipContent( FloatRect { borderBoxRect() }, FloatPoint { hitTestLocationInLocalCoordinates });
    };

    return WTF::switchOn(style().clipPath(),
        [&](const Style::BasicShapePath& clipPath) {
            auto& shape = clipPath.shape();
            auto path = Style::path(shape, referenceBoxRect(clipPath.referenceBox()));
            return path.contains(hitTestLocationInLocalCoordinates, Style::windRule(shape));
        },
        [&](const Style::ReferencePath& clipPath) {
            RefPtr element = document().getElementById(clipPath.fragment());
            return !is<SVGClipPathElement>(element) || !element->renderer() || hitsClipContent(*element);
        },
        [&](const Style::BoxPath&) {
            return true;
        },
        [&](const CSS::Keyword::None&) {
            return true;
        }
    );
}

bool RenderBox::hitTestBorderRadius(const HitTestLocation& hitTestLocation, const LayoutPoint& accumulatedOffset) const
{
    if (isRenderView() || !style().hasBorderRadius())
        return true;

    LayoutPoint adjustedLocation = accumulatedOffset + location();
    LayoutRect borderRect = borderBoxRect();
    borderRect.moveBy(adjustedLocation);

    auto borderShape = BorderShape::shapeForBorderRect(style(), borderRect);
    // To handle non-round corners, BorderShape should do the hit-testing.
    return hitTestLocation.intersects(borderShape.deprecatedRoundedRect());
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
    LayoutRect boundsRect = borderBoxRect();
    boundsRect.moveBy(adjustedLocation);
    if (visibleToHitTesting(request) && action == HitTestForeground && locationInContainer.intersects(boundsRect)) {
        if (!hitTestVisualOverflow(locationInContainer, accumulatedOffset))
            return false;

        if (!hitTestClipPath(locationInContainer, accumulatedOffset))
            return false;

        if (!hitTestBorderRadius(locationInContainer, accumulatedOffset))
            return false;

        updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
        if (result.addNodeToListBasedTestResult(protectedNodeForHitTest().get(), request, locationInContainer, boundsRect) == HitTestProgress::Stop)
            return true;
    }

    return RenderBoxModelObject::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, action);
}

// --------------------- painting stuff -------------------------------

BleedAvoidance RenderBox::determineBleedAvoidance(GraphicsContext& context) const
{
    if (context.paintingDisabled())
        return BleedAvoidance::None;

    const RenderStyle& style = this->style();

    if (!style.hasBackground() || !style.hasBorder() || !style.hasBorderRadius() || borderImageIsLoadedAndCanBeRendered())
        return BleedAvoidance::None;

    if (!theme().mayNeedBleedAvoidance(style))
        return BleedAvoidance::None;

    AffineTransform ctm = context.getCTM();
    FloatSize contextScaling(static_cast<float>(ctm.xScale()), static_cast<float>(ctm.yScale()));

    // Because LayoutRoundedRect uses IntRect internally the inset applied by the
    // BleedAvoidance::ShrinkBackground strategy cannot be less than one integer
    // layout coordinate, even with subpixel layout enabled. To take that into
    // account, we clamp the contextScaling to 1.0 for the following test so
    // that borderObscuresBackgroundEdge can only return true if the border
    // widths are greater than 2 in both layout coordinates and screen
    // coordinates.
    // This precaution will become obsolete if LayoutRoundedRect is ever promoted to
    // a sub-pixel representation.
    if (contextScaling.width() > 1) 
        contextScaling.setWidth(1);
    if (contextScaling.height() > 1) 
        contextScaling.setHeight(1);

    if (borderObscuresBackgroundEdge(contextScaling))
        return BleedAvoidance::ShrinkBackground;

    if (!style.hasUsedAppearance() && borderObscuresBackground() && backgroundHasOpaqueTopLayer())
        return BleedAvoidance::BackgroundOverBorder;

    return BleedAvoidance::UseTransparencyLayer;
}

ControlPart* RenderBox::ensureControlPart()
{
    auto& rareData = ensureRareData();
    auto type = style().usedAppearance();

    // Some form-controls may change because of zooming without recreating
    // a new renderer (e.g Menulist <-> MenulistButton).
    if (!rareData.controlPart || type != rareData.controlPart->type())
        rareData.controlPart = theme().createControlPart(*this);

    return rareData.controlPart.get();
}

ControlPart* RenderBox::ensureControlPartForRenderer()
{
    return theme().canCreateControlPartForRenderer(*this) ? ensureControlPart() : nullptr;
}

ControlPart* RenderBox::ensureControlPartForBorderOnly()
{
    return theme().canCreateControlPartForBorderOnly(*this) ? ensureControlPart() : nullptr;
}

ControlPart* RenderBox::ensureControlPartForDecorations()
{
    return theme().canCreateControlPartForDecorations(*this) ? ensureControlPart() : nullptr;
}

void RenderBox::paintBoxDecorations(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(*this))
        return;

    LayoutRect paintRect = borderBoxRect();
    paintRect.moveBy(paintOffset);
    adjustBorderBoxRectForPainting(paintRect);

    paintRect = theme().adjustedPaintRect(*this, paintRect);
    auto bleedAvoidance = determineBleedAvoidance(paintInfo.context());

    BackgroundPainter backgroundPainter { *this, paintInfo };

    // FIXME: Should eventually give the theme control over whether the box shadow should paint, since controls could have
    // custom shadows of their own.
    if (!BackgroundPainter::boxShadowShouldBeAppliedToBackground(*this, paintRect.location(), bleedAvoidance, { }))
        backgroundPainter.paintBoxShadow(paintRect, style(), Style::ShadowStyle::Normal);

    GraphicsContextStateSaver stateSaver(paintInfo.context(), false);
    if (bleedAvoidance == BleedAvoidance::UseTransparencyLayer) {
        // To avoid the background color bleeding out behind the border, we'll render background and border
        // into a transparency layer, and then clip that in one go (which requires setting up the clip before
        // beginning the layer).
        stateSaver.save();
        auto borderShape = BorderShape::shapeForBorderRect(style(), paintRect);
        borderShape.clipToOuterShape(paintInfo.context(), document().deviceScaleFactor());
        paintInfo.context().beginTransparencyLayer(1);
    }

    // If we have a native theme appearance, paint that before painting our background.
    // The theme will tell us whether or not we should also paint the CSS background.
    bool borderOrBackgroundPaintingIsNeeded = true;
    if (style().hasUsedAppearance()) {
        if (auto* control = ensureControlPartForRenderer())
            borderOrBackgroundPaintingIsNeeded = theme().paint(*this, *control, paintInfo, paintRect);
        else
            borderOrBackgroundPaintingIsNeeded = theme().paint(*this, paintInfo, paintRect);
    }

    BorderPainter borderPainter { *this, paintInfo };

    if (borderOrBackgroundPaintingIsNeeded) {
        if (bleedAvoidance == BleedAvoidance::BackgroundOverBorder)
            borderPainter.paintBorder(paintRect, style(), bleedAvoidance);

        backgroundPainter.paintBackground(paintRect, bleedAvoidance);

        if (style().hasUsedAppearance()) {
            if (auto* control = ensureControlPartForDecorations())
                theme().paint(*this, *control, paintInfo, paintRect);
            else
                theme().paintDecorations(*this, paintInfo, paintRect);
        }
    }

    backgroundPainter.paintBoxShadow(paintRect, style(), Style::ShadowStyle::Inset);

    if (bleedAvoidance != BleedAvoidance::BackgroundOverBorder) {
        bool paintCSSBorder = false;

        if (!style().hasUsedAppearance())
            paintCSSBorder = true;
        else if (borderOrBackgroundPaintingIsNeeded) {
            // The theme will tell us whether or not we should also paint the CSS border.
            if (auto* control = ensureControlPartForBorderOnly())
                paintCSSBorder = theme().paint(*this, *control, paintInfo, paintRect);
            else
                paintCSSBorder = theme().paintBorderOnly(*this, paintInfo);
        }

        if (paintCSSBorder && style().hasVisibleBorderDecoration())
            borderPainter.paintBorder(paintRect, style(), bleedAvoidance);
    }

    if (bleedAvoidance == BleedAvoidance::UseTransparencyLayer)
        paintInfo.context().endTransparencyLayer();
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
    if (!layers.usedFirst().hasImage() || layers.usedLength() > 1) {
        paintedExtent =  backgroundRect;
        return true;
    }

    auto geometry = BackgroundPainter::calculateFillLayerImageGeometry(*this, nullptr, layers.usedFirst(), paintOffset, backgroundRect);
    paintedExtent = geometry.destinationRect;
    return !geometry.hasNonLocalGeometry;
}

bool RenderBox::backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const
{
    if (!BackgroundPainter::paintsOwnBackground(*this))
        return false;

    Color backgroundColor = style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
    if (!backgroundColor.isOpaque())
        return false;

    // If the element has appearance, it might be painted by theme.
    // We cannot be sure if theme paints the background opaque.
    // In this case it is safe to not assume opaqueness.
    // FIXME: May be ask theme if it paints opaque.
    if (style().hasUsedAppearance())
        return false;
    // FIXME: Check the opaqueness of background images.

    if (hasClip() || hasClipPath())
        return false;

    // FIXME: Use rounded rect if border radius is present.
    if (style().hasBorderRadius())
        return false;
    
    // FIXME: The background color clip is defined by the last layer.
    if (style().backgroundLayers().usedLength() > 1)
        return false;
    LayoutRect backgroundRect;
    switch (style().backgroundLayers().usedFirst().clip()) {
    case FillBox::BorderBox:
        backgroundRect = borderBoxRect();
        break;
    case FillBox::PaddingBox:
        backgroundRect = paddingBoxRect();
        break;
    case FillBox::ContentBox:
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
    if (childStyle.usedVisibility() != Visibility::Visible)
        return false;
    if (!childStyle.shapeOutside().isNone())
        return false;
    if (!childBox.width() || !childBox.height())
        return false;
    if (RenderLayer* childLayer = childBox.layer()) {
        if (childLayer->isComposited())
            return false;
        // FIXME: Deal with z-index.
        if (!childStyle.usedZIndex().isAuto())
            return false;
        if (childLayer->isTransformed() || childLayer->isTransparent() || childLayer->hasFilter())
            return false;
        if (!childBox.scrollPosition().isZero())
            return false;
    }
    return true;
}

bool RenderBox::foregroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect, unsigned maxDepthToTest) const
{
    ASSERT(!isSkippedContentRoot(*this));

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
    // Root background painting is special.
    if (isDocumentElementRenderer())
        return false;

    LayoutRect backgroundRect;
    if (!getBackgroundPaintedExtent(paintOffset, backgroundRect))
        return false;

    if (CheckedPtr scrollableArea = layer() ? layer()->scrollableArea() : nullptr) {
        if (scrollableArea->scrollingMayRevealBackground())
            return false;
    }
    return foregroundIsKnownToBeOpaqueInRect(backgroundRect, backgroundObscurationTestMaxDepth);
}

bool RenderBox::backgroundHasOpaqueTopLayer() const
{
    auto& topLayer = style().backgroundLayers().usedFirst();
    if (topLayer.clip() != FillBox::BorderBox)
        return false;

    // Clipped with local scrolling
    if (hasNonVisibleOverflow() && topLayer.attachment() == FillAttachment::LocalBackground)
        return false;

    if (topLayer.hasOpaqueImage(*this) && topLayer.hasRepeatXY() && topLayer.image().tryStyleImage()->canRender(this, style().usedZoom()))
        return true;

    // If there is only one layer and no image, check whether the background color is opaque.
    if (style().backgroundLayers().usedLength() == 1 && !topLayer.hasImage()) {
        Color bgColor = style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
        if (bgColor.isOpaque())
            return true;
    }

    return false;
}

void RenderBox::paintMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(*this) || style().usedVisibility() != Visibility::Visible || paintInfo.phase != PaintPhase::Mask || paintInfo.context().paintingDisabled())
        return;

    LayoutRect paintRect = LayoutRect(paintOffset, size());
    adjustBorderBoxRectForPainting(paintRect);
    paintMaskImages(paintInfo, paintRect);
}

void RenderBox::paintClippingMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(*this) || style().usedVisibility() != Visibility::Visible || paintInfo.phase != PaintPhase::ClippingMask || paintInfo.context().paintingDisabled())
        return;

    LayoutRect paintRect = LayoutRect(paintOffset, size());

    if (document().settings().layerBasedSVGEngineEnabled() && WTF::holdsAlternative<Style::ReferencePath>(style().clipPath())) {
        paintSVGClippingMask(paintInfo, paintRect);
        return;
    }

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
        if (RefPtr maskBorder = style().maskBorder().source().tryStyleImage())
            allMaskImagesLoaded &= maskBorder->isLoaded(this);

        allMaskImagesLoaded &= Style::imagesAreLoaded(style().maskLayers(), *this);

        paintInfo.context().setCompositeOperation(CompositeOperator::DestinationIn);
        paintInfo.context().beginTransparencyLayer(1);
        compositeOp = CompositeOperator::SourceOver;
    }

    if (allMaskImagesLoaded) {
        BackgroundPainter { *this, paintInfo }.paintFillLayers(Color(), style().maskLayers(), paintRect, BleedAvoidance::None, compositeOp);
        BorderPainter { *this, paintInfo }.paintNinePieceImage(paintRect, style(), style().maskBorder(), compositeOp);
    }
    
    if (pushTransparencyLayer)
        paintInfo.context().endTransparencyLayer();
}

LayoutRect RenderBox::maskClipRect(const LayoutPoint& paintOffset)
{
    auto& maskBorder = style().maskBorder();
    if (!maskBorder.source().isNone()) {
        LayoutRect borderImageRect = borderBoxRect();
        
        // Apply outsets to the border box.
        borderImageRect.expand(style().maskBorderOutsets());
        return borderImageRect;
    }
    
    LayoutRect result;
    LayoutRect borderBox = borderBoxRect();
    for (auto& maskLayer : style().maskLayers().usedValues()) {
        if (maskLayer.hasImage()) {
            // Masks should never have fixed attachment, so it's OK for paintContainer to be null.
            result.unite(BackgroundPainter::calculateFillLayerImageGeometry(*this, nullptr, maskLayer, paintOffset, borderBox).destinationRect);
        }
    }
    return result;
}

void RenderBox::imageChanged(WrappedImagePtr image, const IntRect*)
{
    if (RefPtr source = style().borderImage().source().tryStyleImage(); source && source->data() == image) {
        if (parent())
            repaint();
        return;
    }

    if (RefPtr source = style().maskBorder().source().tryStyleImage(); source && source->data() == image) {
        if (parent())
            repaint();
        return;
    }

    if (!view().frameView().layoutContext().isInRenderTreeLayout() && isFloating()) {
        if (RefPtr shapeOutsideImage = style().shapeOutside().image(); shapeOutsideImage && shapeOutsideImage->data() == image) {
            ensureShapeOutsideInfo().markShapeAsDirty();
            markShapeOutsideDependentsForLayout();
        }
    }

    bool didFullRepaint = false;

    auto repaintForBackgroundAndMask = [&](auto& style) {
        if (!parent())
            return;

        if (!didFullRepaint)
            didFullRepaint = repaintLayerRectsForImage(image, style.backgroundLayers(), true);
        if (!didFullRepaint)
            didFullRepaint = repaintLayerRectsForImage(image, style.maskLayers(), false);
    };

    repaintForBackgroundAndMask(style());

    if (auto* firstLineStyle = style().getCachedPseudoStyle({ PseudoElementType::FirstLine }))
        repaintForBackgroundAndMask(*firstLineStyle);

    bool isNonEmpty;
    RefPtr styleImage = Style::findLayerUsedImage(style().backgroundLayers(), image, isNonEmpty);
    if (styleImage && isNonEmpty) {
        incrementVisuallyNonEmptyPixelCountIfNeeded(flooredIntSize(styleImage->imageSize(this, style().usedZoom())));
        if (auto styleable = Styleable::fromRenderer(*this))
            protectedDocument()->didLoadImage(styleable->protectedElement().get(), styleImage->cachedImage());
    }

    if (!isComposited())
        return;

    if (layer()->hasCompositedMask() && Style::findLayerUsedImage(style().maskLayers(), image, isNonEmpty))
        layer()->contentChanged(ContentChangeType::MaskImage);

    if (styleImage)
        layer()->contentChanged(ContentChangeType::BackgroundImage);
}

void RenderBox::incrementVisuallyNonEmptyPixelCountIfNeeded(const IntSize& size)
{
    if (didContibuteToVisuallyNonEmptyPixelCount())
        return;

    view().frameView().incrementVisuallyNonEmptyPixelCount(size);
    setDidContibuteToVisuallyNonEmptyPixelCount();
}

template<typename Layers>
bool RenderBox::repaintLayerRectsForImage(WrappedImagePtr image, const Layers& layers, bool drawingBackground)
{
    LayoutRect rendererRect;
    RenderBox* layerRenderer = nullptr;

    for (auto& layer : layers.usedValues()) {
        if (RefPtr layerImage = layer.image().tryStyleImage(); layerImage && layerImage->data() == image && (layerImage->isLoaded(this) || layerImage->canRender(this, style().usedZoom()))) {
            // Now that we know this image is being used, compute the renderer and the rect if we haven't already.
            bool drawingRootBackground = drawingBackground && (isDocumentElementRenderer() || (isBody() && !document().documentElement()->renderer()->hasBackground()));
            if (!layerRenderer) {
                if (drawingRootBackground) {
                    layerRenderer = &view();

                    auto& renderView = downcast<RenderView>(*layerRenderer);
                    LayoutUnit rw = renderView.frameView().contentsWidth();
                    LayoutUnit rh = renderView.frameView().contentsHeight();

                    rendererRect = LayoutRect(-layerRenderer->marginLeft(),
                        -layerRenderer->marginTop(),
                        std::max(layerRenderer->width() + layerRenderer->horizontalMarginExtent() + layerRenderer->borderLeft() + layerRenderer->borderRight(), rw),
                        std::max(layerRenderer->height() + layerRenderer->verticalMarginExtent() + layerRenderer->borderTop() + layerRenderer->borderBottom(), rh));

                    // If we're drawing the root background, then we want to use the bounds of the view
                    // (since root backgrounds cover the canvas, not just the element). If the root element
                    // is composited though, we need to issue the repaint to that root element.
                    auto documentElementRenderer = downcast<RenderBox>(document().documentElement()->renderer());
                    auto rendererLayer = documentElementRenderer->layer();
                    if (rendererLayer && rendererLayer->isComposited())
                        layerRenderer = documentElementRenderer;
                } else {
                    layerRenderer = this;
                    rendererRect = borderBoxRect();
                }
            }
            // FIXME: Figure out how to pass absolute position to calculateFillLayerImageGeometry (for pixel snapping)
            auto geometry = BackgroundPainter::calculateFillLayerImageGeometry(*layerRenderer, nullptr, layer, LayoutPoint(), rendererRect);
            if (geometry.hasNonLocalGeometry) {
                // Rather than incur the costs of computing the paintContainer for renderers with fixed backgrounds
                // in order to get the right destRect, just repaint the entire renderer.
                layerRenderer->repaint();
                return true;
            }
            
            LayoutRect rectToRepaint = geometry.destinationRect;
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
            if (geometry.destinationRect == rendererRect)
                return true;
        }
    }
    return false;
}

void RenderBox::clipToPaddingBoxShape(GraphicsContext& context, const LayoutPoint& accumulatedOffset, float deviceScaleFactor) const
{
    auto borderShape = BorderShape::shapeForBorderRect(style(), LayoutRect(accumulatedOffset, size()));
    borderShape.clipToInnerShape(context, deviceScaleFactor);
}

void RenderBox::clipToContentBoxShape(GraphicsContext& context, const LayoutPoint& accumulatedOffset, float deviceScaleFactor) const
{
    auto borderShape = borderShapeForContentClipping(LayoutRect { accumulatedOffset, size() });
    borderShape.clipToInnerShape(context, deviceScaleFactor);
}

bool RenderBox::pushContentsClip(PaintInfo& paintInfo, const LayoutPoint& accumulatedOffset)
{
    if (paintInfo.phase == PaintPhase::BlockBackground || paintInfo.phase == PaintPhase::SelfOutline || paintInfo.phase == PaintPhase::Mask)
        return false;

    bool isControlClip = paintInfo.phase != PaintPhase::EventRegion && hasControlClip();
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
    FloatRect clipRect = snapRectToDevicePixels((isControlClip ? controlClipRect(accumulatedOffset) : overflowClipRect(accumulatedOffset, OverlayScrollbarSizeRelevancy::IgnoreOverlayScrollbarSize, paintInfo.phase)), deviceScaleFactor);
    paintInfo.context().save();
    if (style().hasBorderRadius())
        clipToPaddingBoxShape(paintInfo.context(), accumulatedOffset, deviceScaleFactor);

    paintInfo.context().clip(clipRect);

    if (paintInfo.phase == PaintPhase::EventRegion || paintInfo.phase == PaintPhase::Accessibility)
        paintInfo.regionContext->pushClip(enclosingIntRect(clipRect));

    return true;
}

void RenderBox::popContentsClip(PaintInfo& paintInfo, PaintPhase originalPhase, const LayoutPoint& accumulatedOffset)
{
    ASSERT(hasControlClip() || (hasNonVisibleOverflow() && !layer()->isSelfPaintingLayer()));

    if (paintInfo.phase == PaintPhase::EventRegion || paintInfo.phase == PaintPhase::Accessibility)
        paintInfo.regionContext->popClip();

    paintInfo.context().restore();
    if (originalPhase == PaintPhase::Outline) {
        paintInfo.phase = PaintPhase::SelfOutline;
        paintObject(paintInfo, accumulatedOffset);
        paintInfo.phase = originalPhase;
    } else if (originalPhase == PaintPhase::ChildBlockBackground)
        paintInfo.phase = originalPhase;
}

LayoutRect RenderBox::overflowClipRect(const LayoutPoint& location, OverlayScrollbarSizeRelevancy relevancy, PaintPhase) const
{
    LayoutRect clipRect = borderBoxRect();
    clipRect.setLocation(location + clipRect.location() + LayoutSize(borderLeft(), borderTop()));
    clipRect.setSize(clipRect.size() - LayoutSize(borderLeft() + borderRight(), borderTop() + borderBottom()));
    if (style().overflowX() == Overflow::Clip && style().overflowY() == Overflow::Visible)
        clipRect.expandToInfiniteY();
    else if (style().overflowY() == Overflow::Clip && style().overflowX() == Overflow::Visible)
        clipRect.expandToInfiniteX();

    // Subtract out scrollbars if we have them.
    if (CheckedPtr scrollableArea = layer() ? layer()->scrollableArea() : nullptr) {
        if (shouldPlaceVerticalScrollbarOnLeft())
            clipRect.move(scrollableArea->verticalScrollbarWidth(relevancy, isHorizontalWritingMode()), 0);
        clipRect.contract(scrollableArea->verticalScrollbarWidth(relevancy, isHorizontalWritingMode()), scrollableArea->horizontalScrollbarHeight(relevancy, isHorizontalWritingMode()));
    }

    return clipRect;
}

LayoutRect RenderBox::clipRect(const LayoutPoint& location) const
{
    auto borderBoxRect = this->borderBoxRect();
    auto clipRect = LayoutRect(borderBoxRect.location() + location, borderBoxRect.size());

    return WTF::switchOn(style().clip(),
        [&](const CSS::Keyword::Auto&) {
            return clipRect;
        },
        [&](const Style::ClipRect& rect) {
            if (auto clipLeft = rect.value->left().tryLength()) {
                auto c = LayoutUnit { clipLeft->resolveZoom(Style::ZoomNeeded { }) };
                clipRect.move(c, 0_lu);
                clipRect.contract(c, 0_lu);
            }

            // We don't use the fragment-specific border box's width and height since clip offsets are (stupidly) specified
            // from the left and top edges. Therefore it's better to avoid constraining to smaller widths and heights.

            if (auto clipRight = rect.value->right().tryLength())
                clipRect.contract(width() - LayoutUnit { clipRight->resolveZoom(Style::ZoomNeeded { }) }, 0_lu);

            if (auto clipTop = rect.value->top().tryLength()) {
                auto c = LayoutUnit { clipTop->resolveZoom(Style::ZoomNeeded { }) };
                clipRect.move(0_lu, c);
                clipRect.contract(0_lu, c);
            }

            if (auto clipBottom = rect.value->bottom().tryLength())
                clipRect.contract(0_lu, height() - LayoutUnit { clipBottom->resolveZoom(Style::ZoomNeeded { }) });

            return clipRect;
        }
    );
}

LayoutUnit RenderBox::shrinkLogicalWidthToAvoidFloats(LayoutUnit childMarginStart, LayoutUnit childMarginEnd, const RenderBlock& containingBlock) const
{    
    LayoutUnit logicalTopPosition = logicalTop();
    LayoutUnit logicalHeight = containingBlock.logicalHeightForChild(*this);
    LayoutUnit result = containingBlock.availableLogicalWidthForLine(logicalTopPosition, logicalHeight) - childMarginStart - childMarginEnd;

    // We need to see if margins on either the start side or the end side can contain the floats in question. If they can,
    // then just using the line width is inaccurate. In the case where a float completely fits, we don't need to use the line
    // offset at all, but can instead push all the way to the content edge of the containing block. In the case where the float
    // doesn't fit, we can use the line offset, but we need to grow it by the margin to reflect the fact that the margin was
    // "consumed" by the float. Negative margins aren't consumed by the float, and so we ignore them.
    if (childMarginStart > 0) {
        LayoutUnit startContentSide = containingBlock.startOffsetForContent();
        LayoutUnit startContentSideWithMargin = startContentSide + childMarginStart;
        LayoutUnit startOffset = containingBlock.startOffsetForLine(logicalTopPosition, logicalHeight);
        if (startOffset > startContentSideWithMargin)
            result += childMarginStart;
        else
            result += startOffset - startContentSide;
    }
    
    if (childMarginEnd > 0) {
        LayoutUnit endContentSide = containingBlock.endOffsetForContent();
        LayoutUnit endContentSideWithMargin = endContentSide + childMarginEnd;
        LayoutUnit endOffset = containingBlock.endOffsetForLine(logicalTopPosition, logicalHeight);
        if (endOffset > endContentSideWithMargin)
            result += childMarginEnd;
        else
            result += endOffset - endContentSide;
    }

    return result;
}

LayoutUnit RenderBox::containingBlockLogicalWidthForContent() const
{
    CheckedPtr containingBlock = this->containingBlock();
    if (!containingBlock) {
        // Should not be called on detached renderer (e.g. during initial style setting).
        ASSERT_NOT_REACHED();
        return { };
    }

    if (isOutOfFlowPositioned()) {
        PositionedLayoutConstraints constraints(*this, LogicalBoxAxis::Inline);
        return constraints.containingInlineSize();
    }

    if (isGridItem()) {
        ASSERT(is<RenderGrid>(containingBlock));
        if (auto gridAreaContentLogicalWidth = this->gridAreaContentLogicalWidth())
            return gridAreaContentLogicalWidth->value_or(0_lu);
    }

    return containingBlock->contentBoxLogicalWidth();
}

LayoutUnit RenderBox::containingBlockLogicalHeightForContent(AvailableLogicalHeightType heightType) const
{
    if (isGridItem()) {
        if (auto gridAreaContentLogicalHeight = this->gridAreaContentLogicalHeight(); gridAreaContentLogicalHeight && *gridAreaContentLogicalHeight) {
            // FIXME: Containing block for a grid item is the grid area it's located in. We need to return whatever
            // height value we get from gridAreaContentLogicalHeight() here, including std::nullopt.
            return gridAreaContentLogicalHeight->value();
        }
    }

    if (auto* containingBlock = this->containingBlock())
        return containingBlock->availableLogicalHeight(heightType);

    ASSERT_NOT_REACHED();
    return 0_lu;
}

LayoutUnit RenderBox::containingBlockAvailableLineWidth() const
{
    return containingBlock()->availableLogicalWidthForLine(logicalTop(), availableLogicalHeight(AvailableLogicalHeightType::IncludeMarginBorderPadding));
}

LayoutUnit RenderBox::perpendicularContainingBlockLogicalHeight() const
{
    if (isGridItem()) {
        if (auto gridAreaContentLogicalHeight = this->gridAreaContentLogicalHeight(); gridAreaContentLogicalHeight && *gridAreaContentLogicalHeight)
            return gridAreaContentLogicalHeight->value();
    }

    auto* containingBlock = this->containingBlock();
    if (auto overridingLogicalHeight = containingBlock->overridingBorderBoxLogicalHeight())
        return containingBlock->contentBoxLogicalHeight(*overridingLogicalHeight);

    auto& containingBlockStyle = containingBlock->style();
    auto& logicalHeight = containingBlockStyle.logicalHeight();

    // FIXME: For now just support fixed heights.  Eventually should support percentage heights as well.
    if (auto fixedLogicalHeight = logicalHeight.tryFixed()) {
        // Use the content box logical height as specified by the style.
        return containingBlock->adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit { fixedLogicalHeight->resolveZoom(containingBlockStyle.usedZoomForLength()) });
    }

    LayoutUnit fillFallbackExtent = containingBlockStyle.writingMode().isHorizontal()
        ? view().frameView().layoutSize().height()
        : view().frameView().layoutSize().width();
    LayoutUnit fillAvailableExtent = containingBlock->availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding);
    view().addPercentHeightDescendant(const_cast<RenderBox&>(*this));
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=158286 We also need to perform the same percentHeightDescendant treatment to the element which dictates the return value for containingBlock()->availableLogicalHeight() above.
    return std::min(fillAvailableExtent, fillFallbackExtent);
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
    if (isFixedPos)
        mode.add(IsFixed);
    else if (mode.contains(IsFixed) && canContainFixedPositionObjects())
        mode.remove(IsFixed);

    if (wasFixed)
        *wasFixed = mode.contains(IsFixed);
    
    LayoutSize containerOffset = offsetFromContainer(*container, LayoutPoint(transformState.mappedPoint()));

    // Remove sticky positioning from the offset if it should be ignored. This is done here in
    // order to avoid piping this flag down the method chain.
    if (mode.contains(IgnoreStickyOffsets) && isStickilyPositioned())
        containerOffset -= stickyPositionOffset();

    pushOntoTransformState(transformState, mode, ancestorContainer, container, containerOffset, containerSkipped);
    if (containerSkipped)
        return;

    mode.remove(ApplyContainerFlip);

    container->mapLocalToContainer(ancestorContainer, transformState, mode, wasFixed);
}

const RenderElement* RenderBox::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    ASSERT(ancestorToStopAt != this);

    bool ancestorSkipped;
    RenderElement* container = this->container(ancestorToStopAt, ancestorSkipped);
    if (!container)
        return nullptr;

    pushOntoGeometryMap(geometryMap, ancestorToStopAt, container, ancestorSkipped);
    return ancestorSkipped ? ancestorToStopAt : container;
}

void RenderBox::mapAbsoluteToLocalPoint(OptionSet<MapCoordinatesMode> mode, TransformState& transformState) const
{
    bool isFixedPos = isFixedPositioned();
    if (isFixedPos)
        mode.add(IsFixed);
    else if (mode.contains(IsFixed) && canContainFixedPositionObjects()) {
        // If this box has a transform, it acts as a fixed position container for fixed descendants,
        // and may itself also be fixed position. So propagate 'fixed' up only if this box is fixed position.
        mode.remove(IsFixed);
    }

    RenderBoxModelObject::mapAbsoluteToLocalPoint(mode, transformState);
}

LayoutSize RenderBox::offsetFromContainer(const RenderElement& container, const LayoutPoint&, bool* offsetDependsOnPoint) const
{
    // A fragment "has" boxes inside it without being their container. 
    ASSERT(&container == this->container() || is<RenderFragmentContainer>(container));

    LayoutSize offset;    
    if (isInFlowPositioned())
        offset += offsetForInFlowPosition();

    if (!isInline() || isBlockLevelReplacedOrAtomicInline())
        offset += topLeftLocationOffset();

    if (auto* boxContainer = dynamicDowncast<RenderBox>(container))
        offset -= toLayoutSize(boxContainer->scrollPosition());

    if (isAbsolutelyPositioned() && container.isInFlowPositioned()) {
        if (auto* inlineContainer = dynamicDowncast<RenderInline>(container))
            offset += inlineContainer->offsetForInFlowPositionedInline(this);
    }

    if (offsetDependsOnPoint)
        *offsetDependsOnPoint |= is<RenderFragmentedFlow>(container);

    return offset;
}

auto RenderBox::localRectsForRepaint(RepaintOutlineBounds repaintOutlineBounds) const -> RepaintRects
{
    if (isInsideEntirelyHiddenLayer())
        return { };

    auto overflowRect = visualOverflowRect();
    // FIXME: layoutDelta needs to be applied in parts before/after transforms and
    // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
    overflowRect.move(view().frameView().layoutContext().layoutDelta());

    auto rects = RepaintRects { overflowRect };
    if (repaintOutlineBounds == RepaintOutlineBounds::Yes)
        rects.outlineBoundsRect = localOutlineBoundsRepaintRect();

    return rects;
}

auto RenderBox::computeVisibleRectsUsingPaintOffset(const RepaintRects& rects) const -> RepaintRects
{
    auto adjustedRects = rects;
    auto* layoutState = view().frameView().layoutContext().layoutState();

    if (hasLayer() && layer()->transform())
        adjustedRects.transform(*layer()->transform(), document().deviceScaleFactor());

    // We can't trust the bits on RenderObject, because this might be called while re-resolving style.
    if (style().hasInFlowPosition() && layer())
        adjustedRects.move(layer()->offsetForInFlowPosition());

    adjustedRects.moveBy(location());
    adjustedRects.move(layoutState->paintOffset());
    if (layoutState->isClipped())
        adjustedRects.clippedOverflowRect.intersect(layoutState->clipRect());
    return adjustedRects;
}

auto RenderBox::computeVisibleRectsInContainer(const RepaintRects& rects, const RenderLayerModelObject* container, VisibleRectContext context) const -> std::optional<RepaintRects>
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
    if (view().frameView().layoutContext().isPaintOffsetCacheEnabled() && !container && styleToUse.position() != PositionType::Fixed && !context.options.contains(VisibleRectContext::Option::UseEdgeInclusiveIntersection))
        return computeVisibleRectsUsingPaintOffset(rects);

    auto adjustedRects = rects;
    if (hasReflection()) {
        auto reflectedRects = RepaintRects { reflectedRect(adjustedRects.clippedOverflowRect) };
        adjustedRects.unite(reflectedRects);
    }

    if (container == this) {
        if (container->writingMode().isBlockFlipped())
            flipForWritingMode(adjustedRects);
        if (context.descendantNeedsEnclosingIntRect)
            adjustedRects.encloseToIntRects();
        return adjustedRects;
    }

    bool containerIsSkipped;
    auto* localContainer = this->container(container, containerIsSkipped);
    if (!localContainer)
        return adjustedRects;

    if (isWritingModeRoot()) {
        if (!isOutOfFlowPositioned() || !context.dirtyRectIsFlipped) {
            flipForWritingMode(adjustedRects);
            context.dirtyRectIsFlipped = true;
        }
    }

    auto locationOffset = this->locationOffset();

    // FIXME: This is needed as long as RenderWidget snaps to integral size/position.
    // is<RenderReplaced>() is a fast bit check, is<RenderWidget>() is a virtual function call.
    if (is<RenderReplaced>(this) && is<RenderWidget>(this)) {
        LayoutSize flooredLocationOffset = flooredIntSize(locationOffset);
        adjustedRects.expand(locationOffset - flooredLocationOffset);
        locationOffset = flooredLocationOffset;
        context.descendantNeedsEnclosingIntRect = true;
    } else if (auto* columnFlow = dynamicDowncast<RenderMultiColumnFlow>(*this)) {
        // We won't normally run this code. Only when the container is null (i.e., we're trying
        // to get the rect in view coordinates) will we come in here, since normally container
        // will be set and we'll stop at the flow thread. This case is mainly hit by the check for whether
        // or not images should animate.
        // FIXME: Just as with offsetFromContainer, we aren't really handling objects that span multiple columns properly.
        LayoutPoint physicalPoint(flipForWritingMode(adjustedRects.clippedOverflowRect.location()));
        if (auto* fragment = columnFlow->physicalTranslationFromFlowToFragment((physicalPoint))) {
            adjustedRects.clippedOverflowRect.setLocation(fragment->flipForWritingMode(physicalPoint));
            return fragment->computeVisibleRectsInContainer(adjustedRects, container, context);
        }
    }

    // We are now in our parent container's coordinate space. Apply our transform to obtain a bounding box
    // in the parent's coordinate space that encloses us.
    auto position = styleToUse.position();
    if (hasLayer() && layer()->isTransformed()) {
        context.hasPositionFixedDescendant = position == PositionType::Fixed;
        adjustedRects.transform(layer()->currentTransform(), document().deviceScaleFactor());
    } else if (position == PositionType::Fixed)
        context.hasPositionFixedDescendant = true;

    adjustedRects.move(locationOffset);

    if (position == PositionType::Absolute && localContainer->isInFlowPositioned() && is<RenderInline>(*localContainer)) {
        auto offsetForInFlowPosition = downcast<RenderInline>(*localContainer).offsetForInFlowPositionedInline(this);
        adjustedRects.move(offsetForInFlowPosition);
    } else if (styleToUse.hasInFlowPosition() && layer()) {
        // Apply the relative position offset when invalidating a rectangle.  The layer
        // is translated, but the render box isn't, so we need to do this to get the
        // right dirty rect.  Since this is called from RenderObject::setStyle, the relative position
        // flag on the RenderObject has been cleared, so use the one on the style().
        auto offsetForInFlowPosition = layer()->offsetForInFlowPosition();
        adjustedRects.move(offsetForInFlowPosition);
    }

    if (localContainer->hasNonVisibleOverflow()) {
        bool isEmpty = !downcast<RenderLayerModelObject>(*localContainer).applyCachedClipAndScrollPosition(adjustedRects, container, context);
        if (isEmpty) {
            if (context.options.contains(VisibleRectContext::Option::UseEdgeInclusiveIntersection))
                return std::nullopt;
            return adjustedRects;
        }
    }

    if (containerIsSkipped) {
        // If the container is below localContainer, then we need to map the rect into container's coordinates.
        LayoutSize containerOffset = container->offsetFromAncestorContainer(*localContainer);
        adjustedRects.move(-containerOffset);
        return adjustedRects;
    }
    return localContainer->computeVisibleRectsInContainer(adjustedRects, container, context);
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
    computeLogicalWidth(computedValues);

    setLogicalWidth(computedValues.m_extent);
    setLogicalLeft(computedValues.m_position);
    setMarginStart(computedValues.m_margins.m_start);
    setMarginEnd(computedValues.m_margins.m_end);
}

static LayoutUnit inlineSizeFromAspectRatio(LayoutUnit borderPaddingInlineSum, LayoutUnit borderPaddingBlockSum, double aspectRatioValue, BoxSizing boxSizing, LayoutUnit blockSize, const Style::AspectRatio& aspectRatio, bool isRenderReplaced)
{
    if (boxSizing == BoxSizing::BorderBox && aspectRatio.isRatio() && !isRenderReplaced)
        return std::max(borderPaddingInlineSum, LayoutUnit(blockSize * aspectRatioValue));

    return LayoutUnit((blockSize - borderPaddingBlockSum) * aspectRatioValue) + borderPaddingInlineSum;
}

static bool shouldMarginInlineEndContributeToScrollableOverflow(auto& renderer)
{
    auto isSupportedContent = renderer.isGridItem() || renderer.isFlexItemIncludingDeprecated() || (renderer.isInFlow() && renderer.parent()->isBlockContainer());
    if (!isSupportedContent)
        return false;

    auto& parentStyle = renderer.parent()->style();
    if (parentStyle.overflowX() != Overflow::Visible && parentStyle.overflowX() != Overflow::Clip)
        return true;
    return parentStyle.overflowY() != Overflow::Visible && parentStyle.overflowY() != Overflow::Clip;
}

void RenderBox::computeLogicalWidth(LogicalExtentComputedValues& computedValues) const
{
    computedValues.m_extent = logicalWidth();
    computedValues.m_position = logicalLeft();
    computedValues.m_margins.m_start = marginStart();
    computedValues.m_margins.m_end = marginEnd();

    if (isOutOfFlowPositioned()) {
        ASSERT(!overridingBorderBoxLogicalWidth());
        ASSERT(!overridingLogicalWidthForFlexBasisComputation());
        // FIXME: This calculation is not patched for block-flow yet.
        // https://bugs.webkit.org/show_bug.cgi?id=46500
        computeOutOfFlowPositionedLogicalWidth(computedValues);
        return;
    }

    // The parent box is flexing us, so it has increased or decreased our width. Use the width from the style context.
    // FIXME: Account for block-flow in flexible boxes (webkit.org/b/46418)
    if (auto logicalWidth = (parent()->isFlexibleBoxIncludingDeprecated() ? this->overridingBorderBoxLogicalWidth() : std::nullopt)) {
        computedValues.m_extent = *logicalWidth;
        return;
    }

    // FIXME: Stretching is the only reason why we don't want the box to be treated as a replaced element, so we could perhaps
    // refactor all this logic, not only for flex and grid since alignment is intended to be applied to any block.
    auto treatAsReplaced = [&] {
        // FIXME: Account for block-flow in flexible boxes.
        // https://bugs.webkit.org/show_bug.cgi?id=46418
        auto& parent = *this->parent();
        bool inVerticalBox = parent.isRenderDeprecatedFlexibleBox() && (parent.style().boxOrient() == BoxOrient::Vertical);
        bool stretching = (parent.style().boxAlign() == BoxAlignment::Stretch);
        auto isReplaced = is<RenderReplaced>(*this) && (!inVerticalBox || !stretching);
        if (!isReplaced)
            return false;
        return !isGridItem() || !hasStretchedLogicalWidth();
    }();

    auto usedLogicalWidthLength = [&] -> Style::PreferredSize {
        if (auto overridingLogicalWidthLength = overridingLogicalWidthForFlexBasisComputation())
            return *overridingLogicalWidthLength;
        return style().logicalWidth();
    }();

    auto containerLogicalWidth = std::max(0_lu, containingBlockLogicalWidthForContent());
    auto& styleToUse = style();
    if (isInline() && is<RenderReplaced>(*this)) {
        // just calculate margins
        computedValues.m_margins.m_start = Style::evaluateMinimum<LayoutUnit>(styleToUse.marginStart(), containerLogicalWidth, styleToUse.usedZoomForLength());
        computedValues.m_margins.m_end = Style::evaluateMinimum<LayoutUnit>(styleToUse.marginEnd(), containerLogicalWidth, styleToUse.usedZoomForLength());
        if (treatAsReplaced) {
            auto evaluatedWidth = downcast<RenderReplaced>(*this).computeReplacedLogicalWidth();
            auto totalWidth = evaluatedWidth + borderAndPaddingLogicalWidth();
            computedValues.m_extent = std::max(totalWidth, minPreferredLogicalWidth());
        }
        return;
    }

    auto& containingBlock = *this->containingBlock();
    bool hasPerpendicularContainingBlock = containingBlock.isHorizontalWritingMode() != isHorizontalWritingMode();

    // Width calculations
    auto logicalWidth = [&] {
        if (auto overridingLogicalWidth = this->overridingBorderBoxLogicalWidth())
            return *overridingLogicalWidth;
        if (treatAsReplaced)
            return downcast<RenderReplaced>(*this).computeReplacedLogicalWidth() + borderAndPaddingLogicalWidth();
        if (shouldComputeLogicalWidthFromAspectRatio() && style().logicalWidth().isAuto())
            return computeLogicalWidthFromAspectRatio();

        auto containerWidthInInlineDirection = !hasPerpendicularContainingBlock ? containerLogicalWidth : perpendicularContainingBlockLogicalHeight();
        auto preferredWidth = computeLogicalWidthUsing(usedLogicalWidthLength, containerWidthInInlineDirection, containingBlock);
        return constrainLogicalWidthByMinMax(preferredWidth, containerWidthInInlineDirection, containingBlock);
    };
    computedValues.m_extent = logicalWidth();

    // Margin calculations.
    if (hasPerpendicularContainingBlock || isFloating() || isInline()) {
        computedValues.m_margins.m_start = computeOrTrimInlineMargin(containingBlock, Style::MarginTrimSide::BlockStart, [&] {
            return Style::evaluateMinimum<LayoutUnit>(styleToUse.marginStart(), containerLogicalWidth, styleToUse.usedZoomForLength());
        });
        computedValues.m_margins.m_end = computeOrTrimInlineMargin(containingBlock, Style::MarginTrimSide::BlockEnd, [&] {
            return Style::evaluateMinimum<LayoutUnit>(styleToUse.marginEnd(), containerLogicalWidth, styleToUse.usedZoomForLength());
        });
    } else {
        auto containerLogicalWidthForAutoMargins = containerLogicalWidth;
        if (avoidsFloats() && containingBlock.containsFloats())
            containerLogicalWidthForAutoMargins = containingBlockAvailableLineWidth();
        bool hasInvertedDirection = containingBlock.writingMode().isInlineOpposing(writingMode());
        computeInlineDirectionMargins(containingBlock, containerLogicalWidth, containerLogicalWidthForAutoMargins, computedValues.m_extent,
            hasInvertedDirection ? computedValues.m_margins.m_end : computedValues.m_margins.m_start,
            hasInvertedDirection ? computedValues.m_margins.m_start : computedValues.m_margins.m_end);
    }
    
    auto shouldIgnoreOverconstrainedMargin = [&] {
        if (isGridItem() || isFlexItemIncludingDeprecated())
            return true;
        // Is this replaced inline?
        if (isFloating() || isInline())
            return true;
#if ENABLE(MATHML)
        // RenderMathMLBlocks take the size of their content so we must not adjust the margin to fill the container size.
        if (containingBlock.isRenderMathMLBlock())
            return true;
#endif
        if (hasPerpendicularContainingBlock)
            return true;

        if (shouldMarginInlineEndContributeToScrollableOverflow(*this))
            return true;

        return !containerLogicalWidth || containerLogicalWidth == (computedValues.m_extent + computedValues.m_margins.m_start + computedValues.m_margins.m_end);
    };
    if (!shouldIgnoreOverconstrainedMargin()) {
        auto availableSpaceForMargin = containerLogicalWidth - computedValues.m_extent;
        bool hasInvertedDirection = containingBlock.writingMode().isInlineOpposing(writingMode());
        if (hasInvertedDirection)
            computedValues.m_margins.m_start = availableSpaceForMargin - computedValues.m_margins.m_end;
        else
            computedValues.m_margins.m_end = availableSpaceForMargin - computedValues.m_margins.m_start;
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
    auto* container = containingBlock();
    bool isOrthogonalElement = isHorizontalWritingMode() != container->isHorizontalWritingMode();
    auto& marginStartLength = style().marginStart();
    auto& marginEndLength = style().marginEnd();
    LayoutUnit availableSizeForResolvingMargin = isOrthogonalElement ? containingBlockLogicalWidthForContent() : availableLogicalWidth;
    marginStart = computeOrTrimInlineMargin(*container, Style::MarginTrimSide::InlineStart, [&] {
        return Style::evaluateMinimum<LayoutUnit>(marginStartLength, availableSizeForResolvingMargin, style().usedZoomForLength());
    });
    marginEnd = computeOrTrimInlineMargin(*container, Style::MarginTrimSide::InlineEnd, [&] {
        return Style::evaluateMinimum<LayoutUnit>(marginEndLength, availableSizeForResolvingMargin, style().usedZoomForLength());
    });
    return availableLogicalWidth - marginStart - marginEnd;
}


template<typename Keyword> void RenderBox::computeIntrinsicKeywordLogicalWidths(Keyword, LayoutUnit borderAndPadding, LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if constexpr (std::same_as<Keyword, CSS::Keyword::MinIntrinsic>)
        computeIntrinsicKeywordLogicalWidths(minLogicalWidth, maxLogicalWidth);
    else {
        if (shouldComputeLogicalWidthFromAspectRatio()) {
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
    }
}

LayoutUnit RenderBox::computeIntrinsicLogicalWidthUsing(CSS::Keyword::WebkitFillAvailable, LayoutUnit availableLogicalWidth, LayoutUnit borderAndPadding) const
{
    return std::max(borderAndPadding, fillAvailableMeasure(availableLogicalWidth));
}

LayoutUnit RenderBox::computeIntrinsicLogicalWidthUsing(CSS::Keyword::MaxContent keyword, LayoutUnit /*availableLogicalWidth*/, LayoutUnit borderAndPadding) const
{
    LayoutUnit minLogicalWidth;
    LayoutUnit maxLogicalWidth;
    computeIntrinsicKeywordLogicalWidths(keyword, borderAndPadding, minLogicalWidth, maxLogicalWidth);

    return maxLogicalWidth + borderAndPadding;
}

LayoutUnit RenderBox::computeIntrinsicLogicalWidthUsing(CSS::Keyword::MinContent keyword, LayoutUnit /*availableLogicalWidth*/, LayoutUnit borderAndPadding) const
{
    LayoutUnit minLogicalWidth;
    LayoutUnit maxLogicalWidth;
    computeIntrinsicKeywordLogicalWidths(keyword, borderAndPadding, minLogicalWidth, maxLogicalWidth);

    return minLogicalWidth + borderAndPadding;
}

LayoutUnit RenderBox::computeIntrinsicLogicalWidthUsing(CSS::Keyword::MinIntrinsic keyword, LayoutUnit /*availableLogicalWidth*/, LayoutUnit borderAndPadding) const
{
    LayoutUnit minLogicalWidth;
    LayoutUnit maxLogicalWidth;
    computeIntrinsicKeywordLogicalWidths(keyword, borderAndPadding, minLogicalWidth, maxLogicalWidth);

    return minLogicalWidth + borderAndPadding;
}

LayoutUnit RenderBox::computeIntrinsicLogicalWidthUsing(CSS::Keyword::FitContent keyword, LayoutUnit availableLogicalWidth, LayoutUnit borderAndPadding) const
{
    LayoutUnit minLogicalWidth;
    LayoutUnit maxLogicalWidth;
    computeIntrinsicKeywordLogicalWidths(keyword, borderAndPadding, minLogicalWidth, maxLogicalWidth);

    return std::max(minLogicalWidth + borderAndPadding, std::min(maxLogicalWidth + borderAndPadding, fillAvailableMeasure(availableLogicalWidth)));
}

template<typename SizeType> LayoutUnit RenderBox::computeIntrinsicLogicalWidthUsingGeneric(const SizeType& logicalWidth, LayoutUnit availableLogicalWidth, LayoutUnit borderAndPadding) const
{
    if (logicalWidth.isFillAvailable())
        return computeIntrinsicLogicalWidthUsing(CSS::Keyword::WebkitFillAvailable { }, availableLogicalWidth, borderAndPadding);
    if (logicalWidth.isMinIntrinsic())
        return computeIntrinsicLogicalWidthUsing(CSS::Keyword::MinContent { }, availableLogicalWidth, borderAndPadding);
    if (logicalWidth.isMaxContent())
        return computeIntrinsicLogicalWidthUsing(CSS::Keyword::MaxContent { }, availableLogicalWidth, borderAndPadding);
    if (logicalWidth.isMinContent())
        return computeIntrinsicLogicalWidthUsing(CSS::Keyword::MinContent { }, availableLogicalWidth, borderAndPadding);
    if (logicalWidth.isFitContent())
        return computeIntrinsicLogicalWidthUsing(CSS::Keyword::FitContent { }, availableLogicalWidth, borderAndPadding);

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutUnit RenderBox::computeIntrinsicLogicalWidthUsing(const Style::PreferredSize& logicalWidth, LayoutUnit availableLogicalWidth, LayoutUnit borderAndPadding) const
{
    return computeIntrinsicLogicalWidthUsingGeneric(logicalWidth, availableLogicalWidth, borderAndPadding);
}

LayoutUnit RenderBox::computeIntrinsicLogicalWidthUsing(const Style::MinimumSize& logicalWidth, LayoutUnit availableLogicalWidth, LayoutUnit borderAndPadding) const
{
    return computeIntrinsicLogicalWidthUsingGeneric(logicalWidth, availableLogicalWidth, borderAndPadding);
}

LayoutUnit RenderBox::computeIntrinsicLogicalWidthUsing(const Style::MaximumSize& logicalWidth, LayoutUnit availableLogicalWidth, LayoutUnit borderAndPadding) const
{
    return computeIntrinsicLogicalWidthUsingGeneric(logicalWidth, availableLogicalWidth, borderAndPadding);
}

LayoutUnit RenderBox::computeIntrinsicLogicalWidthUsing(const Style::FlexBasis& logicalWidth, LayoutUnit availableLogicalWidth, LayoutUnit borderAndPadding) const
{
    return computeIntrinsicLogicalWidthUsingGeneric(logicalWidth, availableLogicalWidth, borderAndPadding);
}

template<typename SizeType> LayoutUnit RenderBox::computeLogicalWidthUsingGeneric(const SizeType& logicalWidth, LayoutUnit availableLogicalWidth, const RenderBlock& containingBlock) const
{
    if constexpr (std::same_as<SizeType, Style::MinimumSize>) {
        if (logicalWidth.isAuto())
            return borderAndPaddingLogicalWidth();
    }

    if (logicalWidth.isSpecified()) {
        // FIXME: If the containing block flow is perpendicular to our direction we need to use the available logical height instead.
        return adjustBorderBoxLogicalWidthForBoxSizing(Style::evaluate<LayoutUnit>(logicalWidth, availableLogicalWidth, style().usedZoomForLength()));
    }

    if (logicalWidth.isIntrinsic() || logicalWidth.isMinIntrinsic())
        return computeIntrinsicLogicalWidthUsing(logicalWidth, availableLogicalWidth, borderAndPaddingLogicalWidth());

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    LayoutUnit logicalWidthResult = fillAvailableMeasure(availableLogicalWidth, marginStart, marginEnd);

    if (shrinkToAvoidFloats() && containingBlock.containsFloats())
        logicalWidthResult = std::min(logicalWidthResult, shrinkLogicalWidthToAvoidFloats(marginStart, marginEnd, containingBlock));

    if constexpr (std::same_as<SizeType, Style::PreferredSize> || std::same_as<SizeType, Style::FlexBasis>) {
        if (sizesPreferredLogicalWidthToFitContent())
            return std::max(minPreferredLogicalWidth(), std::min(maxPreferredLogicalWidth(), logicalWidthResult));
    }
    return logicalWidthResult;
}

LayoutUnit RenderBox::computeLogicalWidthUsing(const Style::PreferredSize& logicalWidth, LayoutUnit availableLogicalWidth, const RenderBlock& containingBlock) const
{
    return computeLogicalWidthUsingGeneric(logicalWidth, availableLogicalWidth, containingBlock);
}

LayoutUnit RenderBox::computeLogicalWidthUsing(const Style::MinimumSize& logicalWidth, LayoutUnit availableLogicalWidth, const RenderBlock& containingBlock) const
{
    return computeLogicalWidthUsingGeneric(logicalWidth, availableLogicalWidth, containingBlock);
}

LayoutUnit RenderBox::computeLogicalWidthUsing(const Style::MaximumSize& logicalWidth, LayoutUnit availableLogicalWidth, const RenderBlock& containingBlock) const
{
    return computeLogicalWidthUsingGeneric(logicalWidth, availableLogicalWidth, containingBlock);
}

LayoutUnit RenderBox::computeLogicalWidthUsing(const Style::FlexBasis& logicalWidth, LayoutUnit availableLogicalWidth, const RenderBlock& containingBlock) const
{
    return computeLogicalWidthUsingGeneric(logicalWidth, availableLogicalWidth, containingBlock);
}

bool RenderBox::columnFlexItemHasStretchAlignment() const
{
    // auto margins mean we don't stretch. Note that this function will only be
    // used for widths, so we don't have to check marginBefore/marginAfter.
    const auto& parentStyle = parent()->style();
    ASSERT(parentStyle.isColumnFlexDirection());
    if (style().marginStart().isAuto() || style().marginEnd().isAuto())
        return false;

    auto normalBehavior = containingBlock()->selfAlignmentNormalBehavior();

    if (!style().alignSelf().isAuto())
        return style().alignSelf().resolve(normalBehavior).position() == ItemPosition::Stretch;
    return parentStyle.alignItems().resolve(normalBehavior).position() == ItemPosition::Stretch;
}

bool RenderBox::isStretchingColumnFlexItem() const
{
    if (parent()->isRenderDeprecatedFlexibleBox() && parent()->style().boxOrient() == BoxOrient::Vertical && parent()->style().boxAlign() == BoxAlignment::Stretch)
        return true;

    // We don't stretch multiline flexboxes because they need to apply line spacing (align-content) first.
    if (is<RenderFlexibleBox>(*parent()) && parent()->style().flexWrap() == FlexWrap::NoWrap && parent()->style().isColumnFlexDirection() && columnFlexItemHasStretchAlignment())
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
    if (containingBlock->isHorizontalWritingMode() != isHorizontalWritingMode()) {
        if (auto* grid = dynamicDowncast<RenderGrid>(*this); grid && grid->isSubgridInParentDirection(Style::GridTrackSizingDirection::Columns))
            return true;

        auto normalBehavior = containingBlock->selfAlignmentNormalBehavior(this);
        if (!style.justifySelf().isAuto())
            return style.justifySelf().resolve(normalBehavior).position() == ItemPosition::Stretch;
        return containingBlock->style().justifyItems().resolve(normalBehavior).position() == ItemPosition::Stretch;
    }
    if (auto* grid = dynamicDowncast<RenderGrid>(*this); grid && grid->isSubgridInParentDirection(Style::GridTrackSizingDirection::Rows))
        return true;

    auto normalBehavior = containingBlock->selfAlignmentNormalBehavior(this);
    if (!style.alignSelf().isAuto())
        return style.alignSelf().resolve(normalBehavior).position() == ItemPosition::Stretch;
    return containingBlock->style().alignItems().resolve(normalBehavior).position() == ItemPosition::Stretch;
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
    if (containingBlock->isHorizontalWritingMode() != isHorizontalWritingMode()) {
        if (auto* grid = dynamicDowncast<RenderGrid>(*this); grid && grid->isSubgridInParentDirection(Style::GridTrackSizingDirection::Rows))
            return true;

        if (!style.alignSelf().isAuto())
            return style.alignSelf().resolve(normalItemPosition).position() == ItemPosition::Stretch;
        return containingBlock->style().alignItems().resolve(normalItemPosition).position() == ItemPosition::Stretch;
    }
    if (auto* grid = dynamicDowncast<RenderGrid>(*this); grid && grid->isSubgridInParentDirection(Style::GridTrackSizingDirection::Columns))
        return true;

    if (!style.justifySelf().isAuto())
        return style.justifySelf().resolve(normalItemPosition).position() == ItemPosition::Stretch;
    return containingBlock->style().justifyItems().resolve(normalItemPosition).position() == ItemPosition::Stretch;
}

bool RenderBox::sizesPreferredLogicalWidthToFitContent() const
{
    // Marquees in WinIE are like a mixture of blocks and inline-blocks.  They size as though they're blocks,
    // but they allow text to sit on the same line as the marquee.
    if (isFloating() || (isNonReplacedAtomicInlineLevelBox() && !isHTMLMarquee()))
        return true;

    if (isGridItem()) {
        // FIXME: The masonry logic should not be living in RenderBox; it should ideally live in RenderGrid.
        // This is a temporary solution to prevent regressions.
        auto* renderGrid = downcast<RenderGrid>(parent());
        return (renderGrid->areMasonryColumns() && !GridLayoutFunctions::isOrthogonalGridItem(*renderGrid, *this)) || !hasStretchedLogicalWidth();
    }

    // This code may look a bit strange.  Basically width:intrinsic should clamp the size when testing both
    // min-width and width.  max-width is only clamped if it is also intrinsic.
    auto& logicalWidth = style().logicalWidth();
    if (logicalWidth.isIntrinsicKeyword())
        return true;

    // Children of a horizontal marquee do not fill the container by default.
    // FIXME: Need to deal with MarqueeDirection::Auto value properly. It could be vertical.
    // FIXME: Think about block-flow here.  Need to find out how marquee direction relates to
    // block-flow (as well as how marquee overflow should relate to block flow).
    // https://bugs.webkit.org/show_bug.cgi?id=46472
    if (parent()->isHTMLMarquee()) {
        auto dir = parent()->style().marqueeDirection();
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
    if (is<RenderFlexibleBox>(*parent())) {
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
    if (parent()->isRenderDeprecatedFlexibleBox() && (parent()->style().boxOrient() == BoxOrient::Horizontal || parent()->style().boxAlign() != BoxAlignment::Stretch))
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

template<typename Function>
LayoutUnit RenderBox::computeOrTrimInlineMargin(const RenderBlock& containingBlock, Style::MarginTrimSide marginSide, NOESCAPE const Function& computeInlineMargin) const
{
    if (containingBlock.shouldTrimChildMargin(marginSide, *this)) {
        // FIXME(255434): This should be set when the margin is being trimmed
        // within the context of its layout system (block, flex, grid) and should not 
        // be done at this level within RenderBox. We should be able to leave the 
        // trimming responsibility to each of those contexts and not need to
        // do any of it here (trimming the margin and setting the rare data bit)
        if (isGridItem() && (marginSide == Style::MarginTrimSide::InlineStart || marginSide == Style::MarginTrimSide::InlineEnd))
            const_cast<RenderBox&>(*this).markMarginAsTrimmed(marginSide);
        return 0_lu;
    }
    return computeInlineMargin();
}

void RenderBox::computeInlineDirectionMargins(const RenderBlock& containingBlock, LayoutUnit containerWidth, std::optional<LayoutUnit> availableSpaceAdjustedWithFloats, LayoutUnit childWidth, LayoutUnit& marginStart, LayoutUnit& marginEnd) const
{
    auto& containingBlockStyle = containingBlock.style();
    auto marginStartLength = style().marginStart(containingBlockStyle.writingMode());
    auto marginEndLength = style().marginEnd(containingBlockStyle.writingMode());

    const auto& zoomFactor = style().usedZoomForLength();

    if (isFloating()) {
        marginStart = Style::evaluateMinimum<LayoutUnit>(marginStartLength, containerWidth, zoomFactor);
        marginEnd = Style::evaluateMinimum<LayoutUnit>(marginEndLength, containerWidth, zoomFactor);
        return;
    }

    if (isInline()) {
        // Inline blocks/tables don't have their margins increased.
        marginStart = computeOrTrimInlineMargin(containingBlock, Style::MarginTrimSide::InlineStart, [&] {
            return Style::evaluateMinimum<LayoutUnit>(marginStartLength, containerWidth, zoomFactor);
        });
        marginEnd = computeOrTrimInlineMargin(containingBlock, Style::MarginTrimSide::InlineStart, [&] {
            return Style::evaluateMinimum<LayoutUnit>(marginEndLength, containerWidth, zoomFactor);
        });
        return;
    }

    if (is<RenderFlexibleBox>(containingBlock)) {
        // We need to let flexbox handle the margin adjustment - otherwise, flexbox
        // will think we're wider than we actually are and calculate line sizes
        // wrong. See also http://dev.w3.org/csswg/css-flexbox/#auto-margins
        if (marginStartLength.isAuto())
            marginStartLength = 0_css_px;
        if (marginEndLength.isAuto())
            marginEndLength = 0_css_px;
    }

    if (isGridItem() && downcast<RenderGrid>(containingBlock).isComputingTrackSizes()) {
        if (marginStartLength.isAuto())
            marginStartLength = 0_css_px;
        if (marginEndLength.isAuto())
            marginEndLength = 0_css_px;
    }

    auto handleMarginAuto = [&] {
        auto containerWidthForMarginAuto = availableSpaceAdjustedWithFloats.value_or(containerWidth);
        // Case One: The object is being centered in the containing block's available logical width.
        auto marginAutoCenter = marginStartLength.isAuto() && marginEndLength.isAuto() && childWidth < containerWidthForMarginAuto;
        auto alignModeCenter = containingBlock.style().textAlign() == Style::TextAlign::WebKitCenter && !marginStartLength.isAuto() && !marginEndLength.isAuto();
        if (marginAutoCenter || alignModeCenter) {
            // Other browsers center the margin box for align=center elements so we match them here.
            marginStart = computeOrTrimInlineMargin(containingBlock, Style::MarginTrimSide::InlineStart, [&] {
                auto marginStartWidth = Style::evaluateMinimum<LayoutUnit>(marginStartLength, containerWidthForMarginAuto, zoomFactor);
                auto marginEndWidth = Style::evaluateMinimum<LayoutUnit>(marginEndLength, containerWidthForMarginAuto, zoomFactor);
                auto centeredMarginBoxStart = std::max<LayoutUnit>(0, (containerWidthForMarginAuto - childWidth - marginStartWidth - marginEndWidth) / 2);
                return centeredMarginBoxStart + marginStartWidth;
            });
            marginEnd = computeOrTrimInlineMargin(containingBlock, Style::MarginTrimSide::InlineEnd, [&] {
                auto marginEndWidth = Style::evaluateMinimum<LayoutUnit>(marginEndLength, containerWidthForMarginAuto, zoomFactor);
                return containerWidthForMarginAuto - childWidth - marginStart + marginEndWidth;
            });
            return true;
        }

        // Case Two: The object is being pushed to the start of the containing block's available logical width.
        if (marginEndLength.isAuto() && childWidth < containerWidthForMarginAuto) {
            marginStart = Style::evaluate<LayoutUnit>(marginStartLength, containerWidthForMarginAuto, zoomFactor);
            marginEnd = containerWidthForMarginAuto - childWidth - marginStart;
            return true;
        }

        // Case Three: The object is being pushed to the end of the containing block's available logical width.
        auto pushToEndFromTextAlign = !marginEndLength.isAuto() && ((!containingBlockStyle.writingMode().isBidiLTR() && containingBlockStyle.textAlign() == Style::TextAlign::WebKitLeft)
            || (containingBlockStyle.writingMode().isBidiLTR() && containingBlockStyle.textAlign() == Style::TextAlign::WebKitRight));
        if ((marginStartLength.isAuto() || pushToEndFromTextAlign) && childWidth < containerWidthForMarginAuto) {
            marginEnd = computeOrTrimInlineMargin(containingBlock, Style::MarginTrimSide::InlineEnd, [&] {
                return Style::evaluate<LayoutUnit>(marginEndLength, containerWidthForMarginAuto, zoomFactor);
            });
            marginStart = computeOrTrimInlineMargin(containingBlock, Style::MarginTrimSide::InlineStart, [&] {
                return containerWidthForMarginAuto - childWidth - marginEnd;
            });
            return true;
        }
        return false;
    };
    if (handleMarginAuto())
        return;
    
    // Case Four: Either no auto margins, or our width is >= the container width (css2.1, 10.3.3). In that case
    // auto margins will just turn into 0.
    marginStart = computeOrTrimInlineMargin(containingBlock, Style::MarginTrimSide::InlineStart, [&] {
        return Style::evaluateMinimum<LayoutUnit>(marginStartLength, containerWidth, zoomFactor);
    });
    marginEnd = computeOrTrimInlineMargin(containingBlock, Style::MarginTrimSide::InlineEnd, [&] {
        return Style::evaluateMinimum<LayoutUnit>(marginEndLength, containerWidth, zoomFactor);
    });
}

RenderBoxFragmentInfo* RenderBox::renderBoxFragmentInfo(RenderFragmentContainer* fragment, RenderBoxFragmentInfoFlags cacheFlag) const
{
    // Make sure nobody is trying to call this with a null fragment.
    if (!fragment)
        return nullptr;

    // If we have computed our width in this fragment already, it will be cached, and we can
    // just return it.
    RenderBoxFragmentInfo* boxInfo = fragment->renderBoxFragmentInfo(*this);
    if (boxInfo && cacheFlag == RenderBoxFragmentInfoFlags::CacheRenderBoxFragmentInfo)
        return boxInfo;

    return nullptr;
}

static bool shouldFlipBeforeAfterMargins(WritingMode containingBlockWritingMode, WritingMode childWritingMode)
{
    ASSERT(containingBlockWritingMode.isOrthogonal(childWritingMode));
    auto childBlockFlowDirection = childWritingMode.blockDirection();
    bool shouldFlip = false;
    switch (containingBlockWritingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
        shouldFlip = (childBlockFlowDirection == FlowDirection::RightToLeft);
        break;
    case FlowDirection::BottomToTop:
        shouldFlip = (childBlockFlowDirection == FlowDirection::RightToLeft);
        break;
    case FlowDirection::RightToLeft:
        shouldFlip = (childBlockFlowDirection == FlowDirection::BottomToTop);
        break;
    case FlowDirection::LeftToRight:
        shouldFlip = (childBlockFlowDirection == FlowDirection::BottomToTop);
        break;
    }

    if (containingBlockWritingMode.isInlineFlipped())
        shouldFlip = !shouldFlip;

    return shouldFlip;
}

void RenderBox::cacheIntrinsicContentLogicalHeightForFlexItem(LayoutUnit height) const
{
    // FIXME: it should be enough with checking hasOverridingLogicalHeight() as this logic could be shared
    // by any layout system using overrides like grid or flex. However this causes a never ending sequence of calls
    // between layoutBlock() <-> relayoutToAvoidWidows().
    if (isFloatingOrOutOfFlowPositioned())
        return;
    CheckedPtr flexibleBox = dynamicDowncast<RenderFlexibleBox>(parent());
    if (!flexibleBox)
        return;
    if (overridingBorderBoxLogicalHeight() || shouldComputeLogicalHeightFromAspectRatio())
        return;
    flexibleBox->setCachedFlexItemIntrinsicContentLogicalHeight(*this, height);
}

void RenderBox::overrideLogicalHeightForSizeContainment()
{
    LayoutUnit intrinsicHeight;
    if (auto height = explicitIntrinsicInnerLogicalHeight())
        intrinsicHeight = height.value();
    else if (isRenderMenuList()) {
        // RenderMenuList has its own theme, if there isn't explicitIntrinsicInnerLogicalHeight,
        // as a size containment, it should be treated as if there is no content, and the height
        // should the original logical height for theme.
        return;
    }

    // We need the exact width of border and padding here, yet we can't use borderAndPadding* interfaces.
    // Because these interfaces evetually call borderAfter/Before, and RenderBlock::borderBefore
    // adds extra border to fieldset by adding intrinsicBorderForFieldset which is not needed here.
    auto borderAndPadding = RenderBox::borderBefore() + RenderBox::paddingBefore() + RenderBox::borderAfter() + RenderBox::paddingAfter();
    setLogicalHeight(intrinsicHeight + borderAndPadding + scrollbarLogicalHeight());
}

void RenderBox::updateLogicalHeight()
{
    if (shouldApplySizeContainment() && !isRenderGrid())
        overrideLogicalHeightForSizeContainment();

    cacheIntrinsicContentLogicalHeightForFlexItem(contentBoxLogicalHeight());
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

    // Inline non-replaced elements do not support a height property.
    if (isInline() && !isBlockLevelReplacedOrAtomicInline())
        return computedValues;

    // Let's allow the table cell to compute its preferred size.
    if (CheckedPtr tableCell = dynamicDowncast<RenderTableCell>(*this); tableCell && !tableCell->isComputingPreferredSize()) {
        // Use the value set by table layout for orthogonal cells which in this case the logical width of the cell from the table's point of view.
        // see RenderTableCell::setCellLogicalWidth.
        if (tableCell->isOrthogonal())
            computedValues.m_extent = overridingBorderBoxLogicalHeight().value_or(computedValues.m_extent);
        return computedValues;
    }

    if (isOutOfFlowPositioned()) {
        computeOutOfFlowPositionedLogicalHeight(computedValues);
        return computedValues;
    }

    auto& parent = *this->parent();

    // Context here refers to the general term and not necessarily a formatting context
    // since, for example, we might compute the logical height from the aspect ratio.
    auto usedLogicalHeightFromContext = [&] -> std::optional<LayoutUnit> {
        if (is<RenderTable>(*this)) {
            // Table as flex and grid item is special and needs table like handling.
            auto heightValue = logicalHeight;
            if (shouldComputeLogicalHeightFromAspectRatio())
                heightValue = blockSizeFromAspectRatio(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent(), style().logicalAspectRatio(), style().boxSizingForAspectRatio(), logicalWidth(), style().aspectRatio(), is<RenderReplaced>(*this));
            return heightValue;
        }

        if (is<RenderFlexibleBox>(parent)) {
            if (auto overridingLogicalHeight = overridingLogicalHeightForFlexBasisComputation()) {
                ASSERT(!this->overridingBorderBoxLogicalHeight());
                return { };
            }

            if (auto overridingLogicalHeight = this->overridingBorderBoxLogicalHeight())
                return *overridingLogicalHeight;

            if (CheckedPtr replaced = dynamicDowncast<RenderReplaced>(*this))
                return replaced->computeReplacedLogicalHeight() + borderAndPaddingLogicalHeight();
            return { };
        }


        if (CheckedPtr deprecatedFlexBox = dynamicDowncast<RenderDeprecatedFlexibleBox>(parent)) {
            if (auto overridingLogicalHeight = this->overridingBorderBoxLogicalHeight())
                return *overridingLogicalHeight;

            auto& flexBoxStyle = deprecatedFlexBox->style();
            auto treatAsReplaced = [&] {
                if (!is<RenderReplaced>(*this))
                    return false;
                bool inHorizontalBox = flexBoxStyle.boxOrient() == BoxOrient::Horizontal;
                bool stretching = flexBoxStyle.boxAlign() == BoxAlignment::Stretch;
                return !inHorizontalBox || !stretching;
            };
            if (treatAsReplaced())
                return downcast<RenderReplaced>(*this).computeReplacedLogicalHeight() + borderAndPaddingLogicalHeight();

            // Block children of horizontal flexible boxes fill the height of the box.
            if (style().logicalHeight().isAuto() && flexBoxStyle.boxOrient() == BoxOrient::Horizontal && deprecatedFlexBox->isStretchingChildren())
                return deprecatedFlexBox->contentBoxLogicalHeight() - marginBefore() - marginAfter();

            return { };
        }
        if (is<RenderGrid>(parent)) {
            if (auto overridingLogicalHeight = this->overridingBorderBoxLogicalHeight())
                return *overridingLogicalHeight;

            if (CheckedPtr replaced = dynamicDowncast<RenderReplaced>(*this))
                return replaced->computeReplacedLogicalHeight() + borderAndPaddingLogicalHeight();

            return { };
        }

        if (CheckedPtr replaced = dynamicDowncast<RenderReplaced>(*this))
            return replaced->computeReplacedLogicalHeight() + borderAndPaddingLogicalHeight();

        return { };
    };

    auto computedLogicalHeight = [&] -> Style::PreferredSize {
        if (is<RenderFlexibleBox>(parent)) {
            if (auto overridingLogicalHeight = overridingLogicalHeightForFlexBasisComputation()) {
                ASSERT(!this->overridingBorderBoxLogicalHeight());
                return *overridingLogicalHeight;
            }
        }
        return style().logicalHeight();
    }();

    auto usedLogicalHeight = [&] {
        if (auto heightFromFormattingContext = usedLogicalHeightFromContext())
            return *heightFromFormattingContext;

        // Callers passing LayoutUnit::max() for logicalHeight means an indefinite height, so
        // translate this to a nullopt intrinsic height for further logical height computations.
        auto intrinsicHeight = logicalHeight != LayoutUnit::max() ? std::make_optional(logicalHeight) : std::nullopt;
        if (shouldComputeLogicalHeightFromAspectRatio()) {
            if (intrinsicHeight && style().boxSizing() == BoxSizing::ContentBox)
                *intrinsicHeight -= RenderBox::borderBefore() + RenderBox::paddingBefore() + RenderBox::borderAfter() + RenderBox::paddingAfter();
            auto heightFromAspectRatio = blockSizeFromAspectRatio(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent(), style().logicalAspectRatio(), style().boxSizingForAspectRatio(), logicalWidth(), style().aspectRatio(), is<RenderReplaced>(*this));
            return constrainLogicalHeightByMinMax(heightFromAspectRatio, intrinsicHeight);
        }

        if (intrinsicHeight)
            *intrinsicHeight -= borderAndPaddingLogicalHeight();
        auto mainOrPreferredHeight = computeLogicalHeightUsing(computedLogicalHeight, intrinsicHeight).value_or(computedValues.m_extent);
        return constrainLogicalHeightByMinMax(mainOrPreferredHeight, intrinsicHeight);
    };
    computedValues.m_extent = usedLogicalHeight();

    auto computeMargins = [&] {
        auto& containingBlock = *this->containingBlock();
        bool hasPerpendicularContainingBlock = containingBlock.isHorizontalWritingMode() != isHorizontalWritingMode();
        bool shouldFlipBeforeAfter = hasPerpendicularContainingBlock ? shouldFlipBeforeAfterMargins(containingBlock.writingMode(), writingMode()) : containingBlock.writingMode().isBlockOpposing(writingMode());
        auto marginBefore = shouldFlipBeforeAfter ? computedValues.m_margins.m_after : computedValues.m_margins.m_before;
        auto marginAfter = shouldFlipBeforeAfter ? computedValues.m_margins.m_before : computedValues.m_margins.m_after;

        hasPerpendicularContainingBlock ? computeInlineDirectionMargins(containingBlock, containingBlockLogicalWidthForContent(), { }, computedValues.m_extent, marginBefore, marginAfter) : computeBlockDirectionMargins(containingBlock, marginBefore, marginAfter);
        computedValues.m_margins.m_before = shouldFlipBeforeAfter ? marginAfter : marginBefore;
        computedValues.m_margins.m_after = shouldFlipBeforeAfter ? marginBefore : marginAfter;
    };
    computeMargins();

    // WinIE quirk: The <html> block always fills the entire canvas in quirks mode. The <body> always fills the
    // <html> block in quirks mode. Only apply this quirk if the block is normal flow and no height
    // is specified. When we're printing, we also need this quirk if the body or root has a percentage 
    // height since we don't set a height in RenderView when we're printing. So without this quirk, the 
    // height has nothing to be a percentage of, and it ends up being 0. That is bad.
    auto paginatedContentNeedsBaseHeight = [&] {
        if (!document().printing() || !computedLogicalHeight.isPercentOrCalculated() || isInline())
            return false;
        if (isDocumentElementRenderer())
            return true;
        auto* documentElementRenderer = document().documentElement()->renderer();
        return isBody() && &parent == documentElementRenderer && documentElementRenderer->style().logicalHeight().isPercentOrCalculated();
    };
    if (stretchesToViewport() || paginatedContentNeedsBaseHeight()) {
        auto margins = collapsedMarginBefore() + collapsedMarginAfter();
        auto visibleHeight = view().pageOrViewLogicalHeight();
        if (isDocumentElementRenderer())
            computedValues.m_extent = std::max(computedValues.m_extent, visibleHeight - margins);
        else if (parentBox()) {
            auto marginsBordersPadding = margins + parentBox()->marginBefore() + parentBox()->marginAfter() + parentBox()->borderAndPaddingLogicalHeight();
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
    auto estimatedHeight = borderAndPaddingLogicalHeight();
    if (shouldApplySizeContainment()) {
        if (auto height = explicitIntrinsicInnerLogicalHeight())
            estimatedHeight += height.value() + scrollbarLogicalHeight();
    }
    LogicalExtentComputedValues computedValues = computeLogicalHeight(estimatedHeight, 0_lu);
    return computedValues.m_extent;
}

template<typename SizeType> std::optional<LayoutUnit> RenderBox::computeLogicalHeightUsingGeneric(const SizeType& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    if (CheckedPtr replaced = dynamicDowncast<RenderReplaced>(*this)) {
        if constexpr (std::same_as<SizeType, Style::MinimumSize>) {
            if (!replaced->replacedMinLogicalHeightComputesAsNone())
                return replaced->computeReplacedLogicalHeightUsing(logicalHeight) + borderAndPaddingLogicalHeight();
        } else if constexpr (std::same_as<SizeType, Style::MaximumSize>) {
            if (!replaced->replacedMaxLogicalHeightComputesAsNone())
                return replaced->computeReplacedLogicalHeightUsing(logicalHeight) + borderAndPaddingLogicalHeight();
        }
        return { };
    }
    if (auto computedContentAndScrollbarLogicalHeight = computeContentAndScrollbarLogicalHeightUsing(logicalHeight, intrinsicContentHeight))
        return adjustBorderBoxLogicalHeightForBoxSizing(*computedContentAndScrollbarLogicalHeight);
    return { };
}

std::optional<LayoutUnit> RenderBox::computeLogicalHeightUsing(const Style::PreferredSize& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    return computeLogicalHeightUsingGeneric(logicalHeight, intrinsicContentHeight);
}

std::optional<LayoutUnit> RenderBox::computeLogicalHeightUsing(const Style::MinimumSize& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    return computeLogicalHeightUsingGeneric(logicalHeight, intrinsicContentHeight);
}

std::optional<LayoutUnit> RenderBox::computeLogicalHeightUsing(const Style::MaximumSize& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    return computeLogicalHeightUsingGeneric(logicalHeight, intrinsicContentHeight);
}

template<typename SizeType> std::optional<LayoutUnit> RenderBox::computeContentLogicalHeightGeneric(const SizeType& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    if (auto computedContentAndScrollbarLogicalHeight = computeContentAndScrollbarLogicalHeightUsing(logicalHeight, intrinsicContentHeight))
        return std::max<LayoutUnit>(0, adjustContentBoxLogicalHeightForBoxSizing(*computedContentAndScrollbarLogicalHeight) - scrollbarLogicalHeight());
    return std::nullopt;
}

std::optional<LayoutUnit> RenderBox::computeContentLogicalHeight(const Style::PreferredSize& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    return computeContentLogicalHeightGeneric(logicalHeight, intrinsicContentHeight);
}

std::optional<LayoutUnit> RenderBox::computeContentLogicalHeight(const Style::MinimumSize& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    return computeContentLogicalHeightGeneric(logicalHeight, intrinsicContentHeight);
}

std::optional<LayoutUnit> RenderBox::computeContentLogicalHeight(const Style::MaximumSize& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    return computeContentLogicalHeightGeneric(logicalHeight, intrinsicContentHeight);
}

std::optional<LayoutUnit> RenderBox::computeContentLogicalHeight(const Style::FlexBasis& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    return computeContentLogicalHeightGeneric(logicalHeight, intrinsicContentHeight);
}

static inline bool isOrthogonal(const RenderBox& renderer, const RenderElement& ancestor)
{
    return renderer.isHorizontalWritingMode() != ancestor.isHorizontalWritingMode();
}

template<typename SizeType> std::optional<LayoutUnit> RenderBox::computeIntrinsicLogicalContentHeightUsingGeneric(const SizeType& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight, LayoutUnit borderAndPadding) const
{
    auto intrinsic = [&] -> std::optional<LayoutUnit> {
        if (intrinsicContentHeight)
            return adjustIntrinsicLogicalHeightForBoxSizing(*intrinsicContentHeight);
        return { };
    };

    auto minMaxContent = [&] -> std::optional<LayoutUnit> {
        // FIXME: The CSS sizing spec is considering changing what min-content/max-content should resolve to.
        // If that happens, this code will have to change.
        if (auto* renderImage = dynamicDowncast<RenderImage>(this)) {
            auto computedFixedLogicalWidth = style().logicalWidth().tryFixed();
            if (computedFixedLogicalWidth && !style().hasAspectRatio()) {
                auto intrinsicRatio = renderImage->intrinsicRatio();
                return resolveHeightForRatio(
                    borderAndPaddingLogicalWidth(),
                    borderAndPaddingLogicalHeight(),
                    LayoutUnit { computedFixedLogicalWidth->resolveZoom(style().usedZoomForLength()) },
                    intrinsicRatio.transposedSize().aspectRatio(),
                    BoxSizing::ContentBox
                );
            }
        }
        return intrinsic();
    };

    auto fillAvailable = [&] -> std::optional<LayoutUnit> {
        return containingBlock()->availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding) - borderAndPadding;
    };

    return WTF::switchOn(logicalHeight,
        [&](const CSS::Keyword::MinContent&) -> std::optional<LayoutUnit> {
            return minMaxContent();
        },
        [&](const CSS::Keyword::MaxContent&) -> std::optional<LayoutUnit> {
            return minMaxContent();
        },
        [&](const CSS::Keyword::Intrinsic&) -> std::optional<LayoutUnit> {
            return intrinsic();
        },
        [&](const CSS::Keyword::MinIntrinsic&) -> std::optional<LayoutUnit> {
            return intrinsic();
        },
        [&](const CSS::Keyword::FitContent&) -> std::optional<LayoutUnit> {
            return intrinsic();
        },
        [&](const CSS::Keyword::WebkitFillAvailable&) -> std::optional<LayoutUnit> {
            return fillAvailable();
        },
        [&](const auto&) -> std::optional<LayoutUnit>  {
            ASSERT_NOT_REACHED();
            return 0_lu;
        }
    );
}

std::optional<LayoutUnit> RenderBox::computeIntrinsicLogicalContentHeightUsing(const Style::PreferredSize& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight, LayoutUnit borderAndPadding) const
{
    return computeIntrinsicLogicalContentHeightUsingGeneric(logicalHeight, intrinsicContentHeight, borderAndPadding);
}

std::optional<LayoutUnit> RenderBox::computeIntrinsicLogicalContentHeightUsing(const Style::MinimumSize& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight, LayoutUnit borderAndPadding) const
{
    return computeIntrinsicLogicalContentHeightUsingGeneric(logicalHeight, intrinsicContentHeight, borderAndPadding);
}

std::optional<LayoutUnit> RenderBox::computeIntrinsicLogicalContentHeightUsing(const Style::MaximumSize& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight, LayoutUnit borderAndPadding) const
{
    return computeIntrinsicLogicalContentHeightUsingGeneric(logicalHeight, intrinsicContentHeight, borderAndPadding);
}

std::optional<LayoutUnit> RenderBox::computeIntrinsicLogicalContentHeightUsing(const Style::FlexBasis& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight, LayoutUnit borderAndPadding) const
{
    return computeIntrinsicLogicalContentHeightUsingGeneric(logicalHeight, intrinsicContentHeight, borderAndPadding);
}

template<typename SizeType> std::optional<LayoutUnit> RenderBox::computeContentAndScrollbarLogicalHeightUsing(const SizeType& logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    auto intrinsic = [&] {
        // FIXME: The CSS sizing spec is considering changing what min-content/max-content should resolve to.
        // If that happens, this code will have to change.
        return computeIntrinsicLogicalContentHeightUsing(logicalHeight, intrinsicContentHeight, borderAndPaddingLogicalHeight());
    };

    return WTF::switchOn(logicalHeight,
        [&](const typename SizeType::Fixed& fixedLogicalHeight) -> std::optional<LayoutUnit> {
            return LayoutUnit { fixedLogicalHeight.resolveZoom(style().usedZoomForLength()) };
        },
        [&](const typename SizeType::Percentage&) -> std::optional<LayoutUnit> {
            return computePercentageLogicalHeight(logicalHeight);
        },
        [&](const typename SizeType::Calc&) -> std::optional<LayoutUnit> {
            return computePercentageLogicalHeight(logicalHeight);
        },
        [&](const CSS::Keyword::MinContent&) -> std::optional<LayoutUnit> {
            return intrinsic();
        },
        [&](const CSS::Keyword::MaxContent&) -> std::optional<LayoutUnit> {
            return intrinsic();
        },
        [&](const CSS::Keyword::FitContent&) -> std::optional<LayoutUnit> {
            return intrinsic();
        },
        [&](const CSS::Keyword::WebkitFillAvailable&) -> std::optional<LayoutUnit> {
            return intrinsic();
        },
        [&](const CSS::Keyword::Intrinsic&) -> std::optional<LayoutUnit> {
            return intrinsic();
        },
        [&](const CSS::Keyword::MinIntrinsic&) -> std::optional<LayoutUnit> {
            return intrinsic();
        },
        [&](const CSS::Keyword::Auto&) -> std::optional<LayoutUnit> {
            if constexpr (std::same_as<SizeType, Style::MinimumSize>) {
                if (intrinsicContentHeight && isFlexItem() && downcast<RenderFlexibleBox>(parent())->shouldApplyMinBlockSizeAutoForFlexItem(*this))
                    return adjustIntrinsicLogicalHeightForBoxSizing(*intrinsicContentHeight);
                return LayoutUnit { 0 };
            } else
                return { };
        },
        [&](const auto&) -> std::optional<LayoutUnit>  {
            return { };
        }
    );
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
    if (containingBlock.isAnonymousForPercentageResolution())
        return containingBlock.style().display() == DisplayType::Block || containingBlock.style().display() == DisplayType::InlineBlock;
    
    // For quirks mode, we skip most auto-height containing blocks when computing
    // percentages.
    return document().inQuirksMode() && !containingBlock.isRenderTableCell() && !containingBlock.isOutOfFlowPositioned() && !containingBlock.isRenderGrid() && !containingBlock.isFlexibleBoxIncludingDeprecated() && containingBlock.style().logicalHeight().isAuto();
}

static bool tableCellShouldHaveZeroInitialSize(const RenderTableCell& tableCell, const RenderBox& child, bool scrollsOverflowY)
{
    // Normally we would let the cell size intrinsically, but scrolling overflow has to be
    // treated differently, since WinIE lets scrolled overflow fragments shrink as needed.
    // While we can't get all cases right, we can at least detect when the cell has a specified
    // height or when the table has a specified height. In these cases we want to initially have
    // no size and allow the flexing of the table or the cell to its specified height to cause us
    // to grow to fill the space. This could end up being wrong in some cases, but it is
    // preferable to the alternative (sizing intrinsically and making the row end up too big).
    if (!scrollsOverflowY)
        return false;
    if (tableCell.style().logicalHeight().isAuto() && tableCell.table()->style().logicalHeight().isAuto())
        return false;
    if (child.isBlockLevelReplacedOrAtomicInline())
        return false;
    if (is<HTMLFormControlElement>(child.element()) && !is<HTMLFieldSetElement>(child.element()))
        return false;
    return true;
}

template<typename SizeType> std::optional<LayoutUnit> RenderBox::computePercentageLogicalHeightGeneric(const SizeType& logicalHeight, UpdatePercentageHeightDescendants updateDescendants) const
{
    bool skippedAutoHeightContainingBlock = false;
    auto* containingBlock = this->containingBlock();
    const RenderBox* containingBlockChild = this;
    LayoutUnit rootMarginBorderPaddingHeight;
    bool isHorizontal = isHorizontalWritingMode();
    while (containingBlock && !is<RenderView>(*containingBlock) && skipContainingBlockForPercentHeightCalculation(*containingBlock, isHorizontal != containingBlock->isHorizontalWritingMode())) {
        if (containingBlock->isBody() || containingBlock->isDocumentElementRenderer())
            rootMarginBorderPaddingHeight += containingBlock->marginBefore() + containingBlock->marginAfter() + containingBlock->borderAndPaddingLogicalHeight();
        skippedAutoHeightContainingBlock = true;
        containingBlockChild = containingBlock;
        containingBlock = containingBlock->containingBlock();
    }
    if (updateDescendants == UpdatePercentageHeightDescendants::Yes)
        containingBlock->addPercentHeightDescendant(const_cast<RenderBox&>(*this));

    if (is<RenderView>(containingBlock) && view().frameView().isAutoSizeEnabled()) {
        // Dynamic height units like percentage don't play well with autosizing when we don't have a definite viewport size. Let's treat percentage as auto instead.
        return { };
    }

    if (isFlexItem() && view().frameView().layoutContext().isPercentHeightResolveDisabledFor(*this))
        return { };

    auto isOrthogonal = isHorizontal != containingBlock->isHorizontalWritingMode();
    auto overridingAvailableSize = std::optional<LayoutUnit> { };
    if (isGridItem()) {
        if (auto gridAreaSize = isOrthogonal ? gridAreaContentLogicalWidth() : gridAreaContentLogicalHeight()) {
            if (!*gridAreaSize)
                return { };
            overridingAvailableSize = *gridAreaSize;
        }
    }
    if (CheckedPtr tableCell = dynamicDowncast<RenderTableCell>(*containingBlock); tableCell && !isOrthogonal) {
        if (skippedAutoHeightContainingBlock)
            return { };
        // Table cells violate what the CSS spec says to do with heights. Basically we
        // don't care if the cell specified a height or not. We just always make ourselves
        // be a percentage of the cell's current content height.
        auto tableCellLogicalHeight = tableCell->overridingBorderBoxLogicalHeight();
        if (!tableCellLogicalHeight)
            return tableCellShouldHaveZeroInitialSize(*tableCell, *this, scrollsOverflowY()) ? std::make_optional(0_lu) : std::nullopt;
        // Note: can't use contentBoxLogicalHeight here on table cells due to intrinsic padding.
        overridingAvailableSize = *tableCellLogicalHeight - tableCell->computedCSSPaddingBefore() - tableCell->computedCSSPaddingAfter() - tableCell->borderLogicalHeight() - tableCell->scrollbarLogicalHeight();
    }

    auto availableHeight = !overridingAvailableSize ? (!isOrthogonal ? containingBlock->availableLogicalHeightForPercentageComputation() : containingBlockChild->containingBlockLogicalWidthForContent()) : overridingAvailableSize;

    if (!availableHeight)
        return { };


    auto result = [&] {
        if constexpr (Style::IsPercentage<SizeType>)
            return Style::evaluate<LayoutUnit>(logicalHeight, *availableHeight - rootMarginBorderPaddingHeight + (isRenderTable() && isOutOfFlowPositioned() ? containingBlock->paddingBefore() + containingBlock->paddingAfter() : 0_lu));
        else
            return Style::evaluate<LayoutUnit>(logicalHeight, *availableHeight - rootMarginBorderPaddingHeight + (isRenderTable() && isOutOfFlowPositioned() ? containingBlock->paddingBefore() + containingBlock->paddingAfter() : 0_lu), style().usedZoomForLength());
    }();

    // |overridingLogicalHeight| is the maximum height made available by the
    // cell to its percent height children when we decide they can determine the
    // height of the cell. If the percent height child is box-sizing:content-box
    // then we must subtract the border and padding from the cell's
    // |availableHeight| (given by |overridingLogicalHeight|) to arrive
    // at the child's computed height.
    bool subtractBorderAndPadding = isRenderTable() || (is<RenderTableCell>(*containingBlock) && !skippedAutoHeightContainingBlock && containingBlock->overridingBorderBoxLogicalHeight() && style().boxSizing() == BoxSizing::ContentBox);
    if (subtractBorderAndPadding) {
        result -= borderAndPaddingLogicalHeight();
        return std::max(0_lu, result);
    }
    return result;
}

std::optional<LayoutUnit> RenderBox::computePercentageLogicalHeight(const Style::PreferredSize& logicalHeight, UpdatePercentageHeightDescendants updateDescendants) const
{
    return computePercentageLogicalHeightGeneric(logicalHeight, updateDescendants);
}

std::optional<LayoutUnit> RenderBox::computePercentageLogicalHeight(const Style::MinimumSize& logicalHeight, UpdatePercentageHeightDescendants updateDescendants) const
{
    return computePercentageLogicalHeightGeneric(logicalHeight, updateDescendants);
}

std::optional<LayoutUnit> RenderBox::computePercentageLogicalHeight(const Style::MaximumSize& logicalHeight, UpdatePercentageHeightDescendants updateDescendants) const
{
    return computePercentageLogicalHeightGeneric(logicalHeight, updateDescendants);
}

std::optional<LayoutUnit> RenderBox::computePercentageLogicalHeight(const Style::FlexBasis& logicalHeight, UpdatePercentageHeightDescendants updateDescendants) const
{
    return computePercentageLogicalHeightGeneric(logicalHeight, updateDescendants);
}

std::optional<LayoutUnit> RenderBox::computePercentageLogicalHeight(const Style::Percentage<CSS::Nonnegative, float>& logicalHeight, UpdatePercentageHeightDescendants updateDescendants) const
{
    return computePercentageLogicalHeightGeneric(logicalHeight, updateDescendants);
}

std::optional<LayoutUnit> RenderBox::computePercentageLogicalHeight(const Style::UnevaluatedCalculation<CSS::LengthPercentage<CSS::NonnegativeUnzoomed, float>>& logicalHeight, UpdatePercentageHeightDescendants updateDescendants) const
{
    return computePercentageLogicalHeightGeneric(logicalHeight, updateDescendants);
}

bool RenderBox::shouldComputePreferredLogicalWidthsFromStyle() const
{
    auto fixedLogicalWidth = overridingLogicalWidthForFlexBasisComputation().value_or(style().logicalWidth()).tryFixed();
    return fixedLogicalWidth && fixedLogicalWidth->isPositiveOrZero() && !(isDeprecatedFlexItem() && !static_cast<int>(fixedLogicalWidth->resolveZoom(style().usedZoomForLength())));
}

void RenderBox::computePreferredLogicalWidths()
{
    ASSERT(needsPreferredLogicalWidthsUpdate());

    computePreferredLogicalWidths(style().logicalMinWidth(), style().logicalMaxWidth(), borderAndPaddingLogicalWidth());
    clearNeedsPreferredWidthsUpdate();
}

void RenderBox::computePreferredLogicalWidths(const Style::MinimumSize& minLogicalWidth, const Style::MaximumSize& maxLogicalWidth, LayoutUnit borderAndPaddingLogicalWidth)
{
    auto usedMaxLogicalWidth = [&] {
        // FIXME: We should be able to handle other values for the max logical width here.
        if (auto fixedMaxLogicalWidth = maxLogicalWidth.tryFixed())
            return adjustContentBoxLogicalWidthForBoxSizing(*fixedMaxLogicalWidth);

        if (maxLogicalWidth.isMinContent()) {
            if (!shouldComputePreferredLogicalWidthsFromStyle())
                return m_minPreferredLogicalWidth;

            return computeIntrinsicLogicalWidthUsing(maxLogicalWidth, contentBoxLogicalWidth(), { });
        }

        return LayoutUnit::max();
    }();

    auto usedMinLogicalWidth = [&]() -> LayoutUnit {
        // FIXME: We should be able to handle other values for the min logical width here.
        if (auto fixedMinLogicalWidth = minLogicalWidth.tryFixed(); fixedMinLogicalWidth && fixedMinLogicalWidth->isPositive())
            return adjustContentBoxLogicalWidthForBoxSizing(*fixedMinLogicalWidth);

        if (minLogicalWidth.isMaxContent())
            return m_maxPreferredLogicalWidth;

        return { };
    }();

    if (!style().logicalWidth().isFixed() && shouldComputeLogicalHeightFromAspectRatio()) {
        auto [transferredMinLogicalWidth, transferredMaxLogicalWidth] = computeMinMaxLogicalWidthFromAspectRatio();
        transferredMinLogicalWidth = std::max(transferredMinLogicalWidth - borderAndPaddingLogicalWidth, 0_lu);
        transferredMaxLogicalWidth = std::max(transferredMaxLogicalWidth - borderAndPaddingLogicalWidth, 0_lu);
        m_minPreferredLogicalWidth = std::clamp(m_minPreferredLogicalWidth, transferredMinLogicalWidth, transferredMaxLogicalWidth);
        m_maxPreferredLogicalWidth = std::clamp(m_maxPreferredLogicalWidth, transferredMinLogicalWidth, transferredMaxLogicalWidth);
    }

    m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, usedMaxLogicalWidth);
    m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, usedMaxLogicalWidth);

    m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, usedMinLogicalWidth);
    m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, usedMinLogicalWidth);

    m_minPreferredLogicalWidth += borderAndPaddingLogicalWidth;
    m_maxPreferredLogicalWidth += borderAndPaddingLogicalWidth;
}

LayoutUnit RenderBox::availableLogicalHeight(AvailableLogicalHeightType heightType) const
{
    return constrainContentBoxLogicalHeightByMinMax(availableLogicalHeightUsing(style().logicalHeight(), heightType), std::nullopt);
}

LayoutUnit RenderBox::availableLogicalHeightUsing(const Style::PreferredSize& logicalHeight, AvailableLogicalHeightType heightType) const
{
    // We need to stop here, since we don't want to increase the height of the table
    // artificially.  We're going to rely on this cell getting expanded to some new
    // height, and then when we lay out again we'll use the calculation below.
    if (isRenderTableCell() && (logicalHeight.isAuto() || logicalHeight.isPercentOrCalculated())) {
        if (auto overridingLogicalHeight = this->overridingBorderBoxLogicalHeight())
            return *overridingLogicalHeight - computedCSSPaddingBefore() - computedCSSPaddingAfter() - borderBefore() - borderAfter() - scrollbarLogicalHeight();
        return this->logicalHeight() - borderAndPaddingLogicalHeight();
    }

    if (auto usedFlexItemOverridingLogicalHeightForPercentageResolutionForFlex = (isFlexItem() ? downcast<RenderFlexibleBox>(*parent()).usedFlexItemOverridingLogicalHeightForPercentageResolution(*this) : std::nullopt))
        return contentBoxLogicalHeight(*usedFlexItemOverridingLogicalHeightForPercentageResolutionForFlex);

    if (shouldComputeLogicalHeightFromAspectRatio()) {
        auto borderAndPaddingLogicalHeight = this->borderAndPaddingLogicalHeight();
        auto borderBoxLogicalHeight = blockSizeFromAspectRatio(borderAndPaddingLogicalWidth(), borderAndPaddingLogicalHeight, style().logicalAspectRatio(), style().boxSizingForAspectRatio(), logicalWidth(), style().aspectRatio(), isRenderReplaced());
        if (heightType == AvailableLogicalHeightType::ExcludeMarginBorderPadding)
            return borderBoxLogicalHeight - borderAndPaddingLogicalHeight;
        return borderBoxLogicalHeight;
    }

    if (logicalHeight.isPercentOrCalculated() && isOutOfFlowPositioned() && !isRenderFragmentedFlow()) {
        PositionedLayoutConstraints constraints(*this, LogicalBoxAxis::Block);
        return adjustContentBoxLogicalHeightForBoxSizing(Style::evaluate<LayoutUnit>(logicalHeight, constraints.containingSize(), style().usedZoomForLength()));
    }

    if (auto computedContentAndScrollbarLogicalHeight = computeContentAndScrollbarLogicalHeightUsing(logicalHeight, std::nullopt))
        return std::max<LayoutUnit>(0, adjustContentBoxLogicalHeightForBoxSizing(computedContentAndScrollbarLogicalHeight) - scrollbarLogicalHeight());

    // Height of absolutely positioned, non-replaced elements section 5.3 rule 5
    // https://www.w3.org/TR/css-position-3/#abs-non-replaced-height
    if (CheckedPtr block = dynamicDowncast<RenderBlock>(*this); block && isOutOfFlowPositioned() && style().logicalHeight().isAuto() && !(style().logicalTop().isAuto() || style().logicalBottom().isAuto())) {
        auto computedValues = block->computeLogicalHeight(block->logicalHeight(), 0);
        return computedValues.m_extent - block->borderAndPaddingLogicalHeight() - block->scrollbarLogicalHeight();
    }

    LayoutUnit availableHeight = isOrthogonal(*this, *containingBlock()) ? containingBlockLogicalWidthForContent() : containingBlockLogicalHeightForContent(heightType);
    if (heightType == AvailableLogicalHeightType::ExcludeMarginBorderPadding) {
        // FIXME: Margin collapsing hasn't happened yet, so this incorrectly removes collapsed margins.
        availableHeight -= marginBefore() + marginAfter() + borderAndPaddingLogicalHeight();
    }
    return availableHeight;
}

void RenderBox::computeBlockDirectionMargins(const RenderBlock& containingBlock, LayoutUnit& marginBefore, LayoutUnit& marginAfter) const
{
    // First assert that we're not calling this method on box types that don't support margins.
    ASSERT(!isRenderTableCell());
    ASSERT(!isRenderTableRow());
    ASSERT(!isRenderTableSection());
    ASSERT(!isRenderTableCol());

    // Margins are calculated with respect to the logical width of
    // the containing block (8.3)
    LayoutUnit cw = containingBlockLogicalWidthForContent();
    marginBefore = constrainBlockMarginInAvailableSpaceOrTrim(containingBlock, cw, Style::MarginTrimSide::BlockStart);
    marginAfter = constrainBlockMarginInAvailableSpaceOrTrim(containingBlock, cw, Style::MarginTrimSide::BlockEnd);
}

void RenderBox::computeAndSetBlockDirectionMargins(const RenderBlock& containingBlock)
{
    LayoutUnit marginBefore;
    LayoutUnit marginAfter;
    computeBlockDirectionMargins(containingBlock, marginBefore, marginAfter);
    containingBlock.setMarginBeforeForChild(*this, marginBefore);
    containingBlock.setMarginAfterForChild(*this, marginAfter);
}

LayoutUnit RenderBox::constrainBlockMarginInAvailableSpaceOrTrim(const RenderBox& containingBlock, LayoutUnit availableSpace, Style::MarginTrimSide marginSide) const
{
    
    ASSERT(marginSide == Style::MarginTrimSide::BlockStart || marginSide == Style::MarginTrimSide::BlockEnd);
    if (containingBlock.shouldTrimChildMargin(marginSide, *this)) {
        // FIXME(255434): This should be set when the margin is being trimmed
        // within the context of its layout system (block, flex, grid) and should not 
        // be done at this level within RenderBox. We should be able to leave the 
        // trimming responsibility to each of those contexts and not need to
        // do any of it here (trimming the margin and setting the rare data bit)
        if (isGridItem())
            const_cast<RenderBox&>(*this).markMarginAsTrimmed(marginSide);
        return 0_lu;
    }
    
    return marginSide == Style::MarginTrimSide::BlockStart
        ? Style::evaluateMinimum<LayoutUnit>(style().marginBefore(containingBlock.writingMode()), availableSpace, style().usedZoomForLength())
        : Style::evaluateMinimum<LayoutUnit>(style().marginAfter(containingBlock.writingMode()), availableSpace, style().usedZoomForLength());
}

// MARK: - Positioned Layout

LayoutRange RenderBox::containingBlockRangeForPositioned(const RenderBoxModelObject& container, BoxAxis physicalAxis) const
{
    ASSERT(container.canContainAbsolutelyPositionedObjects() || container.canContainFixedPositionObjects());

    bool isContainerInlineAxis = physicalAxis == container.writingMode().inlineAxis();
    auto startEdge = isContainerInlineAxis ? container.borderLogicalLeft() : container.borderBefore();

    if (isFixedPositioned()) {
        if (auto* renderView = dynamicDowncast<RenderView>(container)) {
            return isContainerInlineAxis
                ? LayoutRange(startEdge, renderView->clientLogicalWidthForFixedPosition())
                : LayoutRange(startEdge, renderView->clientLogicalHeightForFixedPosition());
        }
    }

    // Inline containing blocks are formed by relatively-positioned inline boxes.
    CheckedPtr<const RenderInline> inlineContainer;
    if ((inlineContainer = container.inlineContinuation())) {
        auto relativelyPositionedInlineBoxAncestor = [&] {
            CheckedPtr<const RenderElement> ancestor = inlineContainer;
            for (; ancestor && !ancestor->isRelativelyPositioned(); ancestor = ancestor->parent()) { }
            return ancestor ? dynamicDowncast<RenderInline>(ancestor.get()) : nullptr;
        }();
        // Since we stop splitting inlines over 200 nested boxes (see RenderTreeBuilder::Inline::splitInlines), we may not be able to find the real containing block here.
        if (relativelyPositionedInlineBoxAncestor)
            inlineContainer = relativelyPositionedInlineBoxAncestor;
    } else
        inlineContainer = dynamicDowncast<RenderInline>(container);
    if (inlineContainer) {
        return isContainerInlineAxis
            ? LayoutRange(startEdge, inlineContainer->innerPaddingBoxWidth())
            : LayoutRange(startEdge, inlineContainer->innerPaddingBoxHeight());
    }

    auto* containingBlock = dynamicDowncast<RenderBlock>(container) ? : container.containingBlock();
    if (!containingBlock) {
        ASSERT_NOT_REACHED();
        return { };
    }

    if (auto* fragmentedFlow = dynamicDowncast<RenderFragmentedFlow>(containingBlock)) {
        return isContainerInlineAxis
            ? LayoutRange(startEdge, fragmentedFlow->contentLogicalWidthOfFirstFragment())
            : LayoutRange(startEdge, fragmentedFlow->contentLogicalHeightOfFirstFragment());
    }

    if (enclosingFragmentedFlow() && enclosingFragmentedFlow()->writingMode().inlineAxis() == physicalAxis) {
        LayoutUnit pageOffset = containingBlock->offsetFromLogicalTopOfFirstPage();
        if (RenderFragmentContainer* fragment = containingBlock->fragmentAtBlockOffset(pageOffset)) {
            if (auto boxInfo = containingBlock->renderBoxFragmentInfo(fragment)) {
                auto size = boxInfo->logicalWidth();
                if (BoxAxis::Horizontal == physicalAxis)
                    size -= containingBlock->width() - containingBlock->clientWidth();
                else
                    size -= containingBlock->height() - containingBlock->clientHeight();
                return LayoutRange(startEdge, std::max<LayoutUnit>(0, size));
            }
        }
    }

    return BoxAxis::Horizontal == physicalAxis
        ? LayoutRange(startEdge, containingBlock->clientWidth())
        : LayoutRange(startEdge, containingBlock->clientHeight());
}

void RenderBox::computeOutOfFlowPositionedLogicalWidth(LogicalExtentComputedValues& computedValues) const
{
    ASSERT(isOutOfFlowPositioned());

    if (CheckedPtr replaced = dynamicDowncast<RenderReplaced>(this))
        return replaced->computeReplacedOutOfFlowPositionedLogicalWidth(computedValues);

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
    // (block-style-comments in this function and in computeOutOfFlowPositionedLogicalWidthUsing()
    // correspond to text from the spec)

    PositionedLayoutConstraints inlineConstraints(*this, LogicalBoxAxis::Inline);
    inlineConstraints.computeInsets();

    // Calculate the used width. See CSS2 § 10.3.7.
    auto& styleToUse = style();
    auto usedWidth = computeOutOfFlowPositionedLogicalWidthUsing(styleToUse.logicalWidth(), inlineConstraints);

    LayoutUnit transferredMinSize = LayoutUnit::min();
    LayoutUnit transferredMaxSize = LayoutUnit::max();
    if (shouldComputeLogicalHeightFromAspectRatio())
        std::tie(transferredMinSize, transferredMaxSize) = computeMinMaxLogicalWidthFromAspectRatio();

    // Clamp by max-width.
    auto usedMaxWidth = LayoutUnit::max();
    if (auto& logicalMaxWidth = styleToUse.logicalMaxWidth(); !logicalMaxWidth.isNone())
        usedMaxWidth = computeOutOfFlowPositionedLogicalWidthUsing(logicalMaxWidth, inlineConstraints);
    if (transferredMaxSize < usedMaxWidth)
        usedMaxWidth = computeOutOfFlowPositionedLogicalWidthUsing(Style::MaximumSize { Style::MaximumSize::Fixed { transferredMaxSize } }, inlineConstraints);
    if (usedWidth > usedMaxWidth)
        usedWidth = usedMaxWidth;

    // Clamp by min-width.
    auto usedMinWidth = LayoutUnit::min();
    if (auto& logicalMinWidth = styleToUse.logicalMinWidth(); !logicalMinWidth.isKnownZero() || logicalMinWidth.isIntrinsic())
        usedMinWidth = computeOutOfFlowPositionedLogicalWidthUsing(logicalMinWidth, inlineConstraints);
    if (transferredMinSize > usedMinWidth)
        usedMinWidth = computeOutOfFlowPositionedLogicalWidthUsing(Style::MinimumSize { Style::MinimumSize::Fixed { transferredMinSize } }, inlineConstraints);
    if (usedWidth < usedMinWidth)
        usedWidth = usedMinWidth;

    // Set the final width value.
    computedValues.m_extent = usedWidth + inlineConstraints.bordersPlusPadding();

    // Calculate the position.
    inlineConstraints.resolvePosition(computedValues);
    inlineConstraints.fixupLogicalLeftPosition(computedValues);

    // Adjust logicalLeft if we need to for the flipped version of our writing mode in fragments.
    // FIXME: Add support for other types of objects as containerBlock, not only RenderBlock.
    if (enclosingFragmentedFlow() && isWritingModeRoot() && inlineConstraints.isOrthogonal()) {
        if (CheckedPtr container = dynamicDowncast<RenderBlock>(inlineConstraints.container())) {
            ASSERT(inlineConstraints.container().canHaveBoxInfoInFragment());
            LayoutUnit logicalLeftPos = computedValues.m_position;
            LayoutUnit cbPageOffset = container->offsetFromLogicalTopOfFirstPage();
            RenderFragmentContainer* cbFragment = container->fragmentAtBlockOffset(cbPageOffset);
            if (cbFragment) {
                RenderBoxFragmentInfo* boxInfo = container->renderBoxFragmentInfo(cbFragment);
                if (boxInfo) {
                    logicalLeftPos += boxInfo->logicalLeft();
                    computedValues.m_position = logicalLeftPos;
                }
            }
        }
    }
}

template<typename SizeType> LayoutUnit RenderBox::computeOutOfFlowPositionedLogicalWidthUsing(const SizeType& logicalWidth, const PositionedLayoutConstraints& inlineConstraints) const
{
    auto fallback = [&] -> LayoutUnit {
        bool shrinkToFit = inlineConstraints.insetFitsContent() || !inlineConstraints.alignmentAppliesStretch(ItemPosition::Stretch);
        if (shrinkToFit) {
            auto preferredWidth = maxPreferredLogicalWidth() - inlineConstraints.bordersPlusPadding();
            auto preferredMinWidth = minPreferredLogicalWidth() - inlineConstraints.bordersPlusPadding();
            return std::min(std::max(preferredMinWidth, inlineConstraints.availableContentSpace()), preferredWidth);
        }
        return std::max(0_lu, inlineConstraints.availableContentSpace());
    };

    auto intrinsic = [&](const auto& keyword) -> LayoutUnit {
        auto availableSpace = inlineConstraints.containingSize();
        availableSpace -= inlineConstraints.insetBeforeValue();
        availableSpace -= inlineConstraints.insetAfterValue();
        return std::max(0_lu, computeIntrinsicLogicalWidthUsing(keyword, availableSpace, inlineConstraints.bordersPlusPadding()) - inlineConstraints.bordersPlusPadding());
    };

    return WTF::switchOn(logicalWidth,
        [&](const typename SizeType::Fixed& fixedLogicalWidth) -> LayoutUnit {
            return adjustContentBoxLogicalWidthForBoxSizing(fixedLogicalWidth);
        },
        [&](const typename SizeType::Percentage& percentageLogicalWidth) -> LayoutUnit {
            return adjustContentBoxLogicalWidthForBoxSizing(Style::evaluate<LayoutUnit>(percentageLogicalWidth, inlineConstraints.containingSize()));
        },
        [&](const typename SizeType::Calc& calculatedLogicalWidth) -> LayoutUnit {
            return adjustContentBoxLogicalWidthForBoxSizing(Style::evaluate<LayoutUnit>(calculatedLogicalWidth, inlineConstraints.containingSize(), style().usedZoomForLength()));
        },
        [&](const CSS::Keyword::FitContent& keyword) -> LayoutUnit {
            return intrinsic(keyword);
        },
        [&](const CSS::Keyword::WebkitFillAvailable& keyword) -> LayoutUnit {
            return intrinsic(keyword);
        },
        [&](const CSS::Keyword::MinContent& keyword) -> LayoutUnit {
            return intrinsic(keyword);
        },
        [&](const CSS::Keyword::MaxContent& keyword) -> LayoutUnit {
            return intrinsic(keyword);
        },
        [&](const CSS::Keyword::Intrinsic&) -> LayoutUnit {
            if (shouldComputeLogicalWidthFromAspectRatio())
                return inlineConstraints.containingSize();
            return fallback();
        },
        [&](const CSS::Keyword::MinIntrinsic&) -> LayoutUnit {
            if (shouldComputeLogicalWidthFromAspectRatio())
                return inlineConstraints.containingSize();
            return fallback();
        },
        [&](const CSS::Keyword::Auto&) -> LayoutUnit {
            if constexpr (std::same_as<SizeType, Style::MinimumSize>) {
                if (shouldComputeLogicalWidthFromAspectRatio()) {
                    LayoutUnit minLogicalWidth;
                    LayoutUnit maxLogicalWidth;
                    computeIntrinsicLogicalWidths(minLogicalWidth, maxLogicalWidth);
                    return minLogicalWidth;
                }
                return 0_lu;
            } else {
                if (shouldComputeLogicalWidthFromAspectRatio())
                    return computeLogicalWidthFromAspectRatio();
                return fallback();
            }
        },
        [&](const CSS::Keyword::None&) -> LayoutUnit {
            return inlineConstraints.containingSize();
        }
    );
}

void RenderBox::computeOutOfFlowPositionedLogicalHeight(LogicalExtentComputedValues& computedValues) const
{
    ASSERT(isOutOfFlowPositioned());

    if (CheckedPtr replaced = dynamicDowncast<RenderReplaced>(this))
        return replaced->computeReplacedOutOfFlowPositionedLogicalHeight(computedValues);

    PositionedLayoutConstraints blockConstraints(*this, LogicalBoxAxis::Block);
    blockConstraints.computeInsets();

    // Calculate the used height. See CSS2 § 10.6.4.
    auto& styleToUse = style();
    LayoutUnit computedHeight = computedValues.m_extent;
    LayoutUnit usedHeight = computeOutOfFlowPositionedLogicalHeightUsing(styleToUse.logicalHeight(), computedHeight, blockConstraints);

    // Clamp by max height.
    if (auto logicalMaxHeight = styleToUse.logicalMaxHeight(); !logicalMaxHeight.isNone()) {
        auto usedMaxHeight = computeOutOfFlowPositionedLogicalHeightUsing(logicalMaxHeight, computedHeight, blockConstraints);
        if (usedHeight > usedMaxHeight)
            usedHeight = usedMaxHeight;
    }

    // Clamp by min height.
    if (auto& logicalMinHeight = styleToUse.logicalMinHeight(); logicalMinHeight.isAuto() || !logicalMinHeight.isKnownZero() || logicalMinHeight.isIntrinsic()) {
        auto usedMinHeight = computeOutOfFlowPositionedLogicalHeightUsing(logicalMinHeight, computedHeight, blockConstraints);
        if (usedHeight < usedMinHeight)
            usedHeight = usedMinHeight;
    }

    // Set the final height value.
    computedValues.m_extent = usedHeight + blockConstraints.bordersPlusPadding();

    // Calculate the position.
    blockConstraints.resolvePosition(computedValues);
    blockConstraints.adjustLogicalTopWithLogicalHeightIfNeeded(computedValues);

    // Adjust logicalTop if we need to for perpendicular writing modes in fragments.
    // FIXME: Add support for other types of objects as containerBlock, not only RenderBlock.
    if (enclosingFragmentedFlow() && blockConstraints.isOrthogonal()) {
        if (CheckedPtr container = dynamicDowncast<RenderBlock>(blockConstraints.container())) {
            ASSERT(blockConstraints.container().canHaveBoxInfoInFragment());
            LayoutUnit logicalTopPos = computedValues.m_position;
            LayoutUnit cbPageOffset = container->offsetFromLogicalTopOfFirstPage() - logicalLeft();
            RenderFragmentContainer* cbFragment = container->fragmentAtBlockOffset(cbPageOffset);
            if (cbFragment) {
                RenderBoxFragmentInfo* boxInfo = container->renderBoxFragmentInfo(cbFragment);
                if (boxInfo) {
                    logicalTopPos += boxInfo->logicalLeft();
                    computedValues.m_position = logicalTopPos;
                }
            }
        }
    }
}

LayoutUnit RenderBox::computeOutOfFlowPositionedLogicalHeightUsing(const Style::PreferredSize& logicalHeight, LayoutUnit computedHeight, const PositionedLayoutConstraints& blockConstraints) const
{
    auto contentLogicalHeight = computedHeight - blockConstraints.bordersPlusPadding();

    // Height is never unsolved for tables.
    if (isRenderTable())
        return contentLogicalHeight;

    bool fromAspectRatio = shouldComputeLogicalHeightFromAspectRatio();
    bool logicalHeightIsAuto = logicalHeight.isAuto() && !fromAspectRatio;

    if (!logicalHeightIsAuto) {
        if (logicalHeight.isIntrinsic())
            return adjustContentBoxLogicalHeightForBoxSizing(computeIntrinsicLogicalContentHeightUsing(logicalHeight, contentLogicalHeight, blockConstraints.bordersPlusPadding()).value_or(0_lu));
        if (fromAspectRatio) {
            auto resolvedLogicalHeight = blockSizeFromAspectRatio(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent(), style().logicalAspectRatio(), style().boxSizingForAspectRatio(), logicalWidth(), style().aspectRatio(), isRenderReplaced());
            return std::max(LayoutUnit(), resolvedLogicalHeight - blockConstraints.bordersPlusPadding());
        }
        return adjustContentBoxLogicalHeightForBoxSizing(Style::evaluate<LayoutUnit>(logicalHeight, blockConstraints.containingSize(), style().usedZoomForLength()));
    }

    bool shrinkToFit = blockConstraints.insetFitsContent() || !blockConstraints.alignmentAppliesStretch(ItemPosition::Stretch);
    if (!shrinkToFit)
        return std::max<LayoutUnit>(0, blockConstraints.availableContentSpace());

    return contentLogicalHeight;
}

LayoutUnit RenderBox::computeOutOfFlowPositionedLogicalHeightUsing(const Style::MinimumSize& originalLogicalHeight, LayoutUnit computedHeight, const PositionedLayoutConstraints& blockConstraints) const
{
    auto contentLogicalHeight = computedHeight - blockConstraints.bordersPlusPadding();

    // Height is never unsolved for tables.
    if (isRenderTable())
        return contentLogicalHeight;

    auto logicalHeight = originalLogicalHeight;

    if (logicalHeight.isAuto()) {
        if (shouldComputeLogicalHeightFromAspectRatio())
            logicalHeight = Style::MinimumSize::Fixed { computedHeight };
        else
            logicalHeight = 0_css_px;
    }

    if (logicalHeight.isIntrinsic())
        return adjustContentBoxLogicalHeightForBoxSizing(computeIntrinsicLogicalContentHeightUsing(logicalHeight, contentLogicalHeight, blockConstraints.bordersPlusPadding()).value_or(0_lu));
    return adjustContentBoxLogicalHeightForBoxSizing(Style::evaluate<LayoutUnit>(logicalHeight, blockConstraints.containingSize(), style().usedZoomForLength()));
}

LayoutUnit RenderBox::computeOutOfFlowPositionedLogicalHeightUsing(const Style::MaximumSize& logicalHeight, LayoutUnit computedHeight, const PositionedLayoutConstraints& blockConstraints) const
{
    auto contentLogicalHeight = computedHeight - blockConstraints.bordersPlusPadding();

    // Height is never unsolved for tables.
    if (isRenderTable())
        return contentLogicalHeight;

    if (logicalHeight.isIntrinsic())
        return adjustContentBoxLogicalHeightForBoxSizing(computeIntrinsicLogicalContentHeightUsing(logicalHeight, contentLogicalHeight, blockConstraints.bordersPlusPadding()).value_or(0_lu));
    return adjustContentBoxLogicalHeightForBoxSizing(Style::evaluate<LayoutUnit>(logicalHeight, blockConstraints.containingSize(), style().usedZoomForLength()));
}

PositionWithAffinity RenderBox::positionForPoint(const LayoutPoint& point, HitTestSource source, const RenderFragmentContainer* fragment)
{
    // no children...return this render object's element, if there is one, and offset 0
    if (!firstChild())
        return createPositionWithAffinity(nonPseudoElement() ? firstPositionInOrBeforeNode(nonPseudoElement()) : Position());

    if (isRenderTable() && nonPseudoElement()) {
        LayoutUnit right = contentBoxWidth() + horizontalBorderAndPaddingExtent();
        LayoutUnit bottom = contentBoxHeight() + verticalBorderAndPaddingExtent();
        
        if (point.x() < 0 || point.x() > right || point.y() < 0 || point.y() > bottom) {
            if (point.x() <= right / 2)
                return createPositionWithAffinity(firstPositionInOrBeforeNode(nonPseudoElement()));
            return createPositionWithAffinity(lastPositionInOrAfterNode(nonPseudoElement()));
        }
    }

    // Pass off to the closest child.
    LayoutUnit minDist = LayoutUnit::max();
    RenderBox* closestRenderer = nullptr;
    LayoutPoint adjustedPoint = point;
    if (isRenderTableRow())
        adjustedPoint.moveBy(location());

    for (auto& renderer : childrenOfType<RenderBox>(*this)) {
        if (CheckedPtr fragmentedFlow = dynamicDowncast<RenderFragmentedFlow>(*this)) {
            ASSERT(fragment || fragmentedFlow->isSkippedContent());
            if (!fragmentedFlow->objectShouldFragmentInFlowFragment(&renderer, fragment))
                continue;
        }

        if ((!renderer.firstChild() && !renderer.isInline() && !is<RenderBlockFlow>(renderer))
            || (source == HitTestSource::Script ? renderer.style().visibility() : renderer.style().usedVisibility()) != Visibility::Visible)
            continue;

        LayoutUnit top = renderer.borderTop() + renderer.paddingTop() + (is<RenderTableRow>(*this) ? 0_lu : renderer.y());
        LayoutUnit bottom = top + renderer.contentBoxHeight();
        LayoutUnit left = renderer.borderLeft() + renderer.paddingLeft() + (is<RenderTableRow>(*this) ? 0_lu : renderer.x());
        LayoutUnit right = left + renderer.contentBoxWidth();
        
        if (point.x() <= right && point.x() >= left && point.y() <= top && point.y() >= bottom) {
            if (is<RenderTableRow>(renderer))
                return renderer.positionForPoint(point + adjustedPoint - renderer.locationOffset(), source, fragment);
            return renderer.positionForPoint(point - renderer.locationOffset(), source, fragment);
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
        return closestRenderer->positionForPoint(adjustedPoint - closestRenderer->locationOffset(), source, fragment);
    
    return createPositionWithAffinity(firstPositionInOrBeforeNode(nonPseudoElement()));
}

bool RenderBox::shrinkToAvoidFloats() const
{
    // Floating objects don't shrink. Objects that don't avoid floats don't shrink. Non-inline box type of inline level elements don't shrink.
    if (isInline() || isFloating() || !avoidsFloats())
        return false;

    // Only auto width objects can possibly shrink to avoid floats.
    return style().width().isAuto();
}

bool RenderBox::avoidsFloats() const
{
    if (is<RenderReplaced>(*this) || isLegend() || (element() && element()->isFormControlElement()))
        return true;

#if ENABLE(MATHML)
    if (is<RenderMathMLBlock>(*this))
        return true;
#endif

    if (CheckedPtr renderBlock = dynamicDowncast<RenderBlock>(*this))
        return renderBlock->createsNewFormattingContext();

    return false;
}

void RenderBox::addVisualEffectOverflow()
{
    bool hasBoxShadow = style().hasBoxShadow();
    bool hasBorderImageOutsets = style().hasBorderImageOutsets();
    bool hasOutline = outlineStyleForRepaint().hasOutlineInVisualOverflow();
    if (!hasBoxShadow && !hasBorderImageOutsets && !hasOutline)
        return;

    addVisualOverflow(applyVisualEffectOverflow(borderBoxRect()));

    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow())
        fragmentedFlow->addFragmentsVisualEffectOverflow(*this);
}

static void convertOutsetsToOverflowCoordinates(LayoutBoxExtent& outsets, WritingMode writingMode)
{
    switch (writingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
    case FlowDirection::LeftToRight:
        break;
    case FlowDirection::BottomToTop:
        std::swap(outsets.top(), outsets.bottom());
        break;
    case FlowDirection::RightToLeft:
        std::swap(outsets.left(), outsets.right());
        break;
    }
}

LayoutRect RenderBox::applyVisualEffectOverflow(const LayoutRect& borderBox) const
{
    LayoutUnit overflowMinX = borderBox.x();
    LayoutUnit overflowMaxX = borderBox.maxX();
    LayoutUnit overflowMinY = borderBox.y();
    LayoutUnit overflowMaxY = borderBox.maxY();
    
    // Compute box-shadow overflow first.
    if (style().hasBoxShadow()) {
        auto shadowOutsets = Style::shadowOutsetExtent(style().boxShadow(), style().usedZoomForLength());
        // Box-shadow extent's left and top are negative when extends to left and top, respectively, so negate to convert to outsets.
        shadowOutsets.left() = -shadowOutsets.left();
        shadowOutsets.top() = -shadowOutsets.top();
        convertOutsetsToOverflowCoordinates(shadowOutsets, writingMode());

        overflowMinX = borderBox.x() - shadowOutsets.left();
        overflowMaxX = borderBox.maxX() + shadowOutsets.right();
        overflowMinY = borderBox.y() - shadowOutsets.top();
        overflowMaxY = borderBox.maxY() + shadowOutsets.bottom();
    }

    // Now compute border-image-outset overflow.
    if (style().hasBorderImageOutsets()) {
        auto borderOutsets = style().borderImageOutsets();
        convertOutsetsToOverflowCoordinates(borderOutsets, writingMode());

        overflowMinX = std::min(overflowMinX, borderBox.x() - borderOutsets.left());
        overflowMaxX = std::max(overflowMaxX, borderBox.maxX() + borderOutsets.right());
        overflowMinY = std::min(overflowMinY, borderBox.y() - borderOutsets.top());
        overflowMaxY = std::max(overflowMaxY, borderBox.maxY() + borderOutsets.bottom());
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

void RenderBox::addOverflowFromInFlowChildren(OptionSet<ComputeOverflowOptions> options)
{
    for (auto& child : childrenOfType<RenderBox>(*this)) {
        if (!child.isFloatingOrOutOfFlowPositioned())
            addOverflowFromContainedBox(child, options);
    }
}

void RenderBox::addOverflowFromContainedBox(const RenderBox& child, OptionSet<ComputeOverflowOptions> options)
{
    ASSERT(!child.isFloating());
    addOverflowWithRendererOffset(child, child.locationOffset(), options);
}

void RenderBox::addOverflowFromFloatBox(const FloatingObject& floatBox)
{
    if (!floatBox.renderer())
        return;
    addOverflowWithRendererOffset(*floatBox.renderer(), floatBox.locationOffsetOfBorderBox());
}

// 'offsetFromThis' is normally the renderer's position (RenderBox::location()).
// However in case of a float box, it may belong to a different container
// (but is intrusive to this container hence calling this function),
// meaning that the renderer's position may not be the same as its actual offset from this container.
void RenderBox::addOverflowWithRendererOffset(const RenderBox& renderer, LayoutSize offsetFromThis, OptionSet<ComputeOverflowOptions> options)
{
    UNUSED_PARAM(options);

    // Never allow flow threads to propagate overflow up to a parent.
    if (renderer.isRenderFragmentedFlow())
        return;

    CheckedPtr fragmentedFlow = enclosingFragmentedFlow();
    if (fragmentedFlow)
        fragmentedFlow->addFragmentsOverflowFromChild(*this, renderer, offsetFromThis);

    // Only propagate layout overflow from the child if the child isn't clipping its overflow.  If it is, then
    // its overflow is internal to it, and we don't care about it. layoutOverflowRectForPropagation takes care of this
    // and just propagates the border box rect instead.
    auto childLayoutOverflowRect = renderer.layoutOverflowRectForPropagation(writingMode());
    childLayoutOverflowRect.move(offsetFromThis);
    addLayoutOverflow(childLayoutOverflowRect);

    auto ensurePaddingEndIsIncluded = [&] {
        if (!hasNonVisibleOverflow())
            return;

        // As per https://github.com/w3c/csswg-drafts/issues/3653 padding should contribute to the scrollable overflow area.
        if (!paddingEnd())
            return;

        // FIXME: Expand it to non-grid/flex cases when applicable.
        if (!is<RenderGrid>(*this) && !is<RenderFlexibleBox>(*this))
            return;

        if (renderer.isOutOfFlowPositioned())
            return;

        // Note that we can't use childLayoutOverflowRect here as it already includes propagated overflow from descendents.
        auto isMainAxisFlipped = [&] {
            auto isInlineFlipped = writingMode().isInlineFlipped();
            if (CheckedPtr flexContainer = dynamicDowncast<RenderFlexibleBox>(*this)) {
                auto isColumnOrRowReverseSameAsWrapReverse = flexContainer->isColumnOrRowReverse() == flexContainer->isWrapReverse();
                return isInlineFlipped == isColumnOrRowReverseSameAsWrapReverse;
            }
            return isInlineFlipped;
        }();
        auto childLogicalRight = [&] {
            if (!isMainAxisFlipped)
                return (isHorizontalWritingMode() ? offsetFromThis.width() + renderer.width() : offsetFromThis.height() + renderer.height()) + renderer.marginEnd(writingMode());
            return (isHorizontalWritingMode() ? offsetFromThis.width() : offsetFromThis.height()) - renderer.marginEnd(writingMode());
        };

        auto layoutOverflowRect = this->layoutOverflowRect();
        if (!isMainAxisFlipped) {
            auto layoutOverflowLogicalRightIncludingPaddingEnd = childLogicalRight() + paddingEnd();
            isHorizontalWritingMode() ? layoutOverflowRect.shiftMaxXEdgeTo(layoutOverflowLogicalRightIncludingPaddingEnd) : layoutOverflowRect.shiftMaxYEdgeTo(layoutOverflowLogicalRightIncludingPaddingEnd);
        } else {
            auto layoutOverflowLogicalRightIncludingPaddingEnd = childLogicalRight() - paddingEnd();
            isHorizontalWritingMode() ? layoutOverflowRect.shiftXEdgeTo(layoutOverflowLogicalRightIncludingPaddingEnd) : layoutOverflowRect.shiftYEdgeTo(layoutOverflowLogicalRightIncludingPaddingEnd);
        }
        addLayoutOverflow(layoutOverflowRect);
    };
    ensurePaddingEndIsIncluded();

    if (paintContainmentApplies())
        return;

    // Add in visual overflow from the child. Even if the child clips its overflow, it may still
    // have visual overflow of its own set from box shadows or reflections. It is unnecessary to propagate this
    // overflow if we are clipping our own overflow.
    if (hasPotentiallyScrollableOverflow())
        return;

    auto childVisualOverflowRect = std::optional<LayoutRect> { };
    auto computeChildVisualOverflowRect = [&] () {
        childVisualOverflowRect = renderer.visualOverflowRectForPropagation(writingMode());
        childVisualOverflowRect->move(offsetFromThis);
    };
    // If this block is flowed inside a flow thread, make sure its overflow is propagated to the containing fragments.
    if (fragmentedFlow) {
        computeChildVisualOverflowRect();
        fragmentedFlow->addFragmentsVisualOverflow(*this, *childVisualOverflowRect);
    } else {
        // Update our visual overflow in case the child spills out the block, but only if we were going to paint
        // the child block ourselves.
        if (renderer.hasSelfPaintingLayer())
            return;
    }
    if (!childVisualOverflowRect)
        computeChildVisualOverflowRect();
    addVisualOverflow(*childVisualOverflowRect);
}

LayoutOptionalOutsets RenderBox::allowedLayoutOverflow() const
{
    LayoutOptionalOutsets allowance;

    // Overflow is in the block's coordinate space and thus is flipped
    // for horizontal-bt and vertical-rl writing modes. This means we can
    // treat horizontal-tb/bt as the same and vertical-lr/rl as the same.

    if (writingMode().isHorizontal()) {
        allowance.top() = 0_lu;
        if (writingMode().isInlineLeftToRight())
            allowance.left() = 0_lu;
        else
            allowance.right() = 0_lu;
    } else {
        allowance.left() = 0_lu;
        if (writingMode().isInlineTopToBottom())
            allowance.top() = 0_lu;
        else
            allowance.bottom() = 0_lu;
    }

    return allowance;
}

void RenderBox::addLayoutOverflow(const LayoutRect& rect)
{
    auto clientBox = flippedClientBoxRect();
    if (clientBox.contains(rect) || rect.isEmpty())
        return;

    // For overflow clip objects, we don't want to propagate overflow into unreachable areas.
    LayoutRect overflowRect(rect);
    if (hasPotentiallyScrollableOverflow() || isRenderView()) {
        LayoutOptionalOutsets allowance = allowedLayoutOverflow();
        // Non-negative values indicate a limit, let's apply them.
        if (allowance.top())
            overflowRect.shiftYEdgeTo(std::max(overflowRect.y(), clientBox.y() - *allowance.top()));
        if (allowance.bottom())
            overflowRect.shiftMaxYEdgeTo(std::min(overflowRect.maxY(), clientBox.maxY() + *allowance.bottom()));
        if (allowance.left())
            overflowRect.shiftXEdgeTo(std::max(overflowRect.x(), clientBox.x() - *allowance.left()));
        if (allowance.right())
            overflowRect.shiftMaxXEdgeTo(std::min(overflowRect.maxX(), clientBox.maxX() + *allowance.right()));

        // Now re-test with the adjusted rectangle and see if it has become unreachable or fully
        // contained.
        if (clientBox.contains(overflowRect) || overflowRect.isEmpty())
            return;
    }

    ensureOverflow().addLayoutOverflow(overflowRect);
}

void RenderBox::addVisualOverflow(const LayoutRect& rect)
{
    LayoutRect borderBox = borderBoxRect();
    if (borderBox.contains(rect) || rect.isEmpty())
        return;

    ensureOverflow().addVisualOverflow(rect);
}

void RenderBox::clearOverflow()
{
    m_overflow = { };
    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow())
        fragmentedFlow->clearFragmentsOverflow(*this);
}

RenderOverflow& RenderBox::ensureOverflow()
{
    if (!m_overflow)
        m_overflow = makeUnique<RenderOverflow>(flippedClientBoxRect(), borderBoxRect());

    return *m_overflow;
}

bool RenderBox::percentageLogicalHeightIsResolvable() const
{
    // Do this to avoid duplicating all the logic that already exists when computing an actual percentage height.
    return computePercentageLogicalHeight(Style::PreferredSize { 100_css_percentage }) != std::nullopt;
}

bool RenderBox::hasUnsplittableScrollingOverflow() const
{
    // We will paginate as long as we don't scroll overflow in the pagination direction.
    bool isHorizontal = isHorizontalWritingMode();
    if ((isHorizontal && !scrollsOverflowY()) || (!isHorizontal && !scrollsOverflowX()))
        return false;
    
    // Fragmenting scrollbars is only problematic in interactive media, e.g. multicol on a
    // screen. If we're printing, which is non-interactive media, we should allow objects with
    // non-visible overflow to be paginated as normally.
    if (document().printing())
        return false;

    // We do have overflow. We'll still be willing to paginate as long as the block
    // has auto logical height, auto or undefined max-logical-height and a zero or auto min-logical-height.
    // Note this is just a heuristic, and it's still possible to have overflow under these
    // conditions, but it should work out to be good enough for common cases. Paginating overflow
    // with scrollbars present is not the end of the world and is what we used to do in the old model anyway.
    return style().logicalHeight().isSpecified()
        || (style().logicalMaxHeight().isSpecified() && (!style().logicalMaxHeight().isPercentOrCalculated() || percentageLogicalHeightIsResolvable()))
        || (style().logicalMinHeight().isSpecified() && style().logicalMinHeight().isPossiblyPositive() && (!style().logicalMinHeight().isPercentOrCalculated() || percentageLogicalHeightIsResolvable()));
}

bool RenderBox::isUnsplittableForPagination() const
{
    return isBlockLevelReplacedOrAtomicInline()
        || (is<HTMLFormControlElement>(element()) && !is<HTMLFieldSetElement>(element()))
        || hasUnsplittableScrollingOverflow()
        || (parent() && isWritingModeRoot())
        || (isFloating() && style().pseudoElementType() == PseudoElementType::FirstLetter && style().initialLetter().drop() > 0)
        || shouldApplySizeContainment();
}

LayoutUnit RenderBox::lineHeight() const
{
    auto shouldUseLineHeightFromStyle = [&] {
        if (is<RenderBlock>(*this))
            return true;
        if (CheckedPtr listMarkerRenderer = dynamicDowncast<RenderListMarker>(*this))
            return !listMarkerRenderer->isImage();
        return false;
    };
    if (shouldUseLineHeightFromStyle())
        return LayoutUnit::fromFloatCeil(firstLineStyle().computedLineHeight());

    if (isBlockLevelReplacedOrAtomicInline())
        return marginBefore() + logicalHeight() + marginAfter();

    return { };
}

RenderLayer* RenderBox::enclosingFloatPaintingLayer() const
{
    for (auto& box : lineageOfType<RenderBox>(*this)) {
        if (box.layer() && box.layer()->isSelfPaintingLayer())
            return box.layer();
    }
    return nullptr;
}

LayoutRect RenderBox::logicalVisualOverflowRectForPropagation(const WritingMode parentWritingMode) const
{
    LayoutRect rect = visualOverflowRectForPropagation(parentWritingMode);
    if (!parentWritingMode.isHorizontal())
        return rect.transposedRect();
    return rect;
}

LayoutRect RenderBox::visualOverflowRectForPropagation(const WritingMode parentWritingMode) const
{
    // If the writing modes of the child and parent match, then we don't have to 
    // do anything fancy. Just return the result.
    LayoutRect rect = visualOverflowRect();
    if (parentWritingMode.blockDirection() == writingMode().blockDirection())
        return rect;
    
    // We are putting ourselves into our parent's coordinate space.  If there is a flipped block mismatch
    // in a particular axis, then we have to flip the rect along that axis.
    if (writingMode().blockDirection() == FlowDirection::RightToLeft || parentWritingMode.blockDirection() == FlowDirection::RightToLeft)
        rect.setX(width() - rect.maxX());
    else if (writingMode().blockDirection() == FlowDirection::BottomToTop || parentWritingMode.blockDirection() == FlowDirection::BottomToTop)
        rect.setY(height() - rect.maxY());

    return rect;
}

LayoutRect RenderBox::logicalLayoutOverflowRectForPropagation(const WritingMode parentWritingMode) const
{
    LayoutRect rect = layoutOverflowRectForPropagation(parentWritingMode);
    if (!parentWritingMode.isHorizontal())
        return rect.transposedRect();
    return rect;
}

LayoutRect RenderBox::layoutOverflowRectForPropagation(const WritingMode parentWritingMode) const
{
    // Only propagate interior layout overflow if we don't completely clip it.
    auto rect = borderBoxRect();
    // As per https://drafts.csswg.org/css-overflow-3/#scrollable, both flex and grid items margins' should contribute to the scrollable overflow area.
    if (shouldMarginInlineEndContributeToScrollableOverflow(*this)) {
        auto marginEnd = std::max(0_lu, this->marginEnd(parentWritingMode));
        parentWritingMode.isHorizontal() ? rect.setWidth(rect.width() + marginEnd) : rect.setHeight(rect.height() + marginEnd);
    }

    if (!shouldApplyLayoutContainment()) {
        if (hasNonVisibleOverflow()) {
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
            }
        } else
            rect.unite(layoutOverflowRect());
    }

    bool isTransformed = this->isTransformed();
    // While a stickily positioned renderer is also inflow positioned, they stretch the overflow rect with their inflow geometry
    // (as opposed to the paint geometry) because they are not stationary.
    bool paintGeometryAffectsLayoutOverflow = isTransformed || (isInFlowPositioned() && !isStickilyPositioned());
    if (paintGeometryAffectsLayoutOverflow) {
        // If we are relatively positioned or if we have a transform, then we have to convert
        // this rectangle into physical coordinates, apply relative positioning and transforms
        // to it, and then convert it back.
        // It ensures that the overflow rect tracks the paint geometry and not the inflow layout position.
        flipForWritingMode(rect);

        LayoutSize containerOffset;
        if (isInFlowPositioned())
            containerOffset = offsetForInFlowPosition();

        auto container = this->container();
        if (shouldUseTransformFromContainer(container)) {
            TransformationMatrix transform;
            getTransformFromContainer(containerOffset, transform);
            rect = transform.mapRect(rect);
        } else
            rect.move(offsetForInFlowPosition());

        // Now we need to flip back.
        flipForWritingMode(rect);
    }
    
    // If the writing modes of the child and parent match, then we don't have to 
    // do anything fancy. Just return the result.
    if (parentWritingMode.blockDirection() == writingMode().blockDirection())
        return rect;
    
    // We are putting ourselves into our parent's coordinate space.  If there is a flipped block mismatch
    // in a particular axis, then we have to flip the rect along that axis.
    if (writingMode().blockDirection() == FlowDirection::RightToLeft || parentWritingMode.blockDirection() == FlowDirection::RightToLeft)
        rect.setX(width() - rect.maxX());
    else if (writingMode().blockDirection() == FlowDirection::BottomToTop || parentWritingMode.blockDirection() == FlowDirection::BottomToTop)
        rect.setY(height() - rect.maxY());

    return rect;
}

LayoutRect RenderBox::flippedClientBoxRect() const
{
    // Because of the special coordinate system used for overflow rectangles (not quite logical, not
    // quite physical), we need to flip the block progression coordinate in vertical-rl and
    // horizontal-bt writing modes. Apart from that, this method does the same as clientBoxRect().

    auto rect = paddingBoxRectIncludingScrollbar();
    // Flip block progression axis if writing mode is vertical-rl or horizontal-bt.
    flipForWritingMode(rect);
    if (hasNonVisibleOverflow()) {
        // Subtract space occupied by scrollbars. They are at their physical edge in this coordinate
        // system, so order is important here: first flip, then subtract scrollbars.
        if (isHorizontalWritingMode() && shouldPlaceVerticalScrollbarOnLeft())
            rect.move(verticalScrollbarWidth(), 0);
        rect.contract(verticalScrollbarWidth(), horizontalScrollbarHeight());
    }
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
    if (!writingMode().isBlockFlipped())
        return point;
    
    // The child is going to add in its x() and y(), so we have to make sure it ends up in
    // the right place.
    if (isHorizontalWritingMode())
        return LayoutPoint(point.x(), point.y() + height() - child.height() - (2 * child.y()));
    return LayoutPoint(point.x() + width() - child.width() - (2 * child.x()), point.y());
}

void RenderBox::flipForWritingMode(LayoutRect& rect) const
{
    if (!writingMode().isBlockFlipped())
        return;

    if (isHorizontalWritingMode())
        rect.setY(height() - rect.maxY());
    else
        rect.setX(width() - rect.maxX());
}

LayoutUnit RenderBox::flipForWritingMode(LayoutUnit position) const
{
    if (!writingMode().isBlockFlipped())
        return position;
    return logicalHeight() - position;
}

LayoutPoint RenderBox::flipForWritingMode(const LayoutPoint& position) const
{
    if (!writingMode().isBlockFlipped())
        return position;
    return isHorizontalWritingMode() ? LayoutPoint(position.x(), height() - position.y()) : LayoutPoint(width() - position.x(), position.y());
}

LayoutSize RenderBox::flipForWritingMode(const LayoutSize& offset) const
{
    if (!writingMode().isBlockFlipped())
        return offset;
    return isHorizontalWritingMode() ? LayoutSize(offset.width(), height() - offset.height()) : LayoutSize(width() - offset.width(), offset.height());
}

FloatPoint RenderBox::flipForWritingMode(const FloatPoint& position) const
{
    if (!writingMode().isBlockFlipped())
        return position;
    return isHorizontalWritingMode() ? FloatPoint(position.x(), height() - position.y()) : FloatPoint(width() - position.x(), position.y());
}

void RenderBox::flipForWritingMode(FloatRect& rect) const
{
    if (!writingMode().isBlockFlipped())
        return;

    if (isHorizontalWritingMode())
        rect.setY(height() - rect.maxY());
    else
        rect.setX(width() - rect.maxX());
}

void RenderBox::flipForWritingMode(RepaintRects& rects) const
{
    if (!writingMode().isBlockFlipped())
        return;

    rects.flipForWritingMode(size(), isHorizontalWritingMode());
}

LayoutPoint RenderBox::topLeftLocationWithFlipping() const
{
    ASSERT(view().frameView().hasFlippedBlockRenderers());

    auto* containerBlock = containingBlock();
    if (!containerBlock || containerBlock == this)
        return location();
    return containerBlock->flipForWritingModeForChild(*this, location());
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
        if (is<RenderReplaced>(*this)) {
            if (hasStretchedLogicalWidth() && hasStretchedLogicalHeight())
                return false;
        } else if (hasStretchedLogicalWidth(StretchingMode::Explicit))
            return false;
        if (style().logicalWidth().isPercentOrCalculated() && parent()->style().logicalWidth().isFixed())
            return false;
    }

    auto isResolvablePercentageHeight = [&] {
        return style().logicalHeight().isPercentOrCalculated() && (isOutOfFlowPositioned() || percentageLogicalHeightIsResolvable());
    };
    return overridingBorderBoxLogicalHeight() || shouldComputeLogicalWidthFromAspectRatioAndInsets(*this) || style().logicalHeight().isFixed() || isResolvablePercentageHeight();
}

LayoutUnit RenderBox::computeLogicalWidthFromAspectRatioInternal() const
{
    ASSERT(shouldComputeLogicalWidthFromAspectRatio());
    auto computedValues = computeLogicalHeight(logicalHeight(), logicalTop());
    LayoutUnit logicalHeightforAspectRatio = computedValues.m_extent;

    return inlineSizeFromAspectRatio(horizontalBorderAndPaddingExtent(), verticalBorderAndPaddingExtent(), style().logicalAspectRatio(), style().boxSizingForAspectRatio(), logicalHeightforAspectRatio, style().aspectRatio(), isRenderReplaced());
}

LayoutUnit RenderBox::computeLogicalWidthFromAspectRatio() const
{
    auto logicalWidth = computeLogicalWidthFromAspectRatioInternal();
    LayoutUnit containerWidthInInlineDirection = std::max<LayoutUnit>(0, containingBlockLogicalWidthForContent());
    return constrainLogicalWidthByMinMax(logicalWidth, containerWidthInInlineDirection, *containingBlock(), AllowIntrinsic::No);
}

bool RenderBox::isRenderReplacedWithIntrinsicRatio() const
{
    if (auto* replaced = dynamicDowncast<RenderReplaced>(this))
        return replaced->computeIntrinsicAspectRatio();
    return false;
}

std::optional<double> RenderBox::resolveAspectRatio() const
{
    if (auto* replacedElement = dynamicDowncast<RenderReplaced>(this)) 
        return replacedElement->computeIntrinsicAspectRatio();
    if (style().hasAspectRatio()) 
        return style().logicalAspectRatio();
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

std::pair<LayoutUnit, LayoutUnit> RenderBox::computeMinMaxLogicalWidthFromAspectRatio() const
{
    LayoutUnit transferredMinSize = LayoutUnit();
    LayoutUnit transferredMaxSize = LayoutUnit::max();
    std::optional<double> aspectRatio = resolveAspectRatio();
    if (!aspectRatio)
        return { transferredMinSize, transferredMaxSize };

    if (style().logicalMinHeight().isSpecified()) {
        if (LayoutUnit blockMinSize = constrainLogicalHeightByMinMax(LayoutUnit(), std::nullopt); blockMinSize > LayoutUnit())
            transferredMinSize = inlineSizeFromAspectRatio(borderAndPaddingLogicalWidth(), borderAndPaddingLogicalHeight(), *aspectRatio, style().boxSizingForAspectRatio(), blockMinSize, style().aspectRatio(), isRenderReplaced());
    }
    if (style().logicalMaxHeight().isSpecified()) {
        if (LayoutUnit blockMaxSize = constrainLogicalHeightByMinMax(LayoutUnit::max(), std::nullopt); blockMaxSize != LayoutUnit::max())
            transferredMaxSize = inlineSizeFromAspectRatio(borderAndPaddingLogicalWidth(), borderAndPaddingLogicalHeight(), *aspectRatio, style().boxSizingForAspectRatio(), blockMaxSize, style().aspectRatio(), isRenderReplaced());
    }
    // Spec says the transferred max size should be floored by the transferred min size
    transferredMaxSize = std::max(transferredMinSize, transferredMaxSize);
    return { transferredMinSize, transferredMaxSize };
}

std::pair<LayoutUnit, LayoutUnit> RenderBox::computeMinMaxLogicalHeightFromAspectRatio() const
{
    LayoutUnit transferredMinSize = LayoutUnit();
    LayoutUnit transferredMaxSize = LayoutUnit::max();
    std::optional<double> aspectRatio = resolveAspectRatio();
    if (!aspectRatio)
        return { transferredMinSize, transferredMaxSize };

    if (style().logicalMinWidth().isSpecified()) {
        if (LayoutUnit inlineMinSize = computeLogicalWidthUsing(style().logicalMinWidth(), containingBlockLogicalWidthForContent(), *containingBlock()); inlineMinSize > LayoutUnit())
            transferredMinSize = blockSizeFromAspectRatio(borderAndPaddingLogicalWidth(), borderAndPaddingLogicalHeight(), *aspectRatio, style().boxSizingForAspectRatio(), inlineMinSize, style().aspectRatio(), isRenderReplaced());
    }

    if (style().logicalMaxWidth().isSpecified()) {
        if (LayoutUnit inlineMaxSize = computeLogicalWidthUsing(style().logicalMaxWidth(), containingBlockLogicalWidthForContent(), *containingBlock()); inlineMaxSize != LayoutUnit::max())
            transferredMaxSize = blockSizeFromAspectRatio(borderAndPaddingLogicalWidth(), borderAndPaddingLogicalHeight(), *aspectRatio, style().boxSizingForAspectRatio(), inlineMaxSize, style().aspectRatio(), isRenderReplaced());
    }
    // Spec says the transferred max size should be floored by the transferred min size 
    transferredMaxSize = std::max(transferredMinSize, transferredMaxSize);
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
    return Style::extentForRect(style().scrollPaddingBox(), viewportRect, Style::ZoomNeeded { });
}

LayoutUnit RenderBox::intrinsicLogicalWidth() const
{
    return writingMode().isHorizontal() ? intrinsicSize().width() : intrinsicSize().height();
}

bool RenderBox::shouldIgnoreLogicalMinMaxWidthSizes() const
{
    if (!isFlexItem())
        return false;
    if (auto* flexBox = dynamicDowncast<RenderFlexibleBox>(parent()))
        return flexBox->isComputingFlexBaseSizes() && writingMode().isHorizontal() == flexBox->isHorizontalFlow();
    ASSERT_NOT_REACHED();
    return false;
}

bool RenderBox::shouldIgnoreLogicalMinMaxHeightSizes() const
{
    if (!isFlexItem())
        return false;
    if (auto* flexBox = dynamicDowncast<RenderFlexibleBox>(parent()))
        return flexBox->isComputingFlexBaseSizes() && writingMode().isHorizontal() != flexBox->isHorizontalFlow();
    ASSERT_NOT_REACHED();
    return false;
}

std::optional<LayoutUnit> RenderBox::explicitIntrinsicInnerWidth() const
{
    ASSERT(isHorizontalWritingMode() ? shouldApplySizeOrInlineSizeContainment() : shouldApplySizeContainment());

    auto& containIntrinsicWidth = style().containIntrinsicWidth();
    if (containIntrinsicWidth.isNone())
        return { };

    if (containIntrinsicWidth.hasAuto() && isSkippedContentRoot(*this)) {
        // If auto is specified and the element has a last remembered size and is currently skipping its contents,
        // its explicit intrinsic inner size in the corresponding axis is the last remembered size in that axis.
        // https://drafts.csswg.org/css-sizing-4/#intrinsic-size-override
        if (auto width = isHorizontalWritingMode() ? element()->lastRememberedLogicalWidth() : element()->lastRememberedLogicalHeight())
            return width;
    }

    if (auto length = containIntrinsicWidth.tryLength())
        return LayoutUnit { length->resolveZoom(Style::ZoomNeeded { }) };

    ASSERT(containIntrinsicWidth.isAutoAndNone());
    return { };
}

std::optional<LayoutUnit> RenderBox::explicitIntrinsicInnerHeight() const
{
    ASSERT(isHorizontalWritingMode() ? shouldApplySizeContainment() : shouldApplySizeOrInlineSizeContainment());

    auto& containIntrinsicHeight = style().containIntrinsicHeight();
    if (containIntrinsicHeight.isNone())
        return { };

    if (containIntrinsicHeight.hasAuto() && isSkippedContentRoot(*this)) {
        // If auto is specified and the element has a last remembered size and is currently skipping its contents,
        // its explicit intrinsic inner size in the corresponding axis is the last remembered size in that axis.
        // https://drafts.csswg.org/css-sizing-4/#intrinsic-size-override
        if (auto height = isHorizontalWritingMode() ? element()->lastRememberedLogicalHeight() : element()->lastRememberedLogicalWidth())
            return height;
    }

    if (auto length = containIntrinsicHeight.tryLength())
        return LayoutUnit { length->resolveZoom(Style::ZoomNeeded { }) };

    ASSERT(containIntrinsicHeight.isAutoAndNone());
    return { };
}

// hasAutoZIndex only returns true if the element is positioned or a flex-item since
// position:static elements that are not flex-items get their z-index coerced to auto.
bool RenderBox::requiresLayer() const
{
    return RenderBoxModelObject::requiresLayer()
        || hasNonVisibleOverflow()
        || style().specifiesColumns()
        || style().usedContain().contains(Style::ContainValue::Layout)
        || !style().usedZIndex().isAuto()
        || hasRunningAcceleratedAnimations();
}

void RenderBox::updateFloatPainterAfterSelfPaintingLayerChange()
{
    ASSERT(isFloating());
    ASSERT(!hasLayer() || !layer()->isSelfPaintingLayer());

    // Find the ancestor renderer that is supposed to paint this float now that it is not self painting anymore.
    auto floatingObjectForFloatPainting = [&]() -> FloatingObject* {
        auto& layoutContext = view().frameView().layoutContext();
        if (!layoutContext.isInLayout() || layoutContext.subtreeLayoutRoot() != this)
            return nullptr;

        FloatingObject* floatPainter = nullptr;
        for (auto* ancestor = containingBlock(); ancestor; ancestor = ancestor->containingBlock()) {
            auto* blockFlow = dynamicDowncast<RenderBlockFlow>(*ancestor);
            if (!blockFlow) {
                ASSERT_NOT_REACHED();
                break;
            }
            auto* floatingObjects = blockFlow->floatingObjectSet();
            if (!floatingObjects)
                break;
            auto blockFlowContainsThisFloat = false;
            for (auto& floatingObject : *floatingObjects) {
                if (!floatingObject->renderer())
                    continue;
                blockFlowContainsThisFloat = floatingObject->renderer() == this;
                if (blockFlowContainsThisFloat) {
                    floatPainter = floatingObject.get();
                    if (blockFlow->hasLayer() && blockFlow->layer()->isSelfPaintingLayer())
                        return floatPainter;
                    break;
                }
            }
            if (!blockFlowContainsThisFloat)
                break;
        }
        // There has to be an ancestor with a floating object assigned to this renderer.
        ASSERT(floatPainter);
        return floatPainter;
    };
    if (auto* floatingObject = floatingObjectForFloatPainting())
        floatingObject->setPaintsFloat(true);
}

using ShapeOutsideInfoMap = SingleThreadWeakHashMap<const RenderBox, std::unique_ptr<ShapeOutsideInfo>>;
static ShapeOutsideInfoMap& shapeOutsideInfoMap()
{
    static NeverDestroyed<ShapeOutsideInfoMap> staticInfoMap;
    return staticInfoMap;
}

ShapeOutsideInfo* RenderBox::shapeOutsideInfo() const
{
    if (!renderBoxHasShapeOutsideInfo())
        return nullptr;

    if (!ShapeOutsideInfo::isEnabledFor(*this))
        return nullptr;

    return shapeOutsideInfoMap().get(*this);
}

ShapeOutsideInfo& RenderBox::ensureShapeOutsideInfo()
{
    setRenderBoxHasShapeOutsideInfo(true);
    return *shapeOutsideInfoMap().ensure(*this, [&] {
        return makeUnique<ShapeOutsideInfo>(*this);
    }).iterator->value;
}

void RenderBox::removeShapeOutsideInfo()
{
    if (!renderBoxHasShapeOutsideInfo())
        return;

    setRenderBoxHasShapeOutsideInfo(false);
    shapeOutsideInfoMap().remove(*this);
}

bool RenderBox::hasAutoHeightOrContainingBlockWithAutoHeight(UpdatePercentageHeightDescendants updatePercentageDescendants) const
{
    auto& logicalHeight = style().logicalHeight();
    auto* containingBlock = containingBlockForAutoHeightDetection(logicalHeight);

    if (updatePercentageDescendants == UpdatePercentageHeightDescendants::Yes && logicalHeight.isPercentOrCalculated() && containingBlock)
        containingBlock->addPercentHeightDescendant(const_cast<RenderBox&>(*this));

    if (isFlexItem() && downcast<RenderFlexibleBox>(*parent()).canUseFlexItemForPercentageResolution(*this))
        return false;

    if (isGridItem()) {
        if (auto containingBlockContentLogicalHeight = gridAreaContentLogicalHeight())
            return !*containingBlockContentLogicalHeight;
    }

    auto isOutOfFlowPositionedWithImplicitHeight = isOutOfFlowPositioned() && !style().logicalTop().isAuto() && !style().logicalBottom().isAuto();
    if (logicalHeight.isAuto() && !isOutOfFlowPositionedWithImplicitHeight)
        return true;

    // We need the containing block to have a definite block-size in order to resolve the block-size of the descendant,
    // except when in quirks mode. Flexboxes follow strict behavior even in quirks mode, though.
    if (!containingBlock || (document().inQuirksMode() && !containingBlock->isFlexibleBoxIncludingDeprecated()))
        return false;

    return !containingBlock->hasDefiniteLogicalHeight();
}

bool RenderBox::overflowChangesMayAffectLayout() const
{
    if (style().overflowY() != Overflow::Auto && style().overflowX() != Overflow::Auto)
        return false;

    if (style().usesLegacyScrollbarStyle())
        return true;

    // FIXME: Bug 273167
#if PLATFORM(IOS_FAMILY)
    if (!ScrollbarTheme::theme().isMockTheme())
        return false;
#endif
    return !ScrollbarTheme::theme().usesOverlayScrollbars();

}

} // namespace WebCore
