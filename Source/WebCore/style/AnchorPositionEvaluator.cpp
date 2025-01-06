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
#include "RenderLayer.h"
#include "RenderStyle.h"
#include "RenderStyleInlines.h"
#include "RenderView.h"
#include "StyleBuilderConverter.h"
#include "StyleBuilderState.h"
#include "StyleScope.h"
#include "WritingMode.h"

namespace WebCore::Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AnchorPositionedState);

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

static BoxSide mapInsetPropertyToPhysicalSide(CSSPropertyID id, const WritingMode writingMode)
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
        return mapSideLogicalToPhysical(writingMode, LogicalBoxSide::InlineStart);
    case CSSPropertyInsetInlineEnd:
        return mapSideLogicalToPhysical(writingMode, LogicalBoxSide::InlineEnd);
    case CSSPropertyInsetBlockStart:
        return mapSideLogicalToPhysical(writingMode, LogicalBoxSide::BlockStart);
    case CSSPropertyInsetBlockEnd:
        return mapSideLogicalToPhysical(writingMode, LogicalBoxSide::BlockEnd);
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
static bool anchorSideMatchesInsetProperty(CSSValueID anchorSideID, CSSPropertyID insetPropertyID, const WritingMode writingMode)
{
    auto physicalAxis = mapInsetPropertyToPhysicalAxis(insetPropertyID, writingMode);

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
    auto physicalAxis = mapInsetPropertyToPhysicalAxis(insetPropertyID, anchorPositionedRenderer->writingMode());

    // 2. Convert the physical axis to the corresponding logical axis w.r.t. the element OR containing block's writing mode
    auto& style = shouldUseContainingBlockWritingMode ? anchorPositionedRenderer->containingBlock()->style() : anchorPositionedRenderer->style();
    auto writingMode = style.writingMode();
    auto logicalAxis = mapAxisPhysicalToLogical(writingMode, physicalAxis);

    // 3. Convert the logical start OR end side to the corresponding physical side w.r.t. the
    // element OR containing block's writing mode
    if (logicalAxis == LogicalBoxAxis::Inline) {
        if (shouldComputeStart)
            return mapSideLogicalToPhysical(writingMode, LogicalBoxSide::InlineStart);
        return mapSideLogicalToPhysical(writingMode, LogicalBoxSide::InlineEnd);
    }
    if (shouldComputeStart)
        return mapSideLogicalToPhysical(writingMode, LogicalBoxSide::BlockStart);
    return mapSideLogicalToPhysical(writingMode, LogicalBoxSide::BlockEnd);
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

        // https://drafts.csswg.org/css-anchor-position-1/#scroll
        // "anchor() is defined to assume all the scroll containers between the anchor element and
        // the positioned element’s containing block are at their initial scroll position,"
        if (CheckedPtr boxContainer = dynamicDowncast<RenderBox>(*nextContainer))
            offset += toLayoutSize(boxContainer->scrollPosition());

        offset += currentOffset;
        referencePoint.move(currentOffset);
        currentContainer = WTFMove(nextContainer);
    } while (currentContainer != &ancestorContainer);

    return offset;
}

static LayoutSize scrollOffsetFromAncestorContainer(const RenderElement& descendant, const RenderElement& ancestorContainer)
{
    ASSERT(descendant.isDescendantOf(&ancestorContainer));

    auto offset = LayoutSize { };
    for (auto* ancestor = descendant.container(); ancestor; ancestor = ancestor->container()) {
        if (auto* box = dynamicDowncast<RenderBox>(ancestor))
            offset -= toLayoutSize(box->scrollPosition());
        if (ancestor == &ancestorContainer)
            break;
    }
    return offset;
}

