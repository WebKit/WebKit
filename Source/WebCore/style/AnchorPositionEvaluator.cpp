/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AnchorPositionEvaluator.h"

#include "CSSPropertyNames.h"
#include "CSSValue.h"
#include "CSSValueKeywords.h"
#include "ContainerNodeInlines.h"
#include "Document.h"
#include "Element.h"
#include "Node.h"
#include "NodeRenderStyle.h"
#include "PopoverData.h"
#include "PositionedLayoutConstraints.h"
#include "RenderBoxInlines.h"
#include "RenderBlock.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderFragmentedFlow.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayerCompositor.h"
#include "RenderObjectInlines.h"
#include "RenderStyle.h"
#include "RenderStyleInlines.h"
#include "RenderStyleSetters.h"
#include "RenderView.h"
#include "StyleBuilderState.h"
#include "StyleScope.h"
#include "WritingMode.h"
#include <ranges>

namespace WebCore {

AnchorScrollSnapshot::AnchorScrollSnapshot(const RenderBox& scroller, LayoutPoint snapshot)
    : m_scroller(scroller)
    , m_scrollSnapshot(snapshot)
{ }

AnchorScrollSnapshot::AnchorScrollSnapshot(LayoutPoint snapshot)
    : m_scroller(nullptr)
    , m_scrollSnapshot(snapshot)
{ }

inline LayoutSize AnchorScrollSnapshot::adjustmentForCurrentScrollPosition() const
{
    if (m_scroller)
        return m_scrollSnapshot - m_scroller->scrollPosition();
    return { };
}

inline bool AnchorScrollAdjuster::isEmpty() const
{
    return !m_scrollSnapshots.size() && !(m_hasChainedAnchor | m_hasStickyAnchor);
}

AnchorScrollAdjuster::AnchorScrollAdjuster(RenderBox& anchored, const RenderBoxModelObject& defaultAnchor)
    : m_anchored(anchored)
{
    auto& style = anchored.style();

    auto compensatedAxes = style.anchorFunctionScrollCompensatedAxes();
    m_needsXAdjustment = compensatedAxes.contains(BoxAxis::Horizontal);
    m_needsYAdjustment = compensatedAxes.contains(BoxAxis::Vertical);

    auto containingWritingMode = anchored.container()->style().writingMode();
    if (auto positionAreaValue = style.positionArea().tryValue()) {
        if (positionAreaValue->coordMatchedTrackForAxis(BoxAxis::Horizontal, containingWritingMode, style.writingMode()) != Style::PositionAreaTrack::SpanAll)
            m_needsXAdjustment |= true;
        else {
            if (containingWritingMode.isHorizontal()) {
                auto alignment = style.justifySelf();
                m_needsXAdjustment |= alignment.isAuto() || alignment.isNormal() || alignment.isAnchorCenter();
            } else {
                auto alignment = style.alignSelf();
                m_needsXAdjustment |= alignment.isAuto() || alignment.isNormal() || alignment.isAnchorCenter();
            }
        }
        if (positionAreaValue->coordMatchedTrackForAxis(BoxAxis::Vertical, containingWritingMode, style.writingMode()) != Style::PositionAreaTrack::SpanAll)
            m_needsYAdjustment |= true;
        else {
            if (containingWritingMode.isHorizontal()) {
                auto alignment = style.alignSelf();
                m_needsYAdjustment |= alignment.isAuto() || alignment.isNormal() || alignment.isAnchorCenter();
            } else {
                auto alignment = style.justifySelf();
                m_needsYAdjustment |= alignment.isAuto() || alignment.isNormal() || alignment.isAnchorCenter();
            }
        }
    }

    if (!m_needsXAdjustment) {
        m_needsXAdjustment = containingWritingMode.isHorizontal()
            ? style.justifySelf().isAnchorCenter()
            : style.alignSelf().isAnchorCenter();
    }
    if (!m_needsYAdjustment) {
        m_needsYAdjustment = containingWritingMode.isHorizontal()
            ? style.alignSelf().isAnchorCenter()
            : style.alignSelf().isAnchorCenter();
    }

    m_isHidden = style.isForceHidden();

    if ((m_hasStickyAnchor = defaultAnchor.isStickilyPositioned()))
        m_stickySnapshot = defaultAnchor.stickyPositionOffset();

    if (auto anchorBox = dynamicDowncast<RenderBox>(defaultAnchor)) {
        if (CheckedPtr chainedAnchor = Style::AnchorPositionEvaluator::defaultAnchorForBox(*anchorBox))
            m_hasChainedAnchor = true;
    }
}

RenderBox* AnchorScrollAdjuster::anchored() const
{
    return m_anchored.ptr();
}

bool AnchorScrollAdjuster::recaptureDiffers(const AnchorScrollAdjuster& other) const
{
    bool same = m_anchored.ptr() == other.m_anchored.ptr()
        && m_scrollSnapshots == other.m_scrollSnapshots
        && m_stickySnapshot == other.m_stickySnapshot;
    return !same;
}

void AnchorScrollAdjuster::addSnapshot(const RenderBox& scroller)
{
    ASSERT(scroller.hasPotentiallyScrollableOverflow() && !is<RenderView>(scroller));
    m_scrollSnapshots.constructAndAppend(scroller, scroller.constrainedScrollPosition());
}

void AnchorScrollAdjuster::addViewportSnapshot(const RenderView& renderView)
{
    auto& view = renderView.frameView();
    auto position = view.constrainedScrollPosition(ScrollPosition(view.scrollPositionRespectingCustomFixedPosition()));
    m_scrollSnapshots.insert(0, AnchorScrollSnapshot { position });
    m_adjustForViewport = true;
}

LayoutSize AnchorScrollAdjuster::adjustmentForViewport(const RenderView& renderView) const
{
    if (m_adjustForViewport) {
        // Viewport snapshot is stored in the first slot.
        ASSERT(m_scrollSnapshots.size() && !m_scrollSnapshots.first().m_scroller);
        auto& view = renderView.frameView();
        return m_scrollSnapshots.first().m_scrollSnapshot
            - view.constrainedScrollPosition(IntPoint(view.scrollPositionRespectingCustomFixedPosition()));
    }
    return { };
}

LayoutSize AnchorScrollAdjuster::accumulateAdjustments(const RenderView& renderView, const RenderBox& anchored) const
{
    ASSERT(m_anchored.ptr() == &anchored);
    LayoutSize scrollAdjustment;

    if (m_hasChainedAnchor || m_hasStickyAnchor) {
        if (CheckedPtr defaultAnchor = Style::AnchorPositionEvaluator::defaultAnchorForBox(anchored)) {
            auto defaultAnchorBox = dynamicDowncast<RenderBox>(defaultAnchor.get());
            ASSERT(defaultAnchorBox); // We shouldn't exist if there's no default anchor.
            if (defaultAnchorBox) {
                // The anchor may itself be scroll-compensated. Recurse if needed.
                if (auto chainedAdjuster = renderView.layoutContext().anchorScrollAdjusterFor(*defaultAnchorBox))
                    scrollAdjustment += chainedAdjuster->accumulateAdjustments(renderView, *defaultAnchorBox);
                // Compensate for sticky adjustments.
                if (m_hasStickyAnchor)
                    scrollAdjustment += defaultAnchorBox->stickyPositionOffset() - m_stickySnapshot;
            }
        }
    }

    for (auto snapshot : m_scrollSnapshots)
        scrollAdjustment += snapshot.adjustmentForCurrentScrollPosition();
    scrollAdjustment += adjustmentForViewport(renderView);

    if (!m_needsXAdjustment)
        scrollAdjustment.setWidth(0);
    if (!m_needsYAdjustment)
        scrollAdjustment.setHeight(0);
    return scrollAdjustment;
}

void AnchorScrollAdjuster::setFallbackLimits(const RenderBox& anchored)
{
    auto xConstraints = PositionedLayoutConstraints { anchored, anchored.writingMode().isHorizontal() ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block };
    auto yConstraints = PositionedLayoutConstraints { anchored, anchored.writingMode().isHorizontal() ? LogicalBoxAxis::Block : LogicalBoxAxis::Inline };
    auto xLimits = xConstraints.originalContainingRange();
    auto yLimits = yConstraints.originalContainingRange();

    auto marginRect = anchored.marginBoxRect();
    marginRect.moveBy(anchored.location());
    m_fallbackLimits.m_min.setWidth(xLimits.min() - marginRect.x());
    m_fallbackLimits.m_min.setHeight(yLimits.min() - marginRect.y());
    m_fallbackLimits.m_max.setWidth(xLimits.max() - marginRect.maxX());
    m_fallbackLimits.m_max.setHeight(yLimits.max() - marginRect.maxY());

    m_hasFallback = true;
}

bool AnchorScrollAdjuster::invalidateForScroller(const RenderBox& scroller)
{
    bool anchoredNeedsInvalidation = false;
    for (auto snapshot : m_scrollSnapshots) {
        if (snapshot.m_scroller.get() == &scroller) {
            anchoredNeedsInvalidation = true;
            break;
        }
    }
    if (anchoredNeedsInvalidation) {
        m_anchored->setNeedsLayout();
        ASSERT(m_anchored->element());
        if (CheckedPtr element = m_anchored->element())
            element->invalidateForAnchorRectChange();
    }
    return anchoredNeedsInvalidation;
}

namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AnchorPositionedState);

