/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "Document.h"
#include "DocumentInlines.h"
#include "Element.h"
#include "Node.h"
#include "RenderBlock.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderFragmentedFlow.h"
#include "RenderInline.h"
#include "RenderStyle.h"
#include "RenderStyleInlines.h"
#include "StyleBuilderState.h"
#include "StyleScope.h"
#include "WritingMode.h"
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/TypeCasts.h>

namespace WebCore::Style {

static BoxAxis mapInsetPropertyToPhysicalAxis(CSSPropertyID id, const RenderStyle& style)
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
        return mapLogicalAxisToPhysicalAxis(makeTextFlow(style.writingMode(), style.direction()), LogicalBoxAxis::Inline);
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetBlockEnd:
        return mapLogicalAxisToPhysicalAxis(makeTextFlow(style.writingMode(), style.direction()), LogicalBoxAxis::Block);
    default:
        ASSERT_NOT_REACHED();
        return BoxAxis::Horizontal;
    }
}

static BoxSide mapInsetPropertyToPhysicalSide(CSSPropertyID id, const RenderStyle& style)
{
    switch (id) {
    case CSSPropertyLeft:
        return BoxSide::Left;
    case CSSPropertyRight:
        return BoxSide::Right;
    case CSSPropertyTop:
        return BoxSide::Top;
    case CSSPropertyBottom:
        return BoxSide::Bottom;
    case CSSPropertyInsetInlineStart:
        return mapLogicalSideToPhysicalSide(makeTextFlow(style.writingMode(), style.direction()), LogicalBoxSide::InlineStart);
    case CSSPropertyInsetInlineEnd:
        return mapLogicalSideToPhysicalSide(makeTextFlow(style.writingMode(), style.direction()), LogicalBoxSide::InlineEnd);
    case CSSPropertyInsetBlockStart:
        return mapLogicalSideToPhysicalSide(makeTextFlow(style.writingMode(), style.direction()), LogicalBoxSide::BlockStart);
    case CSSPropertyInsetBlockEnd:
        return mapLogicalSideToPhysicalSide(makeTextFlow(style.writingMode(), style.direction()), LogicalBoxSide::BlockEnd);
    default:
        ASSERT_NOT_REACHED();
        return BoxSide::Top;
    }
}

static BoxSide flipBoxSide(BoxSide side)
{
    switch (side) {
    case BoxSide::Top:
        return BoxSide::Bottom;
    case BoxSide::Right:
        return BoxSide::Left;
    case BoxSide::Bottom:
        return BoxSide::Top;
    case BoxSide::Left:
        return BoxSide::Right;
    default:
        ASSERT_NOT_REACHED();
        return BoxSide::Top;
    }
}