// This computes the top left location, physical width, and physical height of the specified
// anchor element. The location is computed relative to the specified containing block.
LayoutRect AnchorPositionEvaluator::computeAnchorRectRelativeToContainingBlock(CheckedRef<const RenderBoxModelObject> anchorBox, const RenderBlock& containingBlock)
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
static LayoutUnit computeInsetValue(CSSPropertyID insetPropertyID, CheckedRef<const RenderBoxModelObject> anchorBox, CheckedRef<const RenderElement> anchorPositionedRenderer, AnchorPositionEvaluator::Side anchorSide)
{
    CheckedPtr containingBlock = anchorPositionedRenderer->containingBlock();
    ASSERT(containingBlock);

    auto insetPropertySide = mapInsetPropertyToPhysicalSide(insetPropertyID, anchorPositionedRenderer->writingMode());
    auto anchorSideID = std::holds_alternative<CSSValueID>(anchorSide) ? std::get<CSSValueID>(anchorSide) : CSSValueInvalid;
    auto anchorRect = AnchorPositionEvaluator::computeAnchorRectRelativeToContainingBlock(anchorBox, *containingBlock);

    // Explicitly deal with the center/percentage value here.
    // "Refers to a position a corresponding percentage between the start and end sides, with
    // 0% being equivalent to start and 100% being equivalent to end. center is equivalent to 50%."
    if (anchorSideID == CSSValueCenter || anchorSideID == CSSValueInvalid) {
        double percentage = anchorSideID == CSSValueCenter ? 0.5 : std::get<double>(anchorSide);

        // We assume that the "start" side always is either the top or left side of the anchor element.
        // However, if that is not the case, we should take the complement of the percentage.
        auto startSide = computeStartEndBoxSide(insetPropertyID, anchorPositionedRenderer, true, true);
        if (startSide == BoxSide::Bottom || startSide == BoxSide::Right)
            percentage = 1 - percentage;

        LayoutUnit insetValue;
        auto insetPropertyAxis = mapInsetPropertyToPhysicalAxis(insetPropertyID, anchorPositionedRenderer->writingMode());
        if (insetPropertyAxis == BoxAxis::Vertical) {
            insetValue = anchorRect.location().y() + anchorRect.height() * percentage;
            if (insetPropertySide == BoxSide::Bottom)
                insetValue = containingBlock->height() - insetValue;
        } else {
            insetValue = anchorRect.location().x() + anchorRect.width() * percentage;
            if (insetPropertySide == BoxSide::Right)
                insetValue = containingBlock->width() - insetValue;
        }
        return removeBorderForInsetValue(insetValue, insetPropertySide, *containingBlock);
    }

    // Normalize the anchor side to a physical side
    BoxSide boxSide;
    switch (anchorSideID) {
    case CSSValueID::CSSValueTop:
        boxSide = BoxSide::Top;
        break;
    case CSSValueID::CSSValueBottom:
        boxSide = BoxSide::Bottom;
        break;
    case CSSValueID::CSSValueLeft:
        boxSide = BoxSide::Left;
        break;
    case CSSValueID::CSSValueRight:
        boxSide = BoxSide::Right;
        break;
    case CSSValueID::CSSValueInside:
        boxSide = insetPropertySide;
        break;
    case CSSValueID::CSSValueOutside:
        boxSide = flipBoxSide(insetPropertySide);
        break;
    case CSSValueID::CSSValueStart:
        boxSide = computeStartEndBoxSide(insetPropertyID, anchorPositionedRenderer, true, true);
        break;
    case CSSValueID::CSSValueEnd:
        boxSide = computeStartEndBoxSide(insetPropertyID, anchorPositionedRenderer, false, true);
        break;
    case CSSValueID::CSSValueSelfStart:
        boxSide = computeStartEndBoxSide(insetPropertyID, anchorPositionedRenderer, true, false);
        break;
    case CSSValueID::CSSValueSelfEnd:
        boxSide = computeStartEndBoxSide(insetPropertyID, anchorPositionedRenderer, false, false);
        break;
    default:
        ASSERT_NOT_REACHED();
        boxSide = BoxSide::Top;
        break;
    }

    // Compute inset from the containing block
    LayoutUnit insetValue;
    switch (boxSide) {
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
    return removeBorderForInsetValue(insetValue, insetPropertySide, *containingBlock);
}

RefPtr<Element> AnchorPositionEvaluator::findAnchorAndAttemptResolution(const BuilderState& builderState, std::optional<ScopedName> elementName)
{
    const auto& style = builderState.style();

    auto isValid = [&] {
        if (!builderState.element())
            return false;

        // FIXME: Support pseudo-elements.
        if (style.pseudoElementType() != PseudoId::None)
            return false;

        return true;
    };

    if (!isValid())
        return { };

    Ref anchorPositionedElement = *builderState.element();

    auto& anchorPositionedStates = anchorPositionedElement->document().styleScope().anchorPositionedStates();
    auto& anchorPositionedState = *anchorPositionedStates.ensure(anchorPositionedElement, [&] {
        return WTF::makeUnique<AnchorPositionedState>();
    }).iterator->value.get();

    if (!elementName)
        elementName = builderState.style().positionAnchor();

    if (elementName) {
        // Collect anchor names that this element refers to in anchor() or anchor-size()
        bool isNewAnchorName = anchorPositionedState.anchorNames.add(elementName->name).isNewEntry;

        // If anchor resolution has progressed past Initial, and we pick up a new anchor name, set the
        // stage back to Initial. This restarts the resolution process to resolve newly added names.
        if (isNewAnchorName)
            anchorPositionedState.stage = AnchorPositionResolutionStage::Initial;
    }

    // An anchor() instance will be ready to be resolved when all referenced anchor-names
    // have been mapped to an actual anchor element in the DOM tree. At that point, we
    // should also have layout information for the anchor-positioned element alongside
    // the anchors referenced by the anchor-positioned element. Until then, we cannot
    // resolve this anchor() instance.
    if (anchorPositionedState.stage == AnchorPositionResolutionStage::Initial)
        return { };

    CheckedPtr anchorPositionedRenderer = anchorPositionedElement->renderer();
    if (!anchorPositionedRenderer) {
        // If no render tree information is present, the procedure is finished.
        anchorPositionedState.stage = AnchorPositionResolutionStage::Resolved;
        return { };
    }

    // Anchor value may now be resolved using layout information

    RefPtr anchorElement = elementName ? anchorPositionedState.anchorElements.get(elementName->name) : nullptr;
    if (!anchorElement) {
        // See: https://drafts.csswg.org/css-anchor-position-1/#valid-anchor-function
        anchorPositionedState.stage = AnchorPositionResolutionStage::Resolved;

        return { };
    }

    if (auto* state = anchorPositionedStates.get(*anchorElement)) {
        // Check if the anchor is itself anchor-positioned but hasn't been positioned yet.
        if (state->stage != AnchorPositionResolutionStage::Positioned)
            return { };
    }

    anchorPositionedState.stage = AnchorPositionResolutionStage::Resolved;

    return anchorElement;
}

std::optional<double> AnchorPositionEvaluator::evaluate(const BuilderState& builderState, std::optional<ScopedName> elementName, Side side)
{
    auto propertyID = builderState.cssPropertyID();
    const auto& style = builderState.style();

    // https://drafts.csswg.org/css-anchor-position-1/#anchor-valid
    auto isValidAnchor = [&] {
        // It’s being used in an inset property...
        if (!CSSProperty::isInsetProperty(propertyID))
            return false;

        // ...on an absolutely-positioned element.
        if (!style.hasOutOfFlowPosition())
            return false;

        // If its <anchor-side> specifies a physical keyword, it’s being used in an inset property in that axis.
        // (For example, left can only be used in left, right, or a logical inset property in the horizontal axis.)
        if (auto* sideID = std::get_if<CSSValueID>(&side); sideID && !anchorSideMatchesInsetProperty(*sideID, propertyID, style.writingMode()))
            return false;

        return true;
    };

    if (!isValidAnchor())
        return { };

    auto anchorElement = findAnchorAndAttemptResolution(builderState, elementName);
    if (!anchorElement)
        return { };

    CheckedPtr anchorRenderer = anchorElement->renderer();
    ASSERT(anchorRenderer);

    CheckedPtr anchorPositionedElement = builderState.element();
    ASSERT(anchorPositionedElement);
    CheckedPtr anchorPositionedRenderer = anchorPositionedElement->renderer();
    ASSERT(anchorPositionedRenderer);

    // Proceed with computing the inset value for the specified inset property.
    CheckedRef anchorBox = downcast<RenderBoxModelObject>(*anchorRenderer);
    return computeInsetValue(propertyID, anchorBox, *anchorPositionedRenderer, side);
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

std::optional<double> AnchorPositionEvaluator::evaluateSize(const BuilderState& builderState, std::optional<ScopedName> elementName, std::optional<AnchorSizeDimension> dimension)
{
    auto propertyID = builderState.cssPropertyID();
    const auto& style = builderState.style();

    auto isValidAnchorSize = [&] {
        // It’s being used in a sizing property, an inset property, or a margin property...
        if (!CSSProperty::isSizingProperty(propertyID) && !CSSProperty::isInsetProperty(propertyID) && !CSSProperty::isMarginProperty(propertyID))
            return false;

        // ...on an absolutely-positioned element.
        if (!style.hasOutOfFlowPosition())
            return false;

        return true;
    };

    if (!isValidAnchorSize())
        return { };

    auto anchorElement = findAnchorAndAttemptResolution(builderState, elementName);
    if (!anchorElement)
        return { };

    // Resolve the dimension (width or height) to return from the anchor positioned element.
    CheckedPtr anchorPositionedElement = builderState.element();
    ASSERT(anchorPositionedElement);
    CheckedPtr anchorPositionedRenderer = anchorPositionedElement->renderer();
    ASSERT(anchorPositionedRenderer);

    CheckedPtr anchorPositionedContainerRenderer = anchorPositionedRenderer->container();
    ASSERT(anchorPositionedContainerRenderer);

    auto resolvedDimension = dimension.value_or(defaultDimensionForPropertyID(propertyID));
    auto physicalDimension = anchorSizeDimensionToPhysicalDimension(resolvedDimension, anchorPositionedRenderer->style(), anchorPositionedContainerRenderer->style());

    // Return the dimension information from the anchor element.
    CheckedPtr anchorRenderer = anchorElement->renderer();
    ASSERT(anchorRenderer);

    CheckedRef anchorBox = downcast<RenderBoxModelObject>(*anchorRenderer);
    auto anchorBorderBoundingBox = anchorBox->borderBoundingBox();

    switch (physicalDimension) {
    case BoxAxis::Horizontal:
        return anchorBorderBoundingBox.width();
    case BoxAxis::Vertical:
        return anchorBorderBoundingBox.height();
    }

    ASSERT_NOT_REACHED();
    return { };
}

static const RenderElement* penultimateContainingBlockChainElement(const RenderElement& descendant, const RenderElement* ancestor)
{
    auto* currentElement = &descendant;
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
static bool isAcceptableAnchorElement(const RenderBoxModelObject& anchorRenderer, Ref<const Element> anchorPositionedElement)
{
    CheckedPtr anchorPositionedRenderer = anchorPositionedElement->renderer();
    ASSERT(anchorPositionedRenderer);
    CheckedPtr containingBlock = anchorPositionedRenderer->containingBlock();
    ASSERT(containingBlock);

    auto* penultimateElement = penultimateContainingBlockChainElement(anchorRenderer, containingBlock.get());
    if (!penultimateElement)
        return false;

    if (!penultimateElement->isOutOfFlowPositioned())
        return true;

    if (!firstChildPrecedesSecondChild(penultimateElement, anchorPositionedRenderer.get(), containingBlock.get()))
        return false;

    // "Possible anchor is either an element or a fully styleable tree-abiding pseudo-element."
    // This always have an associated Element (for ::before/::after it is PseudoElement).
    if (!anchorRenderer.element())
        return false;

    // FIXME: Implement the rest of https://drafts.csswg.org/css-anchor-position-1/#acceptable-anchor-element.
    return true;
}


static std::optional<Ref<Element>> findLastAcceptableAnchorWithName(AtomString anchorName, Ref<const Element> anchorPositionedElement, const AnchorsForAnchorName& anchorsForAnchorName)
{
    const auto& anchors = anchorsForAnchorName.get(anchorName);

    for (auto& anchor : makeReversedRange(anchors)) {
        if (isAcceptableAnchorElement(anchor.get(), anchorPositionedElement))
            return *anchor->element();
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
        for (auto& scopedName : anchorRenderer.style().anchorNames()) {
            anchorsForAnchorName.ensure(scopedName.name, [&] {
                return AnchorsForAnchorName::MappedType { };
            }).iterator->value.append(anchorRenderer);
        }
    }

    // Sort them in tree order.
    for (auto& anchors : anchorsForAnchorName.values()) {
        std::sort(anchors.begin(), anchors.end(), [](auto& a, auto& b) {
            // FIXME: Figure out anonymous pseudo-elements.
            if (!a->element() || !b->element())
                return !!b->element();
            return is_lt(treeOrder<ComposedTree>(*a->element(), *b->element()));
        });
    }

    return anchorsForAnchorName;
}

AnchorElements AnchorPositionEvaluator::findAnchorsForAnchorPositionedElement(const Element& anchorPositionedElement, const HashSet<AtomString>& anchorNames, const AnchorsForAnchorName& anchorsForAnchorName)
{
    AnchorElements anchorElements;

    for (auto& anchorName : anchorNames) {
        auto anchor = findLastAcceptableAnchorWithName(anchorName, anchorPositionedElement, anchorsForAnchorName);
        if (anchor)
            anchorElements.add(anchorName, anchor->get());
    }

    return anchorElements;
}

void AnchorPositionEvaluator::updateAnchorPositioningStatesAfterInterleavedLayout(const Document& document)
{
    if (document.styleScope().anchorPositionedStates().isEmptyIgnoringNullReferences())
        return;

    auto anchorsForAnchorName = collectAnchorsForAnchorName(document);

    for (auto elementAndState : document.styleScope().anchorPositionedStates()) {
        auto& state = *elementAndState.value;
        if (state.stage == AnchorPositionResolutionStage::Initial) {
            Ref element { elementAndState.key };
            if (element->renderer())
                state.anchorElements = findAnchorsForAnchorPositionedElement(element, state.anchorNames, anchorsForAnchorName);
            state.stage = AnchorPositionResolutionStage::FoundAnchors;
            continue;
        }
        if (state.stage == AnchorPositionResolutionStage::Resolved)
            state.stage = AnchorPositionResolutionStage::Positioned;
    }
}

void AnchorPositionEvaluator::updateSnapshottedScrollOffsets(Document& document)
{
    // https://drafts.csswg.org/css-anchor-position-1/#scroll

    auto& states = document.styleScope().anchorPositionedStates();
    for (auto elementAndState : states) {
        CheckedRef anchorPositionedElement = elementAndState.key;
        if (!anchorPositionedElement->renderer())
            continue;

        CheckedPtr anchorPositionedRenderer = dynamicDowncast<RenderBox>(anchorPositionedElement->renderer());
        if (!anchorPositionedRenderer || !anchorPositionedRenderer->layer())
            continue;

        auto needsScrollAdjustment = [&] {
            // FIXME: This is incomplete.
            if (!anchorPositionedRenderer->style().positionAnchor())
                return false;

            if (elementAndState.value->anchorElements.size() != 1)
                return false;

            return true;
        }();

        if (!needsScrollAdjustment)
            continue;

        auto anchorElement = *elementAndState.value->anchorElements.values().begin();
        if (!anchorElement->renderer())
            continue;

        CheckedPtr containingBlock = anchorPositionedRenderer->containingBlock();

        auto scrollOffset = scrollOffsetFromAncestorContainer(*anchorElement->renderer(), *containingBlock);

        if (scrollOffset.isZero() && !anchorPositionedRenderer->layer()->snapshottedScrollOffsetForAnchorPositioning())
            continue;

        anchorPositionedRenderer->layer()->setSnapshottedScrollOffsetForAnchorPositioning(scrollOffset);
    }
}

void AnchorPositionEvaluator::cleanupAnchorPositionedState(Element& element)
{
    if (element.document().styleScope().anchorPositionedStates().remove(element)) {
        if (auto* renderer = dynamicDowncast<RenderBox>(element.renderer()); renderer && renderer->layer())
            renderer->layer()->clearSnapshottedScrollOffsetForAnchorPositioning();
    }
}

WTF::TextStream& operator<<(WTF::TextStream& ts, PositionTryOrder order)
{
    switch (order) {
    case PositionTryOrder::Normal: ts << "normal"; break;
    case PositionTryOrder::MostWidth: ts << "most-width"; break;
    case PositionTryOrder::MostHeight: ts << "most-height"; break;
    case PositionTryOrder::MostBlockSize: ts << "most-block-size"; break;
    case PositionTryOrder::MostInlineSize: ts << "most-inline-size"; break;
    }

    return ts;
}

} // namespace WebCore::Style