static inline void clearAnchorScrollSnapshots(RenderBox& anchored)
{
    if (!anchored.layer()->anchorScrollAdjustment())
        return;
    anchored.layoutContext().unregisterAnchorScrollAdjusterFor(anchored);
}

void AnchorPositionEvaluator::captureScrollSnapshots(RenderBox& anchored, bool invalidateStyleForScrollPositionChanges)
{
    if (!anchored.layer())
        return;

    CheckedPtr defaultAnchor = AnchorPositionEvaluator::defaultAnchorForBox(anchored);
    if (!defaultAnchor)
        return clearAnchorScrollSnapshots(anchored);

    AnchorScrollAdjuster adjuster(anchored, *defaultAnchor);
    if (!adjuster.mayNeedAdjustment()) // Note: We sometimes hit this path during interleaved layout because style bits aren't set yet.
        return clearAnchorScrollSnapshots(anchored);

    CheckedPtr containingBlock = anchored.containingBlock();
    ASSERT(defaultAnchor->isDescendantOf(containingBlock.get()));

    bool isFixedAnchor = defaultAnchor->layer() && defaultAnchor->layer()->behavesAsFixed();
    for (auto* ancestor = defaultAnchor->container(); ancestor && ancestor != containingBlock; ancestor = ancestor->container()) {
        if (auto* box = dynamicDowncast<RenderBox>(ancestor)) {
            if (box->hasPotentiallyScrollableOverflow())
                adjuster.addSnapshot(*box);
            if (box->layer() && box->layer()->behavesAsFixed())
                isFixedAnchor = true;
        }
    }

    if (anchored.layer() && anchored.layer()->behavesAsFixed() && !isFixedAnchor)
        adjuster.addViewportSnapshot(anchored.view());

    if (adjuster.isEmpty())
        return clearAnchorScrollSnapshots(anchored);

    if (!anchored.style().positionTryFallbacks().isNone()
        || anchored.style().positionVisibility().contains(PositionVisibilityValue::NoOverflow))
        adjuster.setFallbackLimits(anchored);

    auto captureDiff = anchored.layoutContext().registerAnchorScrollAdjuster(WTFMove(adjuster));
    if (invalidateStyleForScrollPositionChanges && AnchorScrollAdjuster::SnapshotsDiffer == captureDiff && anchored.style().usesAnchorFunctions()) {
        // Scroll positions changed since the last capture, which means anchor() resolution needs updating.
        if (CheckedPtr element = anchored.element())
            element->invalidateForAnchorRectChange();
    }
    anchored.layer()->setAnchorScrollAdjustment({ });
}

void AnchorPositionEvaluator::updateScrollAdjustments(RenderView& renderView)
{
    // https://drafts.csswg.org/css-anchor-position-1/#scroll
    auto& layoutContext = renderView.layoutContext();
    bool needsRecompositing = false;

    for (auto adjuster : layoutContext.anchorScrollAdjusters()) {
        CheckedPtr anchored = adjuster.anchored();
        if (!anchored || !anchored->layer()) {
            ASSERT_NOT_REACHED(); // RenderBox failed to clean up.
            continue;
        }

        LayoutSize scrollOffset = adjuster.accumulateAdjustments(renderView, *anchored);
        if (!anchored->layer()->setAnchorScrollAdjustment(scrollOffset))
            continue;

        bool shouldBeHidden = false;
        bool needsInvalidation = false;
        if (adjuster.hasFallbackLimits()) {
            if (adjuster.exceedsFallbackLimits(scrollOffset)) {
                if (!anchored->style().positionTryFallbacks().isNone()) {
                    anchored->setNeedsLayout();
                    needsInvalidation = true;
                } else
                    shouldBeHidden = anchored->style().positionVisibility().contains(PositionVisibilityValue::NoOverflow);
            }
        }
        if (!shouldBeHidden && anchored->style().positionVisibility().contains(PositionVisibilityValue::AnchorsVisible))
            shouldBeHidden = AnchorPositionEvaluator::isDefaultAnchorInvisibleOrClippedByInterveningBoxes(*anchored);

        if (needsInvalidation || shouldBeHidden != adjuster.isHidden()) {
            ASSERT(anchored->element());
            if (CheckedPtr element = anchored->element())
                element->invalidateForAnchorRectChange(); // FIXME: Optimize this.
        } else
            needsRecompositing = true;
    };

    if (needsRecompositing && !layoutContext.isInLayout())
        renderView.compositor().scheduleCompositingLayerUpdate();
}


static const ScopedName& implicitAnchorElementName()
{
    // User specified anchor names start with "--".
    static NeverDestroyed<ScopedName> name { ScopedName { "implicit-anchor-element"_s } };
    return name;
}

static BoxAxis mapInsetPropertyToPhysicalAxis(CSSPropertyID id, const WritingMode writingMode)
{
    switch (id) {
    case CSSPropertyLeft:
    case CSSPropertyRight:
        return BoxAxis::Horizontal;
    case CSSPropertyTop:
    case CSSPropertyBottom:
        return BoxAxis::Vertical;
    case CSSPropertyInsetInlineStart:
    case CSSPropertyInsetInlineEnd:
        return mapAxisLogicalToPhysical(writingMode, LogicalBoxAxis::Inline);
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetBlockEnd:
        return mapAxisLogicalToPhysical(writingMode, LogicalBoxAxis::Block);
    default:
        ASSERT_NOT_REACHED();
        return BoxAxis::Horizontal;
    }
}

static LogicalBoxAxis mapInsetPropertyToLogicalAxis(CSSPropertyID id, const WritingMode writingMode)
{
    switch (id) {
    case CSSPropertyLeft:
    case CSSPropertyRight:
        return writingMode.isHorizontal() ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block;
    case CSSPropertyTop:
    case CSSPropertyBottom:
        return writingMode.isHorizontal() ? LogicalBoxAxis::Block : LogicalBoxAxis::Inline;
    case CSSPropertyInsetInlineStart:
    case CSSPropertyInsetInlineEnd:
        return LogicalBoxAxis::Inline;
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetBlockEnd:
        return LogicalBoxAxis::Block;
    default:
        ASSERT_NOT_REACHED();
        return LogicalBoxAxis::Inline;
    }
}