// Physical sides (left/right/top/bottom) can only be used in certain inset properties. "For example,
// left is usable in left, right, or the logical inset properties that refer to the horizontal axis."
// See: https://drafts.csswg.org/css-anchor-position-1/#typedef-anchor-side
static bool anchorSideMatchesInsetProperty(CSSValueID anchorSideID, CSSPropertyID insetPropertyID, const RenderStyle& style)
{
    auto physicalAxis = mapInsetPropertyToPhysicalAxis(insetPropertyID, style);

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

// Anchor side resolution for keywords 'start', 'end', 'self-start', and 'self-end'.
// See: https://drafts.csswg.org/css-anchor-position-1/#funcdef-anchor
static BoxSide computeStartEndBoxSide(CSSPropertyID insetPropertyID, CheckedRef<const RenderElement> anchorPositionedRenderer, bool shouldComputeStart, bool shouldUseContainingBlockWritingMode)
{
    // 1. Compute the physical axis of inset property (using the element's writing mode)
    auto physicalAxis = mapInsetPropertyToPhysicalAxis(insetPropertyID, anchorPositionedRenderer->style());

    // 2. Convert the physical axis to the corresponding logical axis w.r.t. the element OR containing block's writing mode
    auto& textFlowStyle = shouldUseContainingBlockWritingMode ? anchorPositionedRenderer->containingBlock()->style() : anchorPositionedRenderer->style();
    auto textFlow = makeTextFlow(textFlowStyle.writingMode(), textFlowStyle.direction());
    auto logicalAxis = mapPhysicalAxisToLogicalAxis(textFlow, physicalAxis);

    // 3. Convert the logical start OR end side to the corresponding physical side w.r.t. the
    // element OR containing block's writing mode
    if (logicalAxis == LogicalBoxAxis::Inline) {
        if (shouldComputeStart)
            return mapLogicalSideToPhysicalSide(textFlow, LogicalBoxSide::InlineStart);
        return mapLogicalSideToPhysicalSide(textFlow, LogicalBoxSide::InlineEnd);
    }
    if (shouldComputeStart)
        return mapLogicalSideToPhysicalSide(textFlow, LogicalBoxSide::BlockStart);
    return mapLogicalSideToPhysicalSide(textFlow, LogicalBoxSide::BlockEnd);
}

// Insets for positioned elements are specified w.r.t. their containing blocks. Additionally, the containing block
// for a `position: absolute` element is defined by the padding box of its nearest absolutely positioned ancestor.
// Source: https://www.w3.org/TR/CSS2/visudet.html#containing-block-details.
// However, some of the logic in the codebase that deals with finding offsets from a containing block are done from
// the perspective of the container element's border box instead of its padding box. In those cases, we must remove
// the border widths from those locations for the final inset value.
static LayoutUnit removeBorderForInsetValue(LayoutUnit insetValue, BoxSide insetPropertySide, const RenderBlock& containingBlock)
{
    switch (insetPropertySide) {
    case BoxSide::Top:
        return insetValue - containingBlock.borderTop();
    case BoxSide::Right:
        return insetValue - containingBlock.borderRight();
    case BoxSide::Bottom:
        return insetValue - containingBlock.borderBottom();
    case BoxSide::Left:
        return insetValue - containingBlock.borderLeft();
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

static LayoutSize offsetFromAncestorContainer(const RenderElement& descendantContainer, const RenderElement& ancestorContainer)
{
    LayoutSize offset;
    LayoutPoint referencePoint;
    CheckedPtr currentContainer = &descendantContainer;
    do {
        CheckedPtr nextContainer = currentContainer->container();
        ASSERT(nextContainer); // This means we reached the top without finding container.
        if (!nextContainer)
            break;
        LayoutSize currentOffset = currentContainer->offsetFromContainer(*nextContainer, referencePoint);
        offset += currentOffset;
        referencePoint.move(currentOffset);
        currentContainer = WTFMove(nextContainer);
    } while (currentContainer != &ancestorContainer);

    return offset;
}

// This computes the top left location, physical width, and physical height of the specified
// anchor element. The location is computed relative to the specified containing block.
static LayoutRect computeAnchorRectRelativeToContainingBlock(CheckedRef<const RenderBoxModelObject> anchorBox, const RenderBlock& containingBlock)
{
    // Fragmented flows are a little tricky to deal with. One example of a fragmented
    // flow is a block anchor element that is "fragmented" or split across multiple columns
    // as a result of multi-column layout. In this case, we need to compute "the axis-aligned
    // bounding rectangle of the fragments' border boxes" and make that our anchorHeight/Width.
    // We also need to adjust the anchor's top left location to match that of the bounding box
    // instead of the first fragment.
    if (CheckedPtr fragmentedFlow = anchorBox->enclosingFragmentedFlow()) {
        // Compute the bounding box of the fragments.
        // Location is relative to the fragmented flow.
        CheckedPtr anchorRenderBox = dynamicDowncast<RenderBox>(&anchorBox.get());
        if (!anchorRenderBox)
            anchorRenderBox = anchorBox->containingBlock();
        LayoutPoint offsetRelativeToFragmentedFlow = fragmentedFlow->mapFromLocalToFragmentedFlow(anchorRenderBox.get(), { }).location();
        auto unfragmentedBorderBox = anchorBox->borderBoundingBox();
        unfragmentedBorderBox.moveBy(offsetRelativeToFragmentedFlow);
        auto fragmentsBoundingBox = fragmentedFlow->fragmentsBoundingBox(unfragmentedBorderBox);

        // Change the location to be relative to the anchor's containing block.
        if (fragmentedFlow->isDescendantOf(&containingBlock))
            fragmentsBoundingBox.move(offsetFromAncestorContainer(*fragmentedFlow, containingBlock));
        else
            fragmentsBoundingBox.move(-offsetFromAncestorContainer(containingBlock, *fragmentedFlow));

        // FIXME: The final location of the fragments bounding box is not correctly
        // computed in flipped writing modes (i.e. vertical-rl and horizontal-bt).
        return fragmentsBoundingBox;
    }

    auto anchorWidth = anchorBox->offsetWidth();
    auto anchorHeight = anchorBox->offsetHeight();
    auto anchorLocation = LayoutPoint { offsetFromAncestorContainer(anchorBox, containingBlock) };
    if (CheckedPtr anchorRenderInline = dynamicDowncast<RenderInline>(&anchorBox.get())) {
        // RenderInline objects do not automatically account for their offset in offsetFromAncestorContainer,
        // so we incorporate this offset here.
        anchorLocation.moveBy(anchorRenderInline->linesBoundingBox().location());
    }

    return LayoutRect(anchorLocation, LayoutSize(anchorWidth, anchorHeight));
}

// "An anchor() function representing a valid anchor function resolves...to the <length> that would
// align the edge of the positioned elements' inset-modified containing block corresponding to the
// property the function appears in with the specified border edge of the target anchor element..."
// See: https://drafts.csswg.org/css-anchor-position-1/#anchor-pos
static Length computeInsetValue(CSSPropertyID insetPropertyID, CheckedRef<const RenderBoxModelObject> anchorBox, CheckedRef<const RenderElement> anchorPositionedRenderer, const CSSAnchorValue& anchorValue)
{
    CheckedPtr containingBlock = anchorPositionedRenderer->containingBlock();
    ASSERT(containingBlock);

    auto insetPropertySide = mapInsetPropertyToPhysicalSide(insetPropertyID, anchorPositionedRenderer->style());
    auto anchorSideID = anchorValue.anchorSide()->valueID();
    auto anchorRect = computeAnchorRectRelativeToContainingBlock(anchorBox, *containingBlock);

    // Explicitly deal with the center/percentage value here.
    // "Refers to a position a corresponding percentage between the start and end sides, with
    // 0% being equivalent to start and 100% being equivalent to end. center is equivalent to 50%."
    if (anchorSideID == CSSValueCenter || anchorSideID == CSSValueInvalid) {
        double percentage = 0.5;
        if (anchorSideID != CSSValueCenter)
            percentage = dynamicDowncast<CSSPrimitiveValue>(anchorValue.anchorSide())->doubleValueDividingBy100IfPercentage();

        // We assume that the "start" side always is either the top or left side of the anchor element.
        // However, if that is not the case, we should take the complement of the percentage.
        auto startSide = computeStartEndBoxSide(insetPropertyID, anchorPositionedRenderer, true, true);
        if (startSide == BoxSide::Bottom || startSide == BoxSide::Right)
            percentage = 1 - percentage;

        LayoutUnit insetValue;
        auto insetPropertyAxis = mapInsetPropertyToPhysicalAxis(insetPropertyID, anchorPositionedRenderer->style());
        if (insetPropertyAxis == BoxAxis::Vertical) {
            insetValue = anchorRect.location().y() + anchorRect.height() * percentage;
            if (insetPropertySide == BoxSide::Bottom)
                insetValue = containingBlock->height() - insetValue;
        } else {
            insetValue = anchorRect.location().x() + anchorRect.width() * percentage;
            if (insetPropertySide == BoxSide::Right)
                insetValue = containingBlock->width() - insetValue;
        }
        insetValue = removeBorderForInsetValue(insetValue, insetPropertySide, *containingBlock);
        return Length(insetValue, LengthType::Fixed);
    }

    // Normalize the anchor side to a physical side
    BoxSide anchorSide;
    switch (anchorSideID) {
    case CSSValueID::CSSValueTop:
        anchorSide = BoxSide::Top;
        break;
    case CSSValueID::CSSValueBottom:
        anchorSide = BoxSide::Bottom;
        break;
    case CSSValueID::CSSValueLeft:
        anchorSide = BoxSide::Left;
        break;
    case CSSValueID::CSSValueRight:
        anchorSide = BoxSide::Right;
        break;
    case CSSValueID::CSSValueInside:
        anchorSide = insetPropertySide;
        break;
    case CSSValueID::CSSValueOutside:
        anchorSide = flipBoxSide(insetPropertySide);
        break;
    case CSSValueID::CSSValueStart:
        anchorSide = computeStartEndBoxSide(insetPropertyID, anchorPositionedRenderer, true, true);
        break;
    case CSSValueID::CSSValueEnd:
        anchorSide = computeStartEndBoxSide(insetPropertyID, anchorPositionedRenderer, false, true);
        break;
    case CSSValueID::CSSValueSelfStart:
        anchorSide = computeStartEndBoxSide(insetPropertyID, anchorPositionedRenderer, true, false);
        break;
    case CSSValueID::CSSValueSelfEnd:
        anchorSide = computeStartEndBoxSide(insetPropertyID, anchorPositionedRenderer, false, false);
        break;
    default:
        ASSERT_NOT_REACHED();
        anchorSide = BoxSide::Top;
        break;
    }

    // Compute inset from the containing block
    LayoutUnit insetValue;
    switch (anchorSide) {
    case BoxSide::Top:
        insetValue = anchorRect.location().y();
        if (insetPropertySide == BoxSide::Bottom)
            insetValue = containingBlock->height() - insetValue;
        break;
    case BoxSide::Bottom:
        insetValue = anchorRect.location().y() + anchorRect.height();
        if (insetPropertySide == BoxSide::Bottom)
            insetValue = containingBlock->height() - insetValue;
        break;
    case BoxSide::Left:
        insetValue = anchorRect.location().x();
        if (insetPropertySide == BoxSide::Right)
            insetValue = containingBlock->width() - insetValue;
        break;
    case BoxSide::Right:
        insetValue = anchorRect.location().x() + anchorRect.width();
        if (insetPropertySide == BoxSide::Right)
            insetValue = containingBlock->width() - insetValue;
        break;
    }
    insetValue = removeBorderForInsetValue(insetValue, insetPropertySide, *containingBlock);
    return Length(insetValue, LengthType::Fixed);
}

Length AnchorPositionEvaluator::resolveAnchorValue(const BuilderState& builderState, const CSSAnchorValue& anchorValue)
{
    // FIXME: Determine when this if guard is true and what it means.
    if (!builderState.element())
        return Length(0, LengthType::Fixed);
    Ref anchorPositionedElement = *builderState.element();

    // FIXME: Support pseudo-elements.
    if (builderState.style().pseudoElementType() != PseudoId::None)
        return Length(0, LengthType::Fixed);

    // FIXME: Support animations and transitions.
    if (builderState.style().hasAnimationsOrTransitions())
        return Length(0, LengthType::Fixed);

    // In-flow elements cannot be anchor-positioned.
    // FIXME: Should attempt to resolve the fallback value.
    if (!builderState.style().hasOutOfFlowPosition())
        return Length(0, LengthType::Fixed);

    auto& anchorPositionedStates = anchorPositionedElement->document().styleScope().anchorPositionedStates();
    auto& anchorPositionedState = *anchorPositionedStates.ensure(anchorPositionedElement, [&] {
        return WTF::makeUnique<AnchorPositionedState>();
    }).iterator->value.get();

    // If we are encountering this anchor() instance for the first time, then we need to collect
    // all the relevant anchor-name strings that are referenced in this anchor function,
    // including the references in the fallback value.
    if (anchorPositionedState.stage < AnchorPositionResolutionStage::FinishedCollectingAnchorNames)
        anchorValue.collectAnchorNames(anchorPositionedState.anchorNames);

    // An anchor() instance will be ready to be resolved when all referenced anchor-names
    // have been mapped to an actual anchor element in the DOM tree. At that point, we
    // should also have layout information for the anchor-positioned element alongside
    // the anchors referenced by the anchor-positioned element. Until then, we cannot
    // resolve this anchor() instance.
    if (anchorPositionedState.stage < AnchorPositionResolutionStage::FoundAnchors)
        return Length(0, LengthType::Fixed);

    // Anchor value may now be resolved using layout information
    anchorPositionedState.stage = AnchorPositionResolutionStage::Resolved;
    CheckedPtr anchorPositionedRenderer = anchorPositionedElement->renderer();
    ASSERT(anchorPositionedRenderer);

    // Attempt to find the element associated with the target anchor
    String anchorString = anchorValue.anchorElementString();
    if (anchorString.isNull())
        anchorString = anchorPositionedRenderer->style().positionAnchor();
    auto anchorElementIt = anchorPositionedState.anchorElements.find(anchorString);
    if (anchorElementIt == anchorPositionedState.anchorElements.end()) {
        // FIXME: Should rely on fallback, and also should behave as unset if fallback doesn't exist.
        // See: https://drafts.csswg.org/css-anchor-position-1/#valid-anchor-function
        return Length(0, LengthType::Fixed);
    }
    CheckedPtr anchorRenderer = anchorElementIt->value->renderer();
    ASSERT(anchorRenderer);

    // Confirm that the axis specified by the inset property matches the side provided in
    // the anchor() call.
    auto insetPropertyID = builderState.cssPropertyID();
    auto& anchorPositionedElementStyle = anchorPositionedElement->renderer()->style();
    if (!anchorSideMatchesInsetProperty(anchorValue.anchorSide()->valueID(), insetPropertyID, anchorPositionedElementStyle)) {
        // FIXME: Should rely on fallback, and also should behave as unset if fallback doesn't exist.
        // See: https://drafts.csswg.org/css-anchor-position-1/#valid-anchor-function
        return Length(0, LengthType::Fixed);
    }

    // Proceed with computing the inset value for the specified inset property.
    CheckedRef anchorBox = downcast<RenderBoxModelObject>(*anchorRenderer);
    return computeInsetValue(insetPropertyID, anchorBox, *anchorPositionedRenderer, anchorValue);
}

static const RenderElement* penultimateContainingBlockChainElement(const RenderElement* descendant, const RenderElement* ancestor)
{
    auto* currentElement = descendant;
    for (auto* nextElement = currentElement->containingBlock(); nextElement; nextElement = nextElement->containingBlock()) {
        if (nextElement == ancestor)
            return currentElement;
        currentElement = nextElement;
    }
    return nullptr;
}

static bool firstChildPrecedesSecondChild(const RenderObject* firstChild, const RenderObject* secondChild, const RenderBlock* containingBlock)
{
    auto positionedObjects = containingBlock->positionedObjects();
    ASSERT(positionedObjects);
    for (auto it = positionedObjects->begin(); it != positionedObjects->end(); ++it) {
        auto child = it.get();
        if (child == firstChild)
            return true;
        if (child == secondChild)
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

// See: https://drafts.csswg.org/css-anchor-position-1/#acceptable-anchor-element
static bool isAcceptableAnchorElement(Ref<const Element> anchorElement, Ref<const Element> anchorPositionedElement)
{
    CheckedPtr anchorRenderer = anchorElement->renderer();
    CheckedPtr anchorPositionedRenderer = anchorPositionedElement->renderer();
    ASSERT(anchorRenderer && anchorPositionedRenderer);
    CheckedPtr containingBlock = anchorPositionedRenderer->containingBlock();
    ASSERT(containingBlock);

    auto* penultimateElement = penultimateContainingBlockChainElement(anchorRenderer.get(), containingBlock.get());
    if (!penultimateElement)
        return false;

    if (!penultimateElement->isOutOfFlowPositioned())
        return true;

    if (!firstChildPrecedesSecondChild(penultimateElement, anchorPositionedRenderer.get(), containingBlock.get()))
        return false;

    // FIXME: Implement the rest of https://drafts.csswg.org/css-anchor-position-1/#acceptable-anchor-element.
    return true;
}


static std::optional<Ref<Element>> findLastAcceptableAnchorWithName(String anchorName, Ref<const Element> anchorPositionedElement)
{
    const auto& anchors = anchorPositionedElement->document().styleScope().anchorsForAnchorName().get(anchorName);

    // FIXME: These should iterate through the anchor targets in reverse DOM order.
    for (auto anchor : makeReversedRange(anchors)) {
        ASSERT(anchor->renderer());
        if (isAcceptableAnchorElement(anchor.get(), anchorPositionedElement))
            return anchor.get();
    }

    return { };
}

void AnchorPositionEvaluator::findAnchorsForAnchorPositionedElement(Ref<const Element> anchorPositionedElement)
{
    auto* anchorPositionedState = anchorPositionedElement->document().styleScope().anchorPositionedStates().get(anchorPositionedElement);
    ASSERT(anchorPositionedState && anchorPositionedState->stage == AnchorPositionResolutionStage::FinishedCollectingAnchorNames);

    for (auto& anchorName : anchorPositionedState->anchorNames) {
        auto anchor = findLastAcceptableAnchorWithName(anchorName, anchorPositionedElement);
        if (anchor.has_value())
            anchorPositionedState->anchorElements.add(anchorName, anchor->get());
    }

    anchorPositionedState->stage = AnchorPositionResolutionStage::FoundAnchors;
}

} // namespace WebCore::Style