// Physical sides (left/right/top/bottom) can only be used in certain inset properties. "For example,
// left is usable in left, right, or the logical inset properties that refer to the horizontal axis."
// See: https://drafts.csswg.org/css-anchor-position-1/#typedef-anchor-side
static bool anchorSideMatchesInsetProperty(CSSValueID anchorSideID, BoxAxis physicalAxis)
{
    switch (anchorSideID) {
    case CSSValueID::CSSValueInside:
    case CSSValueID::CSSValueOutside:
    case CSSValueID::CSSValueStart:
    case CSSValueID::CSSValueEnd:
    case CSSValueID::CSSValueSelfStart:
    case CSSValueID::CSSValueSelfEnd:
    case CSSValueID::CSSValueCenter:
    case CSSValueID::CSSValueInvalid: // percentage
        return true;
    case CSSValueID::CSSValueTop:
    case CSSValueID::CSSValueBottom:
        return BoxAxis::Vertical == physicalAxis;
    case CSSValueID::CSSValueLeft:
    case CSSValueID::CSSValueRight:
        return BoxAxis::Horizontal == physicalAxis;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

static LayoutSize offsetFromAncestorContainer(const RenderElement& descendantContainer, const RenderElement& ancestorContainer)
{
    LayoutSize offset;
    LayoutPoint referencePoint;
    CheckedPtr currentContainer = &descendantContainer;
    CheckedPtr maxContainer = &ancestorContainer;
    if (CheckedPtr ancestorInline = dynamicDowncast<RenderInline>(&ancestorContainer))
        maxContainer = ancestorInline->containingBlock();
    do {
        CheckedPtr nextContainer = currentContainer->container();
        ASSERT(nextContainer); // This means we reached the top without finding container.
        if (!nextContainer)
            break;
        LayoutSize currentOffset = currentContainer->offsetFromContainer(*nextContainer, referencePoint);

        if (CheckedPtr boxContainer = dynamicDowncast<RenderBox>(*nextContainer)) {
            // Clamp overscroll so we don't layout into it.
            if (boxContainer->hasPotentiallyScrollableOverflow())
                currentOffset += boxContainer->scrollPosition() - boxContainer->constrainedScrollPosition();
        }

        offset += currentOffset;
        referencePoint.move(currentOffset);
        currentContainer = WTFMove(nextContainer);
    } while (currentContainer != maxContainer);

    if (CheckedPtr descendantInline = dynamicDowncast<RenderInline>(&descendantContainer)) {
        // RenderInline objects do not automatically account for their offset above,
        // so we incorporate this offset here.
        offset += toLayoutSize(descendantInline->linesBoundingBox().location());
    }
    if (descendantContainer.containingBlock() == ancestorContainer.containingBlock()) {
        // Account for 'position: relative' inline containing blocks by shifting back down into them.
        if (CheckedPtr ancestorInline = dynamicDowncast<RenderInline>(&ancestorContainer))
            offset -= toLayoutSize(ancestorInline->firstInlineBoxTopLeft()); // FIXME: Handle RTL.
    }

    if (auto ancestorBox = dynamicDowncast<RenderBox>(ancestorContainer)) // Zero out containing block scroll position.
        offset += toLayoutSize(ancestorBox->constrainedScrollPosition());

    return offset;
}

void AnchorPositionEvaluator::addAnchorFunctionScrollCompensatedAxis(RenderStyle& style, const RenderBox& anchored, const RenderBoxModelObject& anchor, BoxAxis axis)
{
    // https://drafts.csswg.org/css-anchor-position-1/#scroll
    // An absolutely positioned box abspos compensates for scroll in the horizontal or vertical axis if both of the following conditions are true:
    // - abspos has a default anchor box.
    auto defaultAnchor = defaultAnchorForBox(anchored);
    if (!defaultAnchor)
        return;

    // - at least one anchor() function on abspos’s used inset properties in the axis refers to a target anchor element
    //   with the same nearest scroll container ancestor as abspos’s default anchor box.
    if (defaultAnchor != &anchor && defaultAnchor->enclosingScrollableContainer() != anchor.enclosingScrollableContainer())
        return;

    auto axes = style.anchorFunctionScrollCompensatedAxes();
    axes.add(axis);
    style.setAnchorFunctionScrollCompensatedAxes(axes);
}

static LayoutRect boundingRectForFragmentedAnchor(const RenderBoxModelObject& anchorBox, const RenderElement& containingBlock, const RenderFragmentedFlow& fragmentedFlow)
{
    // Compute the bounding box of the fragments.
    // Location is relative to the fragmented flow.
    CheckedPtr anchorRenderBox = dynamicDowncast<RenderBox>(&anchorBox);
    if (!anchorRenderBox)
        anchorRenderBox = anchorBox.containingBlock();
    LayoutPoint offsetRelativeToFragmentedFlow = fragmentedFlow.mapFromLocalToFragmentedFlow(anchorRenderBox.get(), { }).location();
    auto unfragmentedBorderBox = anchorBox.borderBoundingBox();
    unfragmentedBorderBox.moveBy(offsetRelativeToFragmentedFlow);
    fragmentedFlow.flipForWritingMode(unfragmentedBorderBox); // Convert to RenderLayer coords.
    auto fragmentsBoundingBox = fragmentedFlow.fragmentsBoundingBox(unfragmentedBorderBox);
    fragmentedFlow.flipForWritingMode(fragmentsBoundingBox); // Convert to RenderBox coords.

    // Now convert to physical coordinates (top/left origin) and walk up.
    // RenderFragmentedFlow doesn't have a usable frame rect, so use its container's content rect.
    CheckedPtr fragmentedFlowContainer = fragmentedFlow.containingBlock();
    if (!fragmentedFlowContainer) {
        ASSERT_NOT_REACHED();
        return fragmentsBoundingBox;
    }
    auto fragmentedFlowRect = fragmentedFlowContainer->contentBoxRect();
    if (fragmentedFlow.writingMode().isBlockFlipped()) {
        if (fragmentedFlow.writingMode().isHorizontal())
            fragmentsBoundingBox.setY(fragmentedFlowRect.height() - fragmentsBoundingBox.maxY());
        else
            fragmentsBoundingBox.setX(fragmentedFlowRect.width() - fragmentsBoundingBox.maxX());
    }
    fragmentsBoundingBox.moveBy(fragmentedFlowRect.location());

    // Change the location to be relative to the anchor's containing block.
    if (fragmentedFlowContainer.get() != &containingBlock)
        fragmentsBoundingBox.move(offsetFromAncestorContainer(*fragmentedFlowContainer, containingBlock));

    return fragmentsBoundingBox;
}

// This computes the top left location, physical width, and physical height of the specified
// anchor element. The location is computed relative to the specified containing block.
LayoutRect AnchorPositionEvaluator::computeAnchorRectRelativeToContainingBlock(CheckedRef<const RenderBoxModelObject> anchorBox, const RenderElement& containingBlock, const RenderBox& anchoredBox)
{
    // Fragmented flows are a little tricky to deal with. One example of a fragmented
    // flow is a block anchor element that is "fragmented" or split across multiple columns
    // as a result of multi-column layout. In this case, we need to compute "the axis-aligned
    // bounding rectangle of the fragments' border boxes" and make that our anchorHeight/Width.
    // We also need to adjust the anchor's top left location to match that of the bounding box
    // instead of the first fragment.
    if (CheckedPtr fragmentedFlow = anchorBox->enclosingFragmentedFlow();
        fragmentedFlow && fragmentedFlow->isDescendantOf(&containingBlock))
        return boundingRectForFragmentedAnchor(anchorBox, containingBlock, *fragmentedFlow);

    auto anchorWidth = anchorBox->offsetWidth();
    auto anchorHeight = anchorBox->offsetHeight();
    auto anchorLocation = LayoutPoint { offsetFromAncestorContainer(anchorBox, containingBlock) };

    if (&containingBlock == &containingBlock.view() && anchoredBox.isFixedPositioned()) {
        // Handle fixed positioning x scrolling anchor.
        bool isFixedAnchor = false;
        for (const RenderElement* box = anchorBox.ptr(); box && box != &containingBlock; box = box->container()) {
            if (box->isFixedPositioned()) {
                isFixedAnchor = true;
                break;
            }
        }
        if (!isFixedAnchor) {
            auto& view = anchorBox->view().frameView();
            anchorLocation.moveBy(-view.constrainedScrollPosition(ScrollPosition(view.scrollPositionRespectingCustomFixedPosition())));
        }
    }

    if (CheckedPtr containingBox = dynamicDowncast<RenderBox>(containingBlock)) {
        if (containingBox->shouldPlaceVerticalScrollbarOnLeft())
            anchorLocation.move(-containingBox->verticalScrollbarWidth(), 0);
    }

    return LayoutRect(anchorLocation, LayoutSize(anchorWidth, anchorHeight));
}

static bool inline isInsetPropertyContainerStartSide(CSSPropertyID insetPropertyID, PositionedLayoutConstraints& constraints)
{
    switch (insetPropertyID) {
    case CSSPropertyLeft:
        return constraints.containingWritingMode().isAnyLeftToRight();
    case CSSPropertyRight:
        return !constraints.containingWritingMode().isAnyLeftToRight();
    case CSSPropertyTop:
        return constraints.containingWritingMode().isAnyTopToBottom();
    case CSSPropertyBottom:
        return !constraints.containingWritingMode().isAnyTopToBottom();
    case CSSPropertyInsetInlineStart:
        return !constraints.selfWritingMode().isInlineOpposing(constraints.containingWritingMode());
    case CSSPropertyInsetInlineEnd:
        return constraints.selfWritingMode().isInlineOpposing(constraints.containingWritingMode());
    case CSSPropertyInsetBlockStart:
        return !constraints.selfWritingMode().isBlockOpposing(constraints.containingWritingMode());
    case CSSPropertyInsetBlockEnd:
        return constraints.selfWritingMode().isBlockOpposing(constraints.containingWritingMode());
    default:
        ASSERT_NOT_REACHED();
        return true;
    }
}

static CSSPropertyID getOppositeInset(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyLeft:
        return CSSPropertyRight;
    case CSSPropertyRight:
        return CSSPropertyLeft;
    case CSSPropertyTop:
        return CSSPropertyBottom;
    case CSSPropertyBottom:
        return CSSPropertyTop;

    case CSSPropertyInsetInlineStart:
        return CSSPropertyInsetInlineEnd;
    case CSSPropertyInsetInlineEnd:
        return CSSPropertyInsetInlineStart;
    case CSSPropertyInsetBlockStart:
        return CSSPropertyInsetBlockEnd;
    case CSSPropertyInsetBlockEnd:
        return CSSPropertyInsetBlockStart;

    default:
        ASSERT_NOT_REACHED();
        return CSSPropertyInsetBlockStart;
    }
}

static std::pair<CSSPropertyID, bool> applyTryTacticsToInset(CSSPropertyID propertyID, WritingMode writingMode, const BuilderPositionTryFallback& fallback)
{
    bool isFlipped = false;
    for (auto tactic : fallback.tactics) {
        switch (tactic) {
        case PositionTryFallback::Tactic::FlipInline:
            if (LogicalBoxAxis::Inline == mapInsetPropertyToLogicalAxis(propertyID, writingMode)) {
                propertyID = getOppositeInset(propertyID);
                isFlipped = true;
            }
            break;
        case PositionTryFallback::Tactic::FlipBlock:
            if (LogicalBoxAxis::Block == mapInsetPropertyToLogicalAxis(propertyID, writingMode)) {
                propertyID = getOppositeInset(propertyID);
                isFlipped = true;
            }
            break;
        case PositionTryFallback::Tactic::FlipX:
            if (BoxAxis::Horizontal == mapInsetPropertyToPhysicalAxis(propertyID, writingMode)) {
                propertyID = getOppositeInset(propertyID);
                isFlipped = true;
            }
            break;
        case PositionTryFallback::Tactic::FlipY:
            if (BoxAxis::Vertical == mapInsetPropertyToPhysicalAxis(propertyID, writingMode)) {
                propertyID = getOppositeInset(propertyID);
                isFlipped = true;
            }
            break;
        case PositionTryFallback::Tactic::FlipStart:
            propertyID = [&] {
                switch (propertyID) {
                case CSSPropertyInsetInlineStart:
                    return CSSPropertyInsetBlockStart;
                case CSSPropertyInsetInlineEnd:
                    return CSSPropertyInsetBlockEnd;
                case CSSPropertyInsetBlockStart:
                    return CSSPropertyInsetInlineStart;
                case CSSPropertyInsetBlockEnd:
                    return CSSPropertyInsetInlineEnd;

                case CSSPropertyLeft:
                    return writingMode.isAnyLeftToRight() == writingMode.isAnyTopToBottom()
                        ? CSSPropertyTop : CSSPropertyBottom;
                case CSSPropertyRight:
                    return writingMode.isAnyLeftToRight() == writingMode.isAnyTopToBottom()
                        ? CSSPropertyBottom : CSSPropertyTop;
                case CSSPropertyTop:
                    return writingMode.isAnyLeftToRight() == writingMode.isAnyTopToBottom()
                        ? CSSPropertyLeft : CSSPropertyRight;
                case CSSPropertyBottom:
                    return writingMode.isAnyLeftToRight() == writingMode.isAnyTopToBottom()
                        ? CSSPropertyRight : CSSPropertyLeft;

                default:
                    ASSERT_NOT_REACHED();
                    return CSSPropertyInsetBlockStart;
                }
            }();
            break;
        }
    }
    return { propertyID, isFlipped };
}

// "An anchor() function representing a valid anchor function resolves...to the <length> that would
// align the edge of the positioned elements' inset-modified containing block corresponding to the
// property the function appears in with the specified border edge of the target anchor element..."
// See: https://drafts.csswg.org/css-anchor-position-1/#anchor-pos
static LayoutUnit computeInsetValue(CSSPropertyID insetPropertyID, CheckedRef<const RenderBoxModelObject> anchorBox, CheckedRef<const RenderBox> anchorPositionedRenderer, AnchorPositionEvaluator::Side anchorSide, const std::optional<BuilderPositionTryFallback>& positionTryFallback)
{
    auto writingMode = anchorPositionedRenderer->writingMode();
    bool isFlipped = false;
    if (positionTryFallback)
        std::tie(insetPropertyID, isFlipped) = applyTryTacticsToInset(insetPropertyID, writingMode, *positionTryFallback);

    auto selfLogicalAxis = mapInsetPropertyToLogicalAxis(insetPropertyID, writingMode);
    PositionedLayoutConstraints constraints(anchorPositionedRenderer, selfLogicalAxis);

    auto anchorPercentage = [&]() -> double {
        if (std::holds_alternative<CSSValueID>(anchorSide)) {
            auto anchorSideID = std::get<CSSValueID>(anchorSide);
            switch (anchorSideID) {
            case CSSValueCenter:
                return 0.5;
            case CSSValueStart:
                return 0;
            case CSSValueEnd:
                return 1;
            case CSSValueSelfStart:
                return constraints.isOpposing() ? 1 : 0;
            case CSSValueSelfEnd:
                return constraints.isOpposing() ? 0 : 1;

            case CSSValueTop:
                return constraints.containingWritingMode().isAnyTopToBottom() ? 0 : 1;
            case CSSValueBottom:
                return constraints.containingWritingMode().isAnyTopToBottom() ? 1 : 0;
            case CSSValueLeft:
                return constraints.containingWritingMode().isAnyLeftToRight() ? 0 : 1;
            case CSSValueRight:
                return constraints.containingWritingMode().isAnyLeftToRight() ? 1 : 0;

            case CSSValueInside:
                return isInsetPropertyContainerStartSide(insetPropertyID, constraints) != isFlipped ? 0 : 1;
            case CSSValueOutside:
                return isInsetPropertyContainerStartSide(insetPropertyID, constraints) != isFlipped ? 1 : 0;

            default:
                ASSERT_NOT_REACHED();
                return 0;
            }
        } else
            return std::get<double>(anchorSide);
    }();
    if (constraints.startIsBefore() == isFlipped)
        anchorPercentage = 1 - anchorPercentage;

    CheckedPtr containingBlock = anchorPositionedRenderer->container();
    ASSERT(containingBlock);
    auto anchorRect = AnchorPositionEvaluator::computeAnchorRectRelativeToContainingBlock(anchorBox, *containingBlock, anchorPositionedRenderer.get());
    auto anchorRange = constraints.extractRange(anchorRect);

    auto anchorPosition = anchorRange.min() + LayoutUnit(anchorRange.size() * anchorPercentage);
    auto insetValue = isInsetPropertyContainerStartSide(insetPropertyID, constraints) == constraints.startIsBefore()
        ? anchorPosition - constraints.containingRange().min()
        : constraints.containingRange().max() - anchorPosition;
    return insetValue;
}

CheckedPtr<RenderBoxModelObject> AnchorPositionEvaluator::findAnchorForAnchorFunctionAndAttemptResolution(BuilderState& builderState, std::optional<ScopedName> anchorNameArgument)
{
    auto& style = builderState.style();
    style.setUsesAnchorFunctions();

    if (!builderState.anchorPositionedStates())
        return { };

    auto isValid = [&] {
        if (!builderState.element())
            return false;

        // FIXME: Support remaining box generating pseudo-elements (like ::marker).
        auto pseudoElement = style.pseudoElementType();
        if (pseudoElement && pseudoElement != PseudoElementType::Before && pseudoElement != PseudoElementType::After)
            return false;

        return true;
    };

    if (!isValid())
        return { };

    Ref elementOrHost = *builderState.element();

    // PseudoElement nodes are created on-demand by render tree builder so dont' work as keys here.
    auto& anchorPositionedStates = *builderState.anchorPositionedStates();
    auto& anchorPositionedState = *anchorPositionedStates.ensure({ elementOrHost.ptr(), style.pseudoElementIdentifier() }, [&] {
        return WTF::makeUnique<AnchorPositionedState>();
    }).iterator->value.get();

    auto scopedAnchorName = [&] {
        if (anchorNameArgument)
            return *anchorNameArgument;
        return defaultAnchorName(style);
    };

    auto resolvedAnchorName = ResolvedScopedName::createFromScopedName(elementOrHost, scopedAnchorName());

    // Collect anchor names that this element refers to in anchor() or anchor-size()
    bool isNewAnchorName = anchorPositionedState.anchorNames.add(resolvedAnchorName).isNewEntry;

    // If anchor resolution has progressed past FindAnchors, and we pick up a new anchor name, set the
    // stage back to FindAnchors. This restarts the resolution process to resolve newly added names.
    if (isNewAnchorName)
        anchorPositionedState.stage = AnchorPositionResolutionStage::FindAnchors;

    // An anchor() instance will be ready to be resolved when all referenced anchor-names
    // have been mapped to an actual anchor element in the DOM tree. At that point, we
    // should also have layout information for the anchor-positioned element alongside
    // the anchors referenced by the anchor-positioned element. Until then, we cannot
    // resolve this anchor() instance.
    if (anchorPositionedState.stage <= AnchorPositionResolutionStage::FindAnchors)
        return { };

    auto anchorPositionedElement = anchorPositionedElementOrPseudoElement(builderState);

    CheckedPtr anchorPositionedRenderer = anchorPositionedElement ? anchorPositionedElement->renderer() : nullptr;
    if (!anchorPositionedRenderer) {
        // If no render tree information is present, the procedure is finished.
        anchorPositionedState.stage = AnchorPositionResolutionStage::Resolved;
        return { };
    }

    // Anchor value may now be resolved using layout information

    RefPtr anchorElement = anchorPositionedState.anchorElements.get(resolvedAnchorName);
    if (!anchorElement) {
        // See: https://drafts.csswg.org/css-anchor-position-1/#valid-anchor-function
        anchorPositionedState.stage = AnchorPositionResolutionStage::Resolved;

        return { };
    }

    if (auto* state = anchorPositionedStates.get(keyForElementOrPseudoElement(*anchorElement))) {
        // Check if the anchor is itself anchor-positioned but hasn't been positioned yet.
        if (state->stage < AnchorPositionResolutionStage::Positioned)
            return { };
    }

    anchorPositionedState.stage = AnchorPositionResolutionStage::Resolved;

    return dynamicDowncast<RenderBoxModelObject>(anchorElement->renderer());
}

bool AnchorPositionEvaluator::propertyAllowsAnchorFunction(CSSPropertyID propertyID)
{
    return CSSProperty::isInsetProperty(propertyID);
}

std::optional<double> AnchorPositionEvaluator::evaluate(BuilderState& builderState, std::optional<ScopedName> elementName, Side side)
{
    auto& style = builderState.style();

    auto propertyID = builderState.cssPropertyID();
    auto physicalAxis = mapInsetPropertyToPhysicalAxis(propertyID, style.writingMode());

    // https://drafts.csswg.org/css-anchor-position-1/#anchor-valid
    auto isValidAnchor = [&] {
        // It’s being used in an inset property...
        if (!propertyAllowsAnchorFunction(propertyID))
            return false;

        // ...on an absolutely-positioned element.
        if (!style.hasOutOfFlowPosition())
            return false;

        // If its <anchor-side> specifies a physical keyword, it’s being used in an inset property in that axis.
        // (For example, left can only be used in left, right, or a logical inset property in the horizontal axis.)
        if (auto* sideID = std::get_if<CSSValueID>(&side); sideID && !anchorSideMatchesInsetProperty(*sideID, physicalAxis))
            return false;

        return true;
    };

    if (!isValidAnchor())
        return { };

    auto anchorRenderer = findAnchorForAnchorFunctionAndAttemptResolution(builderState, elementName);
    if (!anchorRenderer)
        return { };

    RefPtr anchorPositionedElement = anchorPositionedElementOrPseudoElement(builderState);
    if (!anchorPositionedElement)
        return { };

    CheckedPtr anchorPositionedRenderer = dynamicDowncast<RenderBox>(anchorPositionedElement->renderer());
    if (!anchorPositionedRenderer)
        return { };

    addAnchorFunctionScrollCompensatedAxis(style, *anchorPositionedRenderer, *anchorRenderer, physicalAxis);

    // Proceed with computing the inset value for the specified inset property.
    double insetValue = computeInsetValue(propertyID, *anchorRenderer, *anchorPositionedRenderer, side, builderState.positionTryFallback());

    // Adjust for CSS `zoom` property and page zoom.
    return insetValue / style.usedZoom();
}

// Returns the default anchor size dimension to use when it is not specified in
// anchor-size(). This matches the axis of the property that anchor-size() is used in.
static AnchorSizeDimension defaultDimensionForPropertyID(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyWidth:
    case CSSPropertyMinWidth:
    case CSSPropertyMaxWidth:
    case CSSPropertyLeft:
    case CSSPropertyRight:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
        return AnchorSizeDimension::Width;

    case CSSPropertyHeight:
    case CSSPropertyMinHeight:
    case CSSPropertyMaxHeight:
    case CSSPropertyTop:
    case CSSPropertyBottom:
    case CSSPropertyMarginTop:
    case CSSPropertyMarginBottom:
        return AnchorSizeDimension::Height;

    case CSSPropertyBlockSize:
    case CSSPropertyMinBlockSize:
    case CSSPropertyMaxBlockSize:
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetBlockEnd:
    case CSSPropertyMarginBlockStart:
    case CSSPropertyMarginBlockEnd:
        return AnchorSizeDimension::Block;

    case CSSPropertyInlineSize:
    case CSSPropertyMinInlineSize:
    case CSSPropertyMaxInlineSize:
    case CSSPropertyInsetInlineStart:
    case CSSPropertyInsetInlineEnd:
    case CSSPropertyMarginInlineStart:
    case CSSPropertyMarginInlineEnd:
        return AnchorSizeDimension::Inline;

    default:
        ASSERT_NOT_REACHED("anchor-size() being used in disallowed CSS property, which should not happen");
        return AnchorSizeDimension::Width;
    }
}

// Convert anchor size dimension to the physical dimension (width or height).
static BoxAxis anchorSizeDimensionToPhysicalDimension(AnchorSizeDimension dimension, const RenderStyle& style, const RenderStyle& containerStyle)
{
    switch (dimension) {
    case AnchorSizeDimension::Width:
        return BoxAxis::Horizontal;
    case AnchorSizeDimension::Height:
        return BoxAxis::Vertical;
    case AnchorSizeDimension::Block:
        return mapAxisLogicalToPhysical(containerStyle.writingMode(), LogicalBoxAxis::Block);
    case AnchorSizeDimension::Inline:
        return mapAxisLogicalToPhysical(containerStyle.writingMode(), LogicalBoxAxis::Inline);
    case AnchorSizeDimension::SelfBlock:
        return mapAxisLogicalToPhysical(style.writingMode(), LogicalBoxAxis::Block);
    case AnchorSizeDimension::SelfInline:
        return mapAxisLogicalToPhysical(style.writingMode(), LogicalBoxAxis::Inline);
    }

    ASSERT_NOT_REACHED();
    return BoxAxis::Horizontal;
}

bool AnchorPositionEvaluator::propertyAllowsAnchorSizeFunction(CSSPropertyID propertyID)
{
    return CSSProperty::isSizingProperty(propertyID) || CSSProperty::isInsetProperty(propertyID) || CSSProperty::isMarginProperty(propertyID);
}

std::optional<double> AnchorPositionEvaluator::evaluateSize(BuilderState& builderState, std::optional<ScopedName> elementName, std::optional<AnchorSizeDimension> dimension)
{
    auto propertyID = builderState.cssPropertyID();
    const auto& style = builderState.style();

    auto isValidAnchorSize = [&] {
        // It’s being used in a sizing property, an inset property, or a margin property...
        if (!propertyAllowsAnchorSizeFunction(propertyID))
            return false;

        // ...on an absolutely-positioned element.
        if (!style.hasOutOfFlowPosition())
            return false;

        return true;
    };

    if (!isValidAnchorSize())
        return { };

    auto anchorRenderer = findAnchorForAnchorFunctionAndAttemptResolution(builderState, elementName);
    if (!anchorRenderer)
        return { };

    // Resolve the dimension (width or height) to return from the anchor positioned element.
    RefPtr anchorPositionedElement = anchorPositionedElementOrPseudoElement(builderState);
    if (!anchorPositionedElement)
        return { };

    CheckedPtr anchorPositionedRenderer = anchorPositionedElement->renderer();
    ASSERT(anchorPositionedRenderer);

    CheckedPtr anchorPositionedContainerRenderer = anchorPositionedRenderer->container();
    ASSERT(anchorPositionedContainerRenderer);

    auto resolvedDimension = dimension.value_or(defaultDimensionForPropertyID(propertyID));
    auto physicalDimension = anchorSizeDimensionToPhysicalDimension(resolvedDimension, anchorPositionedRenderer->style(), anchorPositionedContainerRenderer->style());

    if (builderState.positionTryFallback()) {
        // "For sizing properties, change the specified axis in anchor-size() functions to maintain the same relative relationship to the new direction that they had to the old."
        if (CSSProperty::isSizingProperty(propertyID)) {
            auto swapDimensions = builderState.positionTryFallback()->tactics.contains(PositionTryFallback::Tactic::FlipStart);
            if (swapDimensions)
                physicalDimension = oppositeAxis(physicalDimension);
        }
    }

    auto anchorBorderBoundingBox = anchorRenderer->borderBoundingBox();
    if (CheckedPtr fragmentedFlow = anchorRenderer->enclosingFragmentedFlow()) {
        CheckedPtr containingBlock = anchorPositionedRenderer->containingBlock();
        if (fragmentedFlow && containingBlock
            && fragmentedFlow->isDescendantOf(containingBlock.get()))
            anchorBorderBoundingBox = boundingRectForFragmentedAnchor(*anchorRenderer, *containingBlock, *fragmentedFlow);
    }

    // Adjust for CSS `zoom` property and page zoom.

    switch (physicalDimension) {
    case BoxAxis::Horizontal:
        return anchorBorderBoundingBox.width() / style.usedZoom();
    case BoxAxis::Vertical:
        return anchorBorderBoundingBox.height() / style.usedZoom();
    }

    ASSERT_NOT_REACHED();
    return { };
}

static const RenderElement* penultimateContainingBlockChainElement(const RenderElement& descendant, const RenderElement* ancestor)
{
    auto* currentElement = &descendant;
    for (auto* nextElement = currentElement->container(); nextElement; nextElement = nextElement->container()) {
        if (nextElement == ancestor)
            return currentElement;
        currentElement = nextElement;
    }
    return nullptr;
}

static bool firstChildPrecedesSecondChild(const RenderObject* firstChild, const RenderObject* secondChild, const RenderObject* containingBlock)
{
    HashSet<CheckedRef<const RenderObject>> firstAncestorChain;

    for (auto* first = firstChild; first; first = first->parent()) {
        firstAncestorChain.add(*first);
        if (first == containingBlock)
            break;
    }

    auto* second = secondChild;
    for (; second != containingBlock; second = second->parent()) {
        if (firstAncestorChain.contains(second->parent())) {
            for (auto* sibling = second->previousSibling(); sibling; sibling = sibling->previousSibling()) {
                if (firstAncestorChain.contains(sibling))
                    return true;
            }
            return false;
        }
    }
    return false;
}

// Given an element and its anchor name, locate the closest ancestor (*) element
// that establishes an anchor scope affecting this anchor name, and return the pointer
// to such element. If no ancestor establishes an anchor scope affecting this name,
// returns nullptr.
// (*): an anchor element can also establish an anchor scope containing itself. In this
// case, the return value is itself.
static CheckedPtr<const Element> anchorScopeForAnchorName(const RenderBoxModelObject& renderer, const ResolvedScopedName anchorName)
{
    // Traverse up the composed tree through itself and each ancestor.
    CheckedPtr<const Element> anchorElement = renderer.element();
    ASSERT(anchorElement);
    for (CheckedPtr<const Element> currentAncestor = anchorElement; currentAncestor; currentAncestor = currentAncestor->parentElementInComposedTree()) {
        CheckedPtr currentAncestorStyle = currentAncestor->renderStyle();
        if (!currentAncestorStyle)
            continue;
        const auto& currentAncestorAnchorScope = currentAncestorStyle->anchorScope();

        if (Style::NameScope::Type::None == currentAncestorAnchorScope.type)
            continue;

        auto styleScope = Scope::forOrdinal(*currentAncestor, currentAncestorAnchorScope.scopeOrdinal);
        ASSERT(styleScope);
        if (anchorName.scopeIdentifier() != styleScope->identifier())
            continue;

        if (Style::NameScope::Type::All == currentAncestorAnchorScope.type
            || currentAncestorAnchorScope.names.contains(CustomIdentifier { anchorName.name() }))
            return currentAncestor;
    }

    return nullptr;
}

enum class TopLayerStatus : uint8_t { Same, Lower, Higher };
static TopLayerStatus computeTopLayerStatus(const RenderBox& anchored, const RenderBoxModelObject& anchor)
{
    // Two elements are in the same top layer if they have the same top layer root (including if both are none).
    // An element A is in a higher top layer than an element B if A has a top layer root, and either B has a top
    // layer root earlier in the top layer than A’s, or B doesn’t have a top layer root at all.
    // https://drafts.csswg.org/css-position-4/#top-layer

    if (!anchored.document().hasTopLayerElement())
        return TopLayerStatus::Same;

    auto topLayerRoot = [&](auto& renderer) -> const RenderLayerModelObject* {
        for (auto* layer = renderer.enclosingLayer(); layer; layer = layer->parent()) {
            if (layer->establishesTopLayer())
                return &layer->renderer();
        }
        return nullptr;
    };

    auto* anchoredRoot = topLayerRoot(anchored);
    auto* anchorRoot = topLayerRoot(anchor);
    if (anchoredRoot == anchorRoot)
        return TopLayerStatus::Same;
    if (!anchoredRoot)
        return TopLayerStatus::Lower;
    if (!anchorRoot)
        return TopLayerStatus::Higher;

    auto& topLayerElements = anchored.document().topLayerElements();
    for (auto& topLayerElement : topLayerElements) {
        if (topLayerElement.ptr() == anchoredRoot->element())
            return TopLayerStatus::Lower;
        if (topLayerElement.ptr() == anchorRoot->element())
            return TopLayerStatus::Higher;
    }
    return TopLayerStatus::Lower;
}

// See: https://drafts.csswg.org/css-anchor-position-1/#acceptable-anchor-element
static bool isAcceptableAnchorElement(const RenderBoxModelObject& anchorRenderer, Ref<const Element> anchorPositionedElement, const std::optional<ResolvedScopedName> anchorName = { })
{
    // "Possible anchor is either an element or a fully styleable tree-abiding pseudo-element."
    // This always have an associated Element (for ::before/::after it is PseudoElement).
    if (!anchorRenderer.element())
        return false;

    CheckedPtr anchorPositionedRenderer = dynamicDowncast<RenderBox>(anchorPositionedElement->renderer());
    ASSERT(anchorPositionedRenderer);

    if (anchorName) {
        // Check that anchorRenderer has the specified name.
        ASSERT(anchorRenderer.style().anchorNames().containsIf(
            [anchorName](auto& scopedName) { return scopedName.name == anchorName->name(); }
        ));

        // The anchor and anchor-positioned element must be in the same scope.
        auto anchorScopeElement = anchorScopeForAnchorName(anchorRenderer, *anchorName);
        auto anchorPositionedScopeElement = anchorScopeForAnchorName(*anchorPositionedRenderer, *anchorName);
        if (anchorScopeElement != anchorPositionedScopeElement)
            return false;
    }

    CheckedPtr containingBlock = anchorPositionedRenderer->container();
    ASSERT(containingBlock);

    // "possible anchor is laid out strictly before positioned el, aka one of the following is true:"
    auto topLayerStatus = computeTopLayerStatus(*anchorPositionedRenderer, anchorRenderer);
    switch (topLayerStatus) {
    case TopLayerStatus::Higher:
        // "- positioned el is in a higher top layer than possible anchor"
        return true;
    case TopLayerStatus::Same: {
        // "- Both elements are in the same top layer..."
        auto* penultimateElement = penultimateContainingBlockChainElement(anchorRenderer, containingBlock.get());
        if (!penultimateElement)
            return false;

        if (!penultimateElement->isOutOfFlowPositioned())
            return true;

        return firstChildPrecedesSecondChild(penultimateElement, anchorPositionedRenderer.get(), containingBlock.get());
    }
    case TopLayerStatus::Lower:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static RefPtr<Element> findImplicitAnchor(const Element& anchorPositionedElement)
{
    auto find = [&]() -> RefPtr<Element> {
        // "The implicit anchor element of a pseudo-element is its originating element, unless otherwise specified."
        // https://drafts.csswg.org/css-anchor-position-1/#implicit
        if (auto pseudoElement = dynamicDowncast<PseudoElement>(anchorPositionedElement))
            return pseudoElement->hostElement();

        // https://html.spec.whatwg.org/multipage/popover.html#the-popover-attribute
        // 24. Set element's implicit anchor element to invoker.
        if (auto popoverData = anchorPositionedElement.popoverData())
            return popoverData->invoker();

        return nullptr;
    };

    if (auto implicitAnchorElement = find()) {
        // "If [a spec] defines is an implicit anchor element for query el which is an acceptable anchor element for query el, return that element."
        // https://drafts.csswg.org/css-anchor-position-1/#target
        CheckedPtr anchor = dynamicDowncast<RenderBoxModelObject>(implicitAnchorElement->renderer());
        if (anchor && isAcceptableAnchorElement(*anchor, anchorPositionedElement))
            return implicitAnchorElement;
    }

    return nullptr;
}

static RefPtr<Element> findLastAcceptableAnchorWithName(ResolvedScopedName anchorName, const Element& anchorPositionedElement, const AnchorsForAnchorName& anchorsForAnchorName)
{
    if (anchorName.name() == implicitAnchorElementName().name)
        return findImplicitAnchor(anchorPositionedElement);

    const auto& anchors = anchorsForAnchorName.get(anchorName);

    for (auto& anchor : anchors | std::views::reverse) {
        if (isAcceptableAnchorElement(anchor.get(), anchorPositionedElement, anchorName))
            return anchor->element();
    }

    return { };
}

static AnchorsForAnchorName collectAnchorsForAnchorName(const Document& document)
{
    if (!document.renderView())
        return { };

    AnchorsForAnchorName anchorsForAnchorName;

    auto& anchors = document.renderView()->anchors();
    for (auto& anchorRenderer : anchors) {
        CheckedPtr anchorElement = anchorRenderer.element();
        ASSERT(anchorElement);

        for (auto& scopedName : anchorRenderer.style().anchorNames()) {
            auto resolvedScopedName = ResolvedScopedName::createFromScopedName(*anchorElement, scopedName);

            anchorsForAnchorName.ensure(resolvedScopedName, [&] {
                return AnchorsForAnchorName::MappedType { };
            }).iterator->value.append(anchorRenderer);
        }
    }

    // Sort them in tree order.
    for (auto& anchors : anchorsForAnchorName.values()) {
        std::ranges::sort(anchors, [](auto& a, auto& b) {
            // FIXME: Figure out anonymous pseudo-elements.
            if (!a->element() || !b->element())
                return !!b->element();
            return is_lt(treeOrder<ComposedTree>(*a->element(), *b->element()));
        });
    }

    return anchorsForAnchorName;
}

AnchorElements AnchorPositionEvaluator::findAnchorsForAnchorPositionedElement(const Element& anchorPositionedElement, const HashSet<ResolvedScopedName>& anchorNames, const AnchorsForAnchorName& anchorsForAnchorName)
{
    AnchorElements anchorElements;

    for (auto& anchorName : anchorNames) {
        auto anchor = findLastAcceptableAnchorWithName(anchorName, anchorPositionedElement, anchorsForAnchorName);
        anchorElements.add(anchorName, anchor);
    }

    return anchorElements;
}

void AnchorPositionEvaluator::updateAnchorPositioningStatesAfterInterleavedLayout(Document& document, AnchorPositionedStates& anchorPositionedStates)
{
    if (anchorPositionedStates.isEmpty())
        return;

    // FIXME: Make the code below oeprate on renderers (boxes) rather than elements.
    auto anchorsForAnchorName = collectAnchorsForAnchorName(document);

    for (auto& elementAndState : anchorPositionedStates) {
        auto& state = *elementAndState.value;

        switch (state.stage) {
        case AnchorPositionResolutionStage::FindAnchors: {
            RefPtr element = elementAndState.key.first;
            if (elementAndState.key.second)
                element = element->pseudoElementIfExists(*elementAndState.key.second);

            CheckedPtr renderer = element ? element->renderer() : nullptr;
            if (renderer) {
                // FIXME: Remove the redundant anchorElements member. The mappings are available in anchorPositionedToAnchorMap.
                state.anchorElements = findAnchorsForAnchorPositionedElement(*element, state.anchorNames, anchorsForAnchorName);
                if (isLayoutTimeAnchorPositioned(renderer->style()))
                    renderer->setNeedsLayout();

                Vector<ResolvedAnchor> anchors;
                for (auto& anchorNameAndElement : state.anchorElements) {
                    CheckedPtr anchorElement = anchorNameAndElement.value.get();
                    anchors.append(ResolvedAnchor {
                        .renderer = anchorElement ? dynamicDowncast<RenderBoxModelObject>(anchorElement->renderer()) : nullptr,
                        .name = anchorNameAndElement.key
                    });
                }
                document.styleScope().anchorPositionedToAnchorMap().set(*element, AnchorPositionedToAnchorEntry {
                    .pseudoElementIdentifier = elementAndState.key.second,
                    .anchors = WTFMove(anchors)
                });
            }
            state.stage = renderer && renderer->style().usesAnchorFunctions() ? AnchorPositionResolutionStage::ResolveAnchorFunctions : AnchorPositionResolutionStage::Resolved;
            break;
        }

        case AnchorPositionResolutionStage::ResolveAnchorFunctions:
        case AnchorPositionResolutionStage::Resolved:
            if (CheckedPtr anchored = elementAndState.key.first->renderer()) {
                if (auto anchoredBox = dynamicDowncast<RenderBox>(anchored.get()))
                    AnchorPositionEvaluator::captureScrollSnapshots(*anchoredBox, false);
            }
            state.stage = AnchorPositionResolutionStage::Positioned;
            break;

        case AnchorPositionResolutionStage::Positioned:
            break;
        }
    }
}

void AnchorPositionEvaluator::updateAnchorPositionedStateForDefaultAnchorAndPositionVisibility(Element& element, const RenderStyle& style, AnchorPositionedStates& states)
{
    auto shouldResolveDefaultAnchor = isAnchorPositioned(style);

    // `position-visibility: no-overflow` should also work for non-anchor positioned out-of-flow boxes.
    // Create an empty anchor positioning state for it so we perform the required layout interleaving.
    auto hasPositionVisibilityNoOverflow = generatesBox(style) && style.hasOutOfFlowPosition() && style.positionVisibility().contains(PositionVisibilityValue::NoOverflow);

    if (!shouldResolveDefaultAnchor && !hasPositionVisibilityNoOverflow)
        return;

    auto* state = states.ensure({ &element, style.pseudoElementIdentifier() }, [&] {
        return makeUnique<AnchorPositionedState>();
    }).iterator->value.get();

    if (shouldResolveDefaultAnchor) {
        // Always resolve the default anchor. Even if nothing is anchored to it we need it to compute the scroll compensation.
        auto resolvedDefaultAnchor = ResolvedScopedName::createFromScopedName(element, defaultAnchorName(style));
        if (state->anchorNames.add(resolvedDefaultAnchor).isNewEntry) {
            // If anchor resolution has progressed past FindAnchors, and we pick up a new anchor name, set the
            // stage back to FindAnchors. This restarts the resolution process to resolve newly added names.
            state->stage = AnchorPositionResolutionStage::FindAnchors;
        }
    }
}

auto AnchorPositionEvaluator::makeAnchorPositionedForAnchorMap(AnchorPositionedToAnchorMap& toAnchorMap) -> AnchorToAnchorPositionedMap
{
    AnchorToAnchorPositionedMap map;

    for (auto elementAndAnchors : toAnchorMap) {
        CheckedRef anchorPositionedElement = elementAndAnchors.key;
        for (auto& anchor : elementAndAnchors.value.anchors) {
            if (!anchor.renderer)
                continue;
            map.ensure(*anchor.renderer, [&] {
                return Vector<Ref<Element>> { };
            }).iterator->value.append(anchorPositionedElement);
        }
    }
    return map;
}

bool AnchorPositionEvaluator::isAnchorPositioned(const RenderStyle& style)
{
    return isStyleTimeAnchorPositioned(style) || isLayoutTimeAnchorPositioned(style);
}

bool AnchorPositionEvaluator::isStyleTimeAnchorPositioned(const RenderStyle& style)
{
    if (!generatesBox(style) || !style.hasOutOfFlowPosition())
        return false;

    return style.usesAnchorFunctions();
}

bool AnchorPositionEvaluator::isLayoutTimeAnchorPositioned(const RenderStyle& style)
{
    if (!generatesBox(style) || !style.hasOutOfFlowPosition())
        return false;

    if (!style.positionArea().isNone())
        return true;

    return style.justifySelf().isAnchorCenter() || style.alignSelf().isAnchorCenter();
}

static CSSPropertyID flipHorizontal(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyLeft:
        return CSSPropertyRight;
    case CSSPropertyRight:
        return CSSPropertyLeft;
    case CSSPropertyMarginLeft:
        return CSSPropertyMarginRight;
    case CSSPropertyMarginRight:
        return CSSPropertyMarginLeft;
    default:
        return propertyID;
    }
}

static CSSPropertyID flipVertical(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyTop:
        return CSSPropertyBottom;
    case CSSPropertyBottom:
        return CSSPropertyTop;
    case CSSPropertyMarginTop:
        return CSSPropertyMarginBottom;
    case CSSPropertyMarginBottom:
        return CSSPropertyMarginTop;
    default:
        return propertyID;
    }
}

static CSSPropertyID flipStart(CSSPropertyID propertyID, WritingMode writingMode)
{
    auto logicalProperty = CSSProperty::unresolvePhysicalProperty(propertyID, writingMode);

    auto flippedLogical = [&] {
        switch (logicalProperty) {
        case CSSPropertyInsetBlockStart:
            return CSSPropertyInsetInlineStart;
        case CSSPropertyInsetBlockEnd:
            return CSSPropertyInsetInlineEnd;
        case CSSPropertyBlockSize:
            return CSSPropertyInlineSize;
        case CSSPropertyMinBlockSize:
            return CSSPropertyMinInlineSize;
        case CSSPropertyMaxBlockSize:
            return CSSPropertyMaxInlineSize;
        case CSSPropertyInsetInlineStart:
            return CSSPropertyInsetBlockStart;
        case CSSPropertyInsetInlineEnd:
            return CSSPropertyInsetBlockEnd;
        case CSSPropertyInlineSize:
            return CSSPropertyBlockSize;
        case CSSPropertyMinInlineSize:
            return CSSPropertyMinBlockSize;
        case CSSPropertyMaxInlineSize:
            return CSSPropertyMaxBlockSize;
        case CSSPropertyMarginBlockStart:
            return CSSPropertyMarginInlineStart;
        case CSSPropertyMarginBlockEnd:
            return CSSPropertyMarginInlineEnd;
        case CSSPropertyMarginInlineStart:
            return CSSPropertyMarginBlockStart;
        case CSSPropertyMarginInlineEnd:
            return CSSPropertyMarginBlockEnd;
        case CSSPropertyAlignSelf:
            return CSSPropertyJustifySelf;
        case CSSPropertyJustifySelf:
            return CSSPropertyAlignSelf;
        default:
            return propertyID;
        }
    };
    return CSSProperty::resolveDirectionAwareProperty(flippedLogical(), writingMode);
}

CSSPropertyID AnchorPositionEvaluator::resolvePositionTryFallbackProperty(CSSPropertyID propertyID, WritingMode writingMode, const BuilderPositionTryFallback& fallback)
{
    ASSERT(!CSSProperty::isDirectionAwareProperty(propertyID));

    for (auto tactic : fallback.tactics) {
        switch (tactic) {
        case PositionTryFallback::Tactic::FlipInline:
            propertyID = writingMode.isHorizontal() ? flipHorizontal(propertyID) : flipVertical(propertyID);
            break;
        case PositionTryFallback::Tactic::FlipBlock:
            propertyID = writingMode.isHorizontal() ? flipVertical(propertyID) : flipHorizontal(propertyID);
            break;
        case PositionTryFallback::Tactic::FlipX:
            propertyID = flipHorizontal(propertyID);
            break;
        case PositionTryFallback::Tactic::FlipY:
            propertyID = flipVertical(propertyID);
            break;
        case PositionTryFallback::Tactic::FlipStart:
            propertyID = flipStart(propertyID, writingMode);
            break;
        }
    }
    return propertyID;
}

CSSValueID AnchorPositionEvaluator::resolvePositionTryFallbackValueForSelfPosition(CSSPropertyID propertyID, CSSValueID position, WritingMode writingMode, const BuilderPositionTryFallback& fallback)
{
    // Implements the bullet starting "For the self-alignment properties" from step 4 of https://drafts.csswg.org/css-anchor-position-1/#swap-due-to-a-try-tactic.

    ASSERT(propertyID == CSSPropertyAlignSelf || propertyID == CSSPropertyJustifySelf);

    auto flipSidedPosition = [](auto position) -> CSSValueID {
        // Swap to the "opposite" position if the current position is "sided".
        // If there is no opposite value, nothing is changed.
        switch (position) {
        case CSSValueStart:
            return CSSValueEnd;
        case CSSValueEnd:
            return CSSValueStart;
        case CSSValueSelfStart:
            return CSSValueSelfEnd;
        case CSSValueSelfEnd:
            return CSSValueSelfStart;
        case CSSValueFlexStart:
            return CSSValueFlexEnd;
        case CSSValueFlexEnd:
            return CSSValueFlexStart;
        case CSSValueLeft:
            return CSSValueRight;
        case CSSValueRight:
            return CSSValueLeft;
        default:
            return position;
        }
    };

    auto flipStart = [](auto writingMode, auto position) -> CSSValueID {
        // `justify-self` additionally takes `left`/`right`, `align-self` doesn't. When
        // applying `flip-start`, `justify-self` gets swapped with `align-self` (see
        // call to `flipStart` in `AnchorPositionEvaluator::resolvePositionTryFallbackProperty`).
        // So if we're resolving `justify-self` (which later gets swapped with `align-self`),
        // and the position is `left`/`right`, resolve it to `self-start`/`self-end`.
        switch (position) {
        case CSSValueLeft:
            return writingMode.bidiDirection() == TextDirection::LTR ? CSSValueSelfStart : CSSValueSelfEnd;
        case CSSValueRight:
            return writingMode.bidiDirection() == TextDirection::LTR ? CSSValueSelfEnd : CSSValueSelfStart;
        default:
            return position;
        }
    };

    for (auto tactic : fallback.tactics) {
        switch (tactic) {
        case PositionTryFallback::Tactic::FlipBlock:
            if (propertyID == CSSPropertyAlignSelf)
                position = flipSidedPosition(position);
            break;
        case PositionTryFallback::Tactic::FlipInline:
            if (propertyID == CSSPropertyJustifySelf)
                position = flipSidedPosition(position);
            break;
        case PositionTryFallback::Tactic::FlipX:
            if (propertyID == (writingMode.isHorizontal() ? CSSPropertyJustifySelf : CSSPropertyAlignSelf))
                position = flipSidedPosition(position);
            break;
        case PositionTryFallback::Tactic::FlipY:
            if (propertyID == (writingMode.isHorizontal() ? CSSPropertyAlignSelf : CSSPropertyJustifySelf))
                position = flipSidedPosition(position);
            break;
        case PositionTryFallback::Tactic::FlipStart:
            if (propertyID == CSSPropertyJustifySelf)
                position = flipStart(writingMode, position);
            break;
        }
    }

    return position;
}

bool AnchorPositionEvaluator::overflowsInsetModifiedContainingBlock(const RenderBox& anchoredBox)
{
    if (!anchoredBox.isOutOfFlowPositioned())
        return false;

    auto inlineConstraints = PositionedLayoutConstraints { anchoredBox, LogicalBoxAxis::Inline };
    auto blockConstraints = PositionedLayoutConstraints { anchoredBox, LogicalBoxAxis::Block };
    inlineConstraints.computeInsets();
    blockConstraints.computeInsets();

    auto anchorInlineSize = anchoredBox.logicalWidth() + anchoredBox.marginStart() + anchoredBox.marginEnd();
    auto anchorBlockSize = anchoredBox.logicalHeight() + anchoredBox.marginBefore() + anchoredBox.marginAfter();

    return inlineConstraints.insetModifiedContainingSize() < anchorInlineSize
        || blockConstraints.insetModifiedContainingSize() < anchorBlockSize;
}

bool AnchorPositionEvaluator::isDefaultAnchorInvisibleOrClippedByInterveningBoxes(const RenderBox& anchoredBox)
{
    CheckedPtr defaultAnchor = defaultAnchorForBox(anchoredBox);
    if (!defaultAnchor)
        return false;

    if (defaultAnchor->style().usedVisibility() == Visibility::Hidden)
        return true;

    CheckedPtr anchorBox = dynamicDowncast<RenderBox>(*defaultAnchor);

    // https://drafts.csswg.org/css-anchor-position-1/#position-visibility
    // "An anchor box anchor is clipped by intervening boxes relative to a positioned box abspos relying on it if anchor’s ink overflow
    // rectangle is fully clipped by a box which is an ancestor of anchor but a descendant of abspos’s containing block."

    auto localAnchorRect = [&] {
        if (anchorBox)
            return anchorBox->visualOverflowRect();
        return downcast<RenderInline>(*defaultAnchor).linesVisualOverflowBoundingBox();
    }();
    auto* anchoredContainingBlock = anchoredBox.container();

    auto anchorRect = defaultAnchor->localToAbsoluteQuad(FloatQuad { localAnchorRect }).boundingBox();

    for (auto* anchorAncestor = defaultAnchor->container(); anchorAncestor && anchorAncestor != anchoredContainingBlock; anchorAncestor = anchorAncestor->container()) {
        if (!anchorAncestor->hasNonVisibleOverflow())
            continue;
        auto* clipAncestor = dynamicDowncast<RenderBox>(*anchorAncestor);
        if (!clipAncestor)
            continue;
        auto localClipRect = clipAncestor->overflowClipRect({ });
        auto clipRect = clipAncestor->localToAbsoluteQuad(FloatQuad { localClipRect }).boundingBox();
        if (!clipRect.intersects(anchorRect))
            return true;
    }

    if (anchorBox) {
        // Test for chained anchors.
        if (isDefaultAnchorInvisibleOrClippedByInterveningBoxes(*anchorBox))
            return true;
    }

    return false;
}

// FIXME: The code should operate fully on host/pseudoElementIdentifier pairs and not use PseudoElements to
// support pseudo-elements other than ::before/::after.
RefPtr<const Element> AnchorPositionEvaluator::anchorPositionedElementOrPseudoElement(BuilderState& builderState)
{
    RefPtr element = builderState.element();
    if (auto identifier = builderState.style().pseudoElementIdentifier())
        return element->pseudoElementIfExists(*identifier);
    return element;
}

AnchorPositionedKey AnchorPositionEvaluator::keyForElementOrPseudoElement(const Element& element)
{
    if (auto* pseudoElement = dynamicDowncast<PseudoElement>(element))
        return { pseudoElement->hostElement(), PseudoElementIdentifier { pseudoElement->pseudoElementType() } };
    return { &element, { } };
}

bool AnchorPositionEvaluator::isAnchor(const RenderStyle& style)
{
    if (!style.anchorNames().isNone())
        return true;

    return isImplicitAnchor(style);
}

bool AnchorPositionEvaluator::isImplicitAnchor(const RenderStyle& style)
{
    // The invoker is an implicit anchor for the popover.
    // https://drafts.csswg.org/css-anchor-position-1/#implicit
    if (style.isPopoverInvoker())
        return true;

    // "The implicit anchor element of a pseudo-element is its originating element, unless otherwise specified."
    // https://drafts.csswg.org/css-anchor-position-1/#implicit
    auto isImplicitAnchorForPseudoElement = [&](PseudoElementType pseudoElementType) {
        const RenderStyle* pseudoElementStyle = style.getCachedPseudoStyle({ pseudoElementType });
        if (!pseudoElementStyle)
            return false;
        // If we have an explicit anchor name then there is no need for an implicit anchor.
        if (!pseudoElementStyle->positionAnchor().isAuto())
            return false;

        return pseudoElementStyle->usesAnchorFunctions() || isLayoutTimeAnchorPositioned(*pseudoElementStyle);
    };
    return isImplicitAnchorForPseudoElement(PseudoElementType::Before) || isImplicitAnchorForPseudoElement(PseudoElementType::After);
}

ScopedName AnchorPositionEvaluator::defaultAnchorName(const RenderStyle& style)
{
    if (auto name = style.positionAnchor().tryName())
        return *name;
    return implicitAnchorElementName();
}

CheckedPtr<RenderBoxModelObject> AnchorPositionEvaluator::defaultAnchorForBox(const RenderBox& box)
{
    if (!box.element())
        return nullptr;

    CheckedRef element = *box.element();

    auto& anchorPositionedMap = box.document().styleScope().anchorPositionedToAnchorMap();
    auto it = anchorPositionedMap.find(element);
    if (it == anchorPositionedMap.end())
        return nullptr;

    auto anchorName = ResolvedScopedName::createFromScopedName(element, defaultAnchorName(box.style()));

    for (auto& anchor : it->value.anchors) {
        if (anchorName == anchor.name)
            return anchor.renderer.get();
    }
    return nullptr;
}

HashMap<AnchorPositionedKey, size_t> AnchorPositionEvaluator::recordLastSuccessfulPositionOptions(const SingleThreadWeakHashSet<const RenderBox>& positionTryBoxes)
{
    HashMap<Style::AnchorPositionedKey, size_t> lastSuccessfulPositionOptionMap;

    for (const auto& positionTryBox : positionTryBoxes) {
        auto styleable = Styleable::fromRenderer(positionTryBox);
        ASSERT(styleable);

        if (auto usedPositionOptionIndex = positionTryBox.style().usedPositionOptionIndex())
            lastSuccessfulPositionOptionMap.add({ styleable->element, styleable->pseudoElementIdentifier }, *usedPositionOptionIndex);
    }

    return lastSuccessfulPositionOptionMap;
}

} // namespace Style

} // namespace WebCore
