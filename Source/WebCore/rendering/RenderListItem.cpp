/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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
#include "RenderListItem.h"

#include "BaselineAlignment.h"
#include "CSSCounterStyleDescriptors.h"
#include "CSSCounterStyleRegistry.h"
#include "ContainerNodeInlines.h"
#include "CSSFontSelector.h"
#include "DocumentInlines.h"
#include "ElementInlines.h"
#include "ElementTraversal.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HTMLUListElement.h"
#include "LocalFrameView.h"
#include "LocalFrameViewLayoutContext.h"
#include "PseudoElement.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderChildIterator.h"
#include "RenderDescendantIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderImage.h"
#include "RenderMenuList.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderObjectInlines.h"
#include "RenderTable.h"
#include "RenderText.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleContent.h"
#include "StyleListStyleImageSizing.h"
#include "StyleListStyleType.h"
#include "TextUtil.h"
#include "UnicodeBidi.h"
#include <wtf/ScopedLambda.h>
#include <wtf/StackStats.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderListItem);

enum class MarkerSearchBoxType : uint8_t {
    BlockContainer,
    SpannerPlaceholder,
    InlineContent,
    Opaque,
    NestedListInQuirksMode,
    TableRubyOrReplaced,
};
NODELETE static MarkerSearchBoxType markerSearchBoxType(const RenderObject& box)
{
    if (box.isFloating() || box.isOutOfFlowPositioned())
        return MarkerSearchBoxType::Opaque;
    if (is<RenderMultiColumnSpannerPlaceholder>(box))
        return MarkerSearchBoxType::SpannerPlaceholder;
    // A nested list item's own marker is inline level but takes no part in any line, so it must not make its list item
    // the answer: that list item may well have nothing but block level children and never run inline layout.
    if (box.isOutsideListMarker())
        return MarkerSearchBoxType::Opaque;
    // Before the form control check below: a control on a line is inline content like any other.
    if (box.isInline())
        return MarkerSearchBoxType::InlineContent;
    if (is<RenderMenuList>(box))
        return MarkerSearchBoxType::Opaque;
    if (auto* renderBox = dynamicDowncast<RenderBox>(box); renderBox && renderBox->isWritingModeRoot())
        return MarkerSearchBoxType::Opaque;
    if (is<RenderListItem>(box.parent()) && box.document().inQuirksMode() && box.node() && isHTMLListElement(*box.node()))
        return MarkerSearchBoxType::NestedListInQuirksMode;
    if (!is<RenderBlock>(box) || is<RenderTable>(box) || box.style().display() == Style::DisplayType::BlockRuby)
        return MarkerSearchBoxType::TableRubyOrReplaced;
    return MarkerSearchBoxType::BlockContainer;
}

static LayoutUnit outsideMarkerLogicalLeftOffsetFor(const RenderBlockFlow& firstFormattedLineRoot, const RenderListItem& listItem, bool isLineStartConstrainedByFloat)
{
    // <ul><li id=o><ul><li id=i><div style="border-left: 5px solid">text</div></li></ul></li></ul>
    // UA sets 40px start padding on <ul>
    // x=0        40         80     85
    // |          |          |      |
    // |[o marker]|[i marker]|      text
    // |          |          |      |
    // +----------+----------+------+-------------
    //                       |<-----|  toEnclosingListItem = -5
    //            |<---------|         toAssociatedListItem = -40
    //            |<----------------|  return value = -45
    // Both markers on the same line and these values are for the outer marker. The second offset is zero unless the line is inside a nested list item.
    // The marker is on the inline start side of its list item, which is not that of a box running the other way, so
    // the boxes in between are measured on the list item's side.
    auto isLeftToRight = listItem.writingMode().isLogicalLeftInlineStart();
    auto borderAndPaddingOnMarkerSide = [&](const RenderBoxModelObject& box) {
        return isLeftToRight ? box.borderAndPaddingLogicalLeft() : box.borderAndPaddingLogicalRight();
    };
    auto toEnclosingListItem = LayoutUnit { };
    auto hasAccountedForBorderAndPadding = false;
    CheckedPtr<const RenderBlock> ancestor = &firstFormattedLineRoot;
    for (; ancestor; ancestor = ancestor->containingBlock()) {
        if (!hasAccountedForBorderAndPadding)
            toEnclosingListItem -= borderAndPaddingOnMarkerSide(*ancestor);
        if (is<RenderListItem>(*ancestor))
            break;

        toEnclosingListItem -= ancestor->marginStart(listItem.writingMode());
        if (ancestor->isFlexItem()) {
            toEnclosingListItem -= ancestor->logicalLeft();
            hasAccountedForBorderAndPadding = true;
            continue;
        }
        hasAccountedForBorderAndPadding = false;
    }

    auto toAssociatedListItem = LayoutUnit { };
    if (ancestor && ancestor.get() != &listItem) {
        for (CheckedPtr<const RenderBoxModelObject> box = ancestor->containingBlock(); box; box = box->containingBlock()) {
            toAssociatedListItem -= (box->marginStart(listItem.writingMode()) + borderAndPaddingOnMarkerSide(*box));
            if (box.get() == &listItem)
                break;
        }
        // A float pushing the line start inwards also constrains the marker, so it stays with the line rather than
        // moving out to its own list item. Nesting list items then all end up with their marker in the same place.
        if (isLineStartConstrainedByFloat)
            toAssociatedListItem = { };
    }

    // The offsets above are inline start relative (negative means further towards the inline start), while what we return is a logical left offset, so in a right to left inline direction it points the other way.
    if (!isLeftToRight)
        return -(toEnclosingListItem + toAssociatedListItem);
    return toEnclosingListItem + toAssociatedListItem;
}

RenderListItem::RenderListItem(Element& element, UniqueRef<Style::ComputedStyle>&& style)
    : RenderBlockFlow(Type::ListItem, element, WTF::move(style))
{
    ASSERT(isRenderListItem());
    setInline(false);
}

RenderListItem::~RenderListItem()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
    ASSERT(!m_marker);
}

UniqueRef<Style::ComputedStyle> RenderListItem::computeMarkerStyle() const
{
    auto markerStyle = [&] {
        if (!is<PseudoElement>(element())) {
            if (auto markerStyle = style().pseudoElementStyle({ PseudoElementType::Marker }))
                return Style::ComputedStyle::clone(*markerStyle);
        }

        // The marker always inherits from the list item, regardless of where it might end
        // up (e.g., in some deeply nested line box). See CSS3 spec.
        auto markerStyle = Style::ComputedStyle::create();
        markerStyle->inheritFrom(style());

        // In the case of a ::before or ::after pseudo-element, we manually apply the properties
        // otherwise set in the user-agent stylesheet since we don't support ::before::marker or
        // ::after::marker. See bugs.webkit.org/b/218897.
        auto fontDescription = style().fontDescription();
        fontDescription.setVariantNumericSpacing(FontVariantNumericSpacing::TabularNumbers);
        markerStyle->setFontDescription(WTF::move(fontDescription));
        markerStyle->setUnicodeBidi(UnicodeBidi::Isolate);
        markerStyle->setWhiteSpaceCollapse(WhiteSpaceCollapse::Preserve);
        markerStyle->setTextWrapMode(TextWrapMode::NoWrap);
        markerStyle->setTextTransform({ });
        return markerStyle;
    }();

    markerStyle->setPseudoElementIdentifier({ { PseudoElementType::Marker } });

    // A disclosure triangle is drawn from the system font rather than from the one the marker inherited, so that is the
    // font the marker has: it is what the glyph is measured with too, the way it is for every other marker.
    if (listMarkerIsDisclosure(markerStyle, protect(document()))) {
        auto fontDescription = FontCascadeDescription { markerStyle->fontDescription() };
        fontDescription.setFamilies({ { "system-ui"_s, FontFamilyKind::Generic } });
        markerStyle->setFontDescription(WTF::move(fontDescription));
        markerStyle->fontCascade().update(&protect(document())->fontSelector());
    }

    // The marker box is a text-decoration boundary: the originating element's text-decoration must
    // not propagate into the marker's generated contents, matching list-style-type markers which are
    // never decorated by the list's text-decoration.
    markerStyle->setTextDecorationLineInEffect(markerStyle->textDecorationLine());

    // text-align neither applies to nor is inherited by ::marker (css-pseudo-4): reset the value
    // inherited from the originating element so it cannot leak into generated marker contents.
    // (list-style-type markers ignore text-align as well.)
    markerStyle->setTextAlign(Style::ComputedStyle::initialTextAlign());
    markerStyle->setTextAlignLast(Style::ComputedStyle::initialTextAlignLast());

    return markerStyle;
}

bool isHTMLListElement(const Node& node)
{
    return isAnyOf<HTMLUListElement, HTMLOListElement>(node);
}

// Returns the enclosing list with respect to the DOM order.
static Element* enclosingList(const RenderListItem& listItem)
{
    SUPPRESS_UNCOUNTED_LOCAL auto* element = listItem.element();
    SUPPRESS_UNCOUNTED_LOCAL auto* pseudoElement = dynamicDowncast<PseudoElement>(element);
    SUPPRESS_UNCOUNTED_LOCAL auto* parent = pseudoElement ? pseudoElement->hostElement() : element->parentElement();
    for (SUPPRESS_UNCOUNTED_LOCAL auto* ancestor = parent; ancestor; ancestor = ancestor->parentElement()) {
        if (isHTMLListElement(*ancestor) || (ancestor->renderer() && ancestor->renderer()->shouldApplyStyleContainment()))
            return ancestor;
    }

    // If there's no actual list element, then the parent element acts as our
    // list for purposes of determining what other list items should be numbered as
    // part of the same list.
    return parent;
}

static RenderListItem* nextListItemHelper(const Element& list, const Element& element)
{
    RefPtr current = &element;
    auto advance = [&] {
        if (!current->renderOrDisplayContentsStyle())
            current = ElementTraversal::nextIncludingPseudoSkippingChildren(*current, &list);
        else
            current = ElementTraversal::nextIncludingPseudo(*current, &list);
    };
    advance();
    while (current) {
        auto* item = dynamicDowncast<RenderListItem>(current->renderer());
        if (!item) {
            advance();
            continue;
        }
        RefPtr otherList = enclosingList(*item);
        if (!otherList) {
            advance();
            continue;
        }

        // This item is part of our current list, so it's what we're looking for.
        if (&list == otherList)
            return item;

        // We found ourself inside another list; skip the rest of its contents.
        current = ElementTraversal::nextIncludingPseudoSkippingChildren(*current, &list);
    }

    return nullptr;
}

static inline RenderListItem* nextListItem(const Element& list, const RenderListItem& item)
{
    return nextListItemHelper(list, protect(*item.element()));
}

static inline RenderListItem* firstListItem(const Element& list)
{
    return nextListItemHelper(list, list);
}

static RenderListItem* previousListItem(const Element& list, const RenderListItem& item)
{
    RefPtr current = item.element();
    auto advance = [&] {
        current = ElementTraversal::previousIncludingPseudo(*current, &list);
    };
    advance();
    while (current) {
        auto* item = dynamicDowncast<RenderListItem>(current->renderer());
        if (!item) {
            advance();
            continue;
        }
        RefPtr otherList = enclosingList(*item);
        if (!otherList) {
            advance();
            continue;
        }

        // This item is part of our current list, so we found what we're looking for.
        if (&list == otherList)
            return item;

        // We found ourself inside another list; skip the rest of its contents by
        // advancing to it. However, since the list itself might be a list item,
        // don't advance past it.
        current = otherList;
    }
    return nullptr;
}

void RenderListItem::updateItemValuesForOrderedList(const HTMLOListElement& list)
{
    for (CheckedPtr listItem = firstListItem(list); listItem; listItem = nextListItem(list, *listItem))
        listItem->updateValueAndMarkerContent();
}

unsigned RenderListItem::itemCountForOrderedList(const HTMLOListElement& list)
{
    unsigned itemCount = 0;
    for (CheckedPtr listItem = firstListItem(list); listItem; listItem = nextListItem(list, *listItem))
        ++itemCount;
    return itemCount;
}

int RenderListItem::startForReversedOrderedList(const HTMLOListElement& list)
{
    ASSERT(list.isReversed() && !list.hasExplicitStart());
    size_t itemsBefore = 0;
    for (CheckedPtr item = firstListItem(list); item; item = nextListItem(list, *item)) {
        auto directives = item->style().usedCounterDirectives().map.get("list-item"_s);
        if (directives.setValue)
            return itemsBefore + *directives.setValue;
        ++itemsBefore;
    }
    return list.start();
}

void RenderListItem::updateValueNow() const
{
    RefPtr list = enclosingList(*this);
    RefPtr orderedList = dynamicDowncast<HTMLOListElement>(list);

    // The start item is either the closest item before this one in the list that already has a value,
    // or the first item in the list if none have before this have values yet.
    // FIXME: This should skip over items with counter-reset.
    auto* startItem = this;
    if (list) {
        auto* item = this;
        while ((item = previousListItem(*list, *item))) {
            startItem = item;
            if (item->m_value)
                break;
        }
    }

    int defaultIncrement = orderedList && orderedList->isReversed() ? -1 : 1;
    auto valueForItem = [&](int previousValue, CounterDirectives& directives) {
        if (directives.setValue)
            return *directives.setValue;
        int increment = directives.incrementValue.value_or(defaultIncrement);
        if (directives.resetValue)
            return *directives.resetValue + increment;
        return previousValue + increment;
    };

    auto& startValue = startItem->m_value;
    if (!startValue) {
        // Take in account enclosing list counter-reset.
        // FIXME: This can be a lot more simple when lists use presentational hints.
        if (list && list->renderer()) {
            auto listDirectives = list->renderer()->style().usedCounterDirectives().map.get("list-item"_s);
            if (listDirectives.resetValue)
                startValue = *listDirectives.resetValue;
            else if (orderedList && orderedList->isReversed() && !orderedList->hasExplicitStart())
                startValue = startForReversedOrderedList(*orderedList) - defaultIncrement;
            else
                startValue = orderedList ? orderedList->start() - defaultIncrement : 0;
        }
        auto directives = startItem->style().usedCounterDirectives().map.get("list-item"_s);
        startValue = valueForItem(startValue.value_or(0), directives);
    }

    int value = *startValue;

    for (auto* item = startItem; item != this; ) {
        item = nextListItem(*list, *item);
        auto directives = item->style().usedCounterDirectives().map.get("list-item"_s);
        item->m_value = valueForItem(value, directives);
        // counter-reset creates a new nested counter, so it should not be counted towards the current counter.
        if (!directives.resetValue)
            value = *item->m_value;
    }
}

void RenderListItem::updateValue()
{
    m_value = std::nullopt;
    if (CheckedPtr marker = markerBox()) {
        marker->setNeedsLayoutAndInvalidateContentLogicalWidths();
        if (m_outsideMarkerPosition)
            invalidateOutsideMarkerContainer();
    }
}

static RenderImage* imageForMarker(const RenderBlockFlow& marker)
{
    ASSERT(marker.isOutsideListMarker());
    if (listMarkerHasContent(marker.style(), marker.document()) || !listMarkerImage(marker.style()))
        return nullptr;
    return dynamicDowncast<RenderImage>(marker.firstChild());
}

RenderImage* RenderListItem::markerImage() const
{
    CheckedPtr marker = markerBox();
    return marker ? imageForMarker(*marker) : nullptr;
}

LayoutSize RenderListItem::markerImageSize(const Style::Image& image) const
{
    CheckedPtr marker = markerBox();
    if (!marker)
        return { };
    // FIXME: The spec says this should be 1em - https://drafts.csswg.org/css-lists-3/#valdef-list-style-image-image
    LayoutUnit bulletWidth = protect(marker->style())->metricsOfPrimaryFont().intAscent() / 2_lu;
    return marker->calculateImageIntrinsicDimensions(image, Style::ListStyleImageSizing { bulletWidth }, ScaleByUsedZoom::Yes);
}

void RenderListItem::updateMarkerContent()
{
    if (CheckedPtr markerBox = this->markerBox()) {
        // css-lists-3 §3.3: with a non-normal `content` the marker's contents are the author's, not ours to fill in.
        if (listMarkerHasContent(markerBox->style(), document()))
            return;

        if (CheckedPtr imageRenderer = markerImage()) {
            RefPtr image = listMarkerImage(markerBox->style());
            auto imageSize = markerImageSize(*image);
            auto usedZoom = imageRenderer->style().usedZoomForLength();
            auto logicalWidth = Style::PreferredSize { Style::PreferredSize::Fixed { imageSize.width() / usedZoom.value } };
            auto logicalHeight = Style::PreferredSize { Style::PreferredSize::Fixed { imageSize.height() / usedZoom.value } };
            if (imageRenderer->style().width() == logicalWidth && imageRenderer->style().height() == logicalHeight)
                return;
            imageRenderer->mutableStyle().setWidth(WTF::move(logicalWidth));
            imageRenderer->mutableStyle().setHeight(WTF::move(logicalHeight));
            imageRenderer->setNeedsLayout();
            return;
        }

        // The marker text is only known here (counter values resolve at layout), while the renderer holding it was
        // created from style. Push the text down so the inline formatting context has something to lay out.
        CheckedPtr textRenderer = dynamicDowncast<RenderText>(markerBox->firstChild());
        if (!textRenderer)
            return;
        auto textContent = listMarkerTextContent(markerBox->style(), *this);
        if (listMarkerSynthesizesGlyph(markerBox->style(), protect(document()))) {
            textRenderer->setText(textContent.textWithoutSuffix().toString());
            return;
        }
        textRenderer->setText(textContent.textWithSuffix);
        return;
    }

    CheckedPtr marker = m_marker;
    // css-lists-3 §3.3: with a non-normal `content` the marker's contents are the author's, not ours to fill in.
    if (!marker || marker->style().content().isData())
        return;

    if (CheckedPtr imageRenderer = dynamicDowncast<RenderImage>(marker->firstChild())) {
        // The image is a renderer of its own; all the marker owes the line is the room after it, which the image keeps
        // on its own end edge so that the inline formatting context puts it on the side the line runs towards.
        setListMarkerInlineMargins(imageRenderer->mutableStyle(), writingMode(), { }, listMarkerImagePadding);
        return;
    }

    CheckedPtr textRenderer = dynamicDowncast<RenderText>(marker->firstChild());
    if (!textRenderer)
        return;

    auto textContent = listMarkerTextContent(marker->style(), *this);
    if (!listMarkerSynthesizesGlyph(marker->style(), protect(document()))) {
        textRenderer->setText(textContent.textWithSuffix);
        return;
    }

    // The glyph stands alone and the room after it is a margin rather than the suffix space, so that word-spacing and
    // letter-spacing leave it be. TextBoxPainter draws it at a size the font metrics give it rather than at the counter
    // style character's, so the room after it is measured from that same size (see Layout::TextUtil::width).
    textRenderer->setText(textContent.textWithoutSuffix().toString());
    auto& fontMetrics = marker->style().metricsOfPrimaryFont();
    auto glyphWidth = LayoutUnit { (fontMetrics.intAscent() * 2 / 3 + 1) / 2 + 2 };
    setListMarkerInlineMargins(marker->mutableStyle(), writingMode(), -1_lu, fontMetrics.intAscent() - glyphWidth + 1);
}

void RenderListItem::updateValueAndMarkerContent()
{
    updateValue();
    // Nothing is changing the render tree here, so there is nothing to wait for: fill the marker's text in right away
    // rather than leave it to layout (see RenderTreeBuilder::updateListMarkerContents()).
    updateMarkerContent();
}

// An outside marker shows its list item's list-style-image (it can't have one of its own), so the list item is the image's
// client and updates the marker in imageChanged(): a failed load turns it into a text marker. An inside marker's RenderImage
// is a client of the image itself.
static RefPtr<Style::Image> outsideMarkerImage(const Style::ComputedStyle& style)
{
    if (style.listStylePosition() != ListStylePosition::Outside)
        return nullptr;
    return style.listStyleImage().tryStyleImage();
}

void RenderListItem::willBeDestroyed()
{
    if (RefPtr image = outsideMarkerImage(style()))
        image->removeClient(*this);
    RenderBlockFlow::willBeDestroyed();
}

void RenderListItem::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);

    if (diff == Style::DifferenceResult::Layout) {
        if (oldStyle && oldStyle->usedCounterDirectives().map.get("list-item"_s) != style().usedCounterDirectives().map.get("list-item"_s))
            usedCounterDirectivesChanged();
        if (m_outsideMarkerPosition)
            invalidateOutsideMarkerContainer();
    }

    RefPtr oldImage = oldStyle ? outsideMarkerImage(*oldStyle) : nullptr;
    RefPtr newImage = outsideMarkerImage(style());
    if (oldImage != newImage) {
        if (oldImage)
            oldImage->removeClient(*this);
        if (newImage)
            newImage->addClient(*this);
    }
}

void RenderListItem::forEachStyleImage(NOESCAPE const ScopedLambda<void(Style::Image&)>& function)
{
    RenderBlockFlow::forEachStyleImage(function);
    if (RefPtr image = outsideMarkerImage(style()))
        function(*image);
}

void RenderListItem::imageChanged(WrappedImagePtr changedImage, const IntRect* rect)
{
    RefPtr image = outsideMarkerImage(style());
    if (CheckedPtr marker = markerBox(); marker && image && changedImage == image->data()) {
        if (image->errorOccurred()) {
            // A failed image turns this into a text marker, and that text needs renderers the marker was not built with.
            RefPtr element = this->element();
            if (RefPtr pseudoElement = dynamicDowncast<PseudoElement>(element))
                element = pseudoElement->hostElement();
            if (element)
                element->invalidateStyleAndRenderersForSubtree();
            marker->setNeedsLayoutAndInvalidateContentLogicalWidths();
        } else if (CheckedPtr imageRenderer = markerImage(); imageRenderer && imageRenderer->borderBoxSize() != markerImageSize(*image)) {
            updateMarkerContent();
            marker->setNeedsLayoutAndInvalidateContentLogicalWidths();
        } else
            marker->repaint();
    }
    RenderBlockFlow::imageChanged(changedImage, rect);
}

std::pair<LayoutUnit, LayoutUnit> RenderListItem::computeIntrinsicLogicalWidths() const
{
    if (CheckedPtr outsideMarker = markerBox(); outsideMarker && !outsideMarker->nextSibling())
        return { };

    return RenderBlockFlow::computeIntrinsicLogicalWidths();
}

void RenderListItem::layoutBlock(RelayoutChildren relayoutChildren, LayoutUnit pageLogicalHeight)
{
    CheckedPtr outsideMarker = markerBox();
    if (!outsideMarker) {
        m_outsideMarkerPosition = { };
        return RenderBlockFlow::layoutBlock(relayoutChildren, pageLogicalHeight);
    }

    auto markerScope = ListItemOutsideMarkerScope { view().frameView().layoutContext(), *this };
    RenderBlockFlow::layoutBlock(relayoutChildren, pageLogicalHeight);
    placeOutsideMarker(*outsideMarker);
    addOverflowFromContainedBox(*outsideMarker);
}

static std::pair<float, float> layoutBoundsForMarkerText(const Style::ComputedStyle& style, const String& text)
{
    // FIXME: This should be part of InlineBoxBuilder (webkit.org/b/294342)
    // This is essentially what we do in LineBoxBuilder::enclosingAscentDescentWithFallbackFonts.
    auto ascentAndDescent = [&](auto& fontMetrics) {
        auto ascent = fontMetrics.ascent();
        auto descent = fontMetrics.descent();
        auto halfLeading = (fontMetrics.lineSpacing() - (ascent + descent)) / 2.f;
        return std::pair<float, float> { ascent + halfLeading, descent + halfLeading };
    };
    auto& metricsOfPrimaryFont = style.metricsOfPrimaryFont();
    auto primaryFontHeight = metricsOfPrimaryFont.height();

    if (style.textAutosizingAdjustedLineHeight().isNormal()) {
        auto maxAscentAndDescent = ascentAndDescent(metricsOfPrimaryFont);

        for (Ref fallbackFont : Layout::TextUtil::fallbackFontsForText(text, style, Layout::TextUtil::IncludeHyphen::No)) {
            auto& fontMetrics = fallbackFont->fontMetrics();
            if (primaryFontHeight >= fontMetrics.height()) {
                // FIXME: Figure out why certain symbols (e.g. disclosure-open) would initiate fallback fonts with just slightly different (subpixel) metrics.
                // This is mainly about preserving legacy behavior.
                continue;
            }
            auto ascentDescent = ascentAndDescent(fontMetrics);
            maxAscentAndDescent.first = std::max(maxAscentAndDescent.first, ascentDescent.first);
            maxAscentAndDescent.second = std::max(maxAscentAndDescent.second, ascentDescent.second);
        }
        return { maxAscentAndDescent.first, maxAscentAndDescent.second };
    }

    auto primaryFontAscentAndDescent = ascentAndDescent(metricsOfPrimaryFont);
    auto halfLeading = (style.usedLineHeight() - (primaryFontAscentAndDescent.first + primaryFontAscentAndDescent.second)) / 2.f;
    return { primaryFontAscentAndDescent.first + halfLeading, primaryFontAscentAndDescent.second + halfLeading };
}

static std::pair<float, float> outsideMarkerLayoutBounds(const RenderBlockFlow& marker, const String& markerText)
{
    ASSERT(marker.isOutsideListMarker());
    if (CheckedPtr image = imageForMarker(marker))
        return { image->logicalHeight().toFloat(), 0.f };

    CheckedRef style = marker.style();
    if (listMarkerHasContent(style, protect(marker.document()))) {
        auto markerLogicalHeight = marker.logicalHeight();
        auto contentBaseline = marker.firstLineBaseline().value_or(markerLogicalHeight);
        return { contentBaseline.toFloat(), (markerLogicalHeight - contentBaseline).toFloat() };
    }
    return layoutBoundsForMarkerText(style, markerText);
}

LayoutUnit outsideMarkerMarginStart(const RenderBlockFlow& marker)
{
    ASSERT(marker.isOutsideListMarker());
    if (imageForMarker(marker))
        return -marker.logicalWidth() - listMarkerImagePadding;
    CheckedRef style = marker.style();
    if (listMarkerSynthesizesGlyph(style, protect(marker.document())))
        return LayoutUnit { -(style->metricsOfPrimaryFont().intAscent() * 2 / 3) - listMarkerImagePadding - 1 };
    return -marker.logicalWidth();
}

// The offset from the block-start edge of an outside marker's primary font box (its image, for an image marker) to the marker's own block-start edge, so that box can be placed instead of the marker. A marker with `content` is placed by its own box, so this is 0.
static LayoutUnit outsideMarkerContentLogicalTop(const RenderBlockFlow& marker)
{
    ASSERT(marker.isOutsideListMarker());
    if (CheckedPtr image = imageForMarker(marker))
        return -image->logicalTop();
    CheckedRef style = marker.style();
    if (listMarkerHasContent(style, protect(marker.document())))
        return 0_lu;
    // fontBaseline is the primary font's baseline from the block-start edge of its box, the way the marker's first baseline is from the marker's block-start edge. The block-start edge is the line-under edge when lines are inverted.
    auto& metricsOfPrimaryFont = style->metricsOfPrimaryFont();
    auto writingMode = marker.writingMode();
    auto fontLogicalHeight = LayoutUnit { metricsOfPrimaryFont.intHeight() };
    auto fontBaseline = [&] {
        if (writingMode.prefersCentralBaseline())
            return fontLogicalHeight / 2;
        auto ascent = LayoutUnit { metricsOfPrimaryFont.ascent(BaselineAlignment::dominantBaseline(writingMode)) };
        return writingMode.isLineInverted() ? fontLogicalHeight - ascent : ascent;
    }();
    return fontBaseline - marker.firstLineBaseline().value_or(marker.logicalHeight());
}

void RenderListItem::layoutExcludedChildren(RelayoutChildren relayoutChildren)
{
    CheckedPtr outsideMarker = markerBox();
    if (!outsideMarker)
        return RenderBlockFlow::layoutExcludedChildren(relayoutChildren);

    auto markerNeedsOwnLine = [&] {
        // Quirks mode puts the marker of a list item that starts with a nested list on a line of its own above that list, rather than on the line its first item makes.
        for (CheckedPtr child = firstChild(); child; child = child->nextSibling()) {
            auto boxType = markerSearchBoxType(*child);
            if (boxType != MarkerSearchBoxType::Opaque)
                return boxType == MarkerSearchBoxType::NestedListInQuirksMode;
        }
        return false;
    };

    // The marker takes no part in layout, so a line of its own is room this list item makes for it before its block
    // children are placed. layoutBlockChildren calls us right after it starts them at the content edge.
    if (!childrenInline() && markerNeedsOwnLine())
        setLogicalHeight(logicalHeight() + lineHeight());

    outsideMarker->setIsExcludedFromNormalLayout(true);
    if (relayoutChildren == RelayoutChildren::Yes)
        outsideMarker->setNeedsLayout(MarkingBehavior::OnlyThis);
    auto markerNeedsLayout = outsideMarker->needsLayout();
    outsideMarker->layoutIfNeeded();

    if (markerNeedsLayout) {
        auto layoutBounds = outsideMarkerLayoutBounds(*outsideMarker, markerText());
        // The line the marker is aligned with made room for it, so it has to be laid out again when the marker needs a different amount.
        if (m_outsideMarkerPosition && layoutBounds != m_markerLayoutBounds)
            invalidateOutsideMarkerContainer();
        m_markerLayoutBounds = layoutBounds;
    }

    RenderBlockFlow::layoutExcludedChildren(relayoutChildren);
}

void RenderListItem::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!logicalHeight() && hasNonVisibleOverflow())
        return;

    RenderBlockFlow::paint(paintInfo, paintOffset);
}

void RenderListItem::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (CheckedPtr outsideMarker = markerBox(); outsideMarker && !outsideMarker->hasSelfPaintingLayer())
        outsideMarker->paintAsInlineBlock(paintInfo, blockStartRelativeToPhysicalForChild(*outsideMarker, paintOffset));

    RenderBlockFlow::paintObject(paintInfo, paintOffset);
}

bool RenderListItem::hasLineIfEmpty() const
{
    if (markerBox())
        return true;

    return RenderBlockFlow::hasLineIfEmpty();
}

bool RenderListItem::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (RenderBlockFlow::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction))
        return true;

    CheckedPtr outsideMarker = markerBox();
    if (!outsideMarker || outsideMarker->hasSelfPaintingLayer())
        return false;

    // The marker's own location is relative to us, so it needs our adjusted one, the way hitTestChildren gets it.
    auto adjustedLocation = accumulatedOffset + location();
    // This is the virtual RenderBlock::nodeAtPoint: RenderBlock declares it protected, so call it through RenderBox, where it is public.
    return static_cast<RenderBox&>(*outsideMarker).nodeAtPoint(request, result, locationInContainer, blockStartRelativeToPhysicalForChild(*outsideMarker, adjustedLocation), hitTestAction);
}

RenderBlockFlow* RenderListItem::markerBox() const
{
    // An inside marker is an inline box, so a block marker is the outside one.
    return dynamicDowncast<RenderBlockFlow>(m_marker);
}

RenderListItem* RenderListItem::listItemForMarker(const RenderObject& marker)
{
    // An outside marker is its list item's child and an inside marker an anonymous inline box in its content, so find
    // the list item whose marker it is.
    for (CheckedPtr ancestor = marker.parent(); ancestor; ancestor = ancestor->parent()) {
        auto* listItem = dynamicDowncast<RenderListItem>(ancestor);
        if (listItem && listItem->markerRenderer() == &marker)
            return listItem;
    }
    return nullptr;
}

String RenderListItem::markerText(ListMarkerIncludeSuffix includeSuffix) const
{
    CheckedPtr marker = m_marker.get();
    if (!marker)
        return { };

    // css-lists-3 §3.3: with a non-normal `content` the marker says whatever its content renderers hold, unless the
    // author gave it alternative text (css-content-3 §1.2).
    if (listMarkerHasContent(marker->style(), document())) {
        if (auto altText = marker->style().content().altText(); !altText.isNull())
            return altText;
        StringBuilder text;
        for (auto& textRenderer : descendantsOfType<RenderText>(*marker))
            text.append(textRenderer.text());
        return text.toString();
    }

    if (listMarkerImage(marker->style()))
        return { };

    auto textContent = listMarkerTextContent(marker->style(), const_cast<RenderListItem&>(*this));
    return includeSuffix == ListMarkerIncludeSuffix::Yes ? textContent.textWithSuffix : textContent.textWithoutSuffix().toString();
}

String RenderListItem::markerText(const RenderObject& marker, ListMarkerIncludeSuffix includeSuffix)
{
    CheckedPtr listItem = listItemForMarker(marker);
    // A marker that is being detached from its item has no item to ask.
    return listItem ? listItem->markerText(includeSuffix) : String();
}

void RenderListItem::usedCounterDirectivesChanged()
{
    if (CheckedPtr marker = markerBox())
        marker->setNeedsLayoutAndInvalidateContentLogicalWidths();

    updateValueAndMarkerContent();
    RefPtr list = enclosingList(*this);
    if (!list)
        return;
    CheckedPtr item = this;
    while ((item = nextListItem(*list, *item)))
        item->updateValueAndMarkerContent();
}

Vector<CheckedRef<RenderListItem>> RenderListItem::updateListMarkerNumbers()
{
    RefPtr list = enclosingList(*this);
    if (!list)
        return { };

    bool isInReversedOrderedList = false;
    if (auto* orderedList = dynamicDowncast<HTMLOListElement>(*list)) {
        orderedList->itemCountChanged();
        isInReversedOrderedList = orderedList->isReversed();
    }

    // If an item has been marked for update before, we know that all following items have, too.
    // This gives us the opportunity to stop and avoid marking the same nodes again.
    Vector<CheckedRef<RenderListItem>> itemsNeedingMarkerUpdate;
    auto* item = this;
    auto subsequentListItem = isInReversedOrderedList ? previousListItem : nextListItem;
    while ((item = subsequentListItem(*list, *item)) && item->m_value) {
        item->updateValue();
        itemsNeedingMarkerUpdate.append(*item);
    }
    return itemsNeedingMarkerUpdate;
}

bool RenderListItem::isInReversedOrderedList() const
{
    RefPtr list = dynamicDowncast<HTMLOListElement>(enclosingList(*this));
    return list && list->isReversed();
}

void RenderListItem::placeOutsideMarker(RenderBlockFlow& marker)
{
    ASSERT(marker.isOutsideListMarker());
    auto& markerPosition = m_outsideMarkerPosition;
    CheckedPtr firstFormattedLineRoot = markerPosition ? markerPosition->firstFormattedLineRoot.get() : nullptr;
    if (!firstFormattedLineRoot) {
        // No line took the marker, so there is no formatted line to align it with. Put the top of its primary font or
        // image at the top of our content box. Like a marker on a collapsed line, it adds no height.
        setLogicalTopForChild(marker, borderAndPaddingBefore() + outsideMarkerContentLogicalTop(marker));
        auto marginStart = outsideMarkerMarginStart(marker);
        auto markerLogicalLeft = writingMode().isLogicalLeftInlineStart() ? marginStart : logicalWidth() - logicalWidthForChild(marker) - marginStart;
        setLogicalLeftForChild(marker, markerLogicalLeft);
        return;
    }

    // Line layout placed the marker on the line as if it belonged to the list item establishing that formatting context.
    // Take it from there: out to our own border box start, then across the in-flow content up to us.
    auto logicalTop = LayoutUnit { markerPosition->topLeft.y() };
    auto logicalLeft = LayoutUnit { markerPosition->topLeft.x() } + outsideMarkerLogicalLeftOffsetFor(*firstFormattedLineRoot, *this, markerPosition->isLineStartConstrainedByFloat);
    for (CheckedPtr<RenderBlock> ancestor = firstFormattedLineRoot; ancestor && ancestor != this; ancestor = ancestor->containingBlock()) {
        // Inside a fragmented flow the coordinates are in flow thread space, where the content is one continuous
        // strip. The column the line ended up in is a sibling of the flow thread rather than an ancestor, so leave
        // the flow thread for that column set and carry on from there, in visual space.
        if (CheckedPtr fragmentedFlow = dynamicDowncast<RenderMultiColumnFlow>(ancestor)) {
            CheckedPtr columnSet = dynamicDowncast<RenderMultiColumnSet>(static_cast<const RenderFragmentedFlow&>(*fragmentedFlow).fragmentAtBlockOffset(fragmentedFlow.get(), logicalTop, true));
            if (!columnSet)
                continue;
            auto translation = fragmentedFlow->physicalTranslationOffsetFromFlowToFragment(columnSet.get(), logicalTop);
            logicalTop += translation.height();
            logicalLeft += translation.width();
            ancestor = columnSet.get();
        }
        logicalTop += ancestor->logicalTop();
        logicalLeft += ancestor->logicalLeft();
    }
    setLogicalTopForChild(marker, logicalTop);
    setLogicalLeftForChild(marker, logicalLeft);
}

RenderListItem::FirstFormattedLineCandidate RenderListItem::firstFormattedLineRootFor(RenderBlock& blockContainer, const RenderBoxModelObject& marker)
{
    RenderBlock* fallbackParent = is<RenderListItem>(blockContainer) && blockContainer.childrenInline() ? &blockContainer : nullptr;

    auto firstFormattedLineRootIn = [&](RenderBlock& block) -> CheckedPtr<RenderBlock> {
        auto nestedResult = firstFormattedLineRootFor(block, marker);
        if (nestedResult.parent)
            return nestedResult.parent;

        if (!fallbackParent) {
            if (nestedResult.fallbackParent)
                fallbackParent = nestedResult.fallbackParent.get();
            else if (auto* firstInFlowChild = block.firstInFlowChild(); !firstInFlowChild || firstInFlowChild == &marker)
                fallbackParent = &block;
        }
        return { };
    };

    for (auto& child : childrenOfType<RenderObject>(blockContainer)) {
        if (&child == &marker)
            continue;

        switch (markerSearchBoxType(child)) {
        case MarkerSearchBoxType::Opaque:
            break;
        case MarkerSearchBoxType::NestedListInQuirksMode:
            return { { }, fallbackParent };
        case MarkerSearchBoxType::TableRubyOrReplaced:
            return { { }, fallbackParent };
        case MarkerSearchBoxType::InlineContent:
            // Neither an empty inline (e.g. <a id="anchor"></a>) nor collapsible whitespace produces a line.
            if (auto* text = dynamicDowncast<RenderText>(child); text && text->containsOnlyCollapsibleWhitespace()) {
                fallbackParent = &blockContainer;
                break;
            }
            if (child.isInlineBox()) {
                CheckedRef inlineBox = downcast<RenderBoxModelObject>(child);
                if (isEmptyInline(inlineBox.get())) {
                    fallbackParent = &blockContainer;
                    break;
                }
                // An inside marker takes a line ahead of the block, so only an outside marker follows the content in.
                if (marker.isOutsideListMarker()) {
                    if (CheckedPtr leadingBlock = dynamicDowncast<RenderBlock>(firstContentfulChild(inlineBox.get()))) {
                        if (auto firstFormattedLineRoot = firstFormattedLineRootIn(*leadingBlock))
                            return { firstFormattedLineRoot, { } };
                        break;
                    }
                }
            }
            return { &blockContainer, { } };
        case MarkerSearchBoxType::BlockContainer:
        case MarkerSearchBoxType::SpannerPlaceholder: {
            auto blockToDescendInto = [&] {
                if (CheckedPtr placeholder = dynamicDowncast<RenderMultiColumnSpannerPlaceholder>(child))
                    return dynamicDowncast<RenderBlock>(placeholder->spanner());
                return dynamicDowncast<RenderBlock>(child);
            };
            if (CheckedPtr block = blockToDescendInto()) {
                if (auto firstFormattedLineRoot = firstFormattedLineRootIn(*block))
                    return { firstFormattedLineRoot, { } };
            }
            break;
        }
        }
    }
    return { { }, fallbackParent };
}

Vector<CheckedPtr<RenderListItem>> RenderListItem::listItemsWithOutsideMarkerFor(const RenderBlockFlow& inlineRoot, const Vector<SingleThreadWeakPtr<RenderListItem>>& listItemsWithOutsideMarker)
{
    auto listItems = Vector<CheckedPtr<RenderListItem>> { };
    for (auto& listItem : listItemsWithOutsideMarker) {
        CheckedPtr marker = listItem ? listItem->markerBox() : nullptr;
        if (!marker) {
            ASSERT_NOT_REACHED();
            continue;
        }
        auto candidate = firstFormattedLineRootFor(*listItem, *marker);
        if ((candidate.parent ? candidate.parent : candidate.fallbackParent) == &inlineRoot)
            listItems.append(listItem.get());
    }
    return listItems;
}

void RenderListItem::setOutsideMarkerPosition(OutsideMarkerPosition&& position)
{
    ASSERT(position.firstFormattedLineRoot);
    m_outsideMarkerPosition = WTF::move(position);
}

void RenderListItem::invalidateOutsideMarkerContainer()
{
    ASSERT(m_outsideMarkerPosition);

    if (!m_outsideMarkerPosition || !m_outsideMarkerPosition->firstFormattedLineRoot) {
        m_outsideMarkerPosition = { };
        return;
    }

    if (m_outsideMarkerPosition->firstFormattedLineRoot->isDescendantOf(this))
        return m_outsideMarkerPosition->firstFormattedLineRoot->setNeedsLayout();

    m_outsideMarkerPosition = { };
}

void setListMarkerInlineMargins(Style::ComputedStyle& markerStyle, WritingMode listItemWritingMode, LayoutUnit marginStart, LayoutUnit marginEnd)
{
    auto zoom = markerStyle.usedZoomForLength().value;
    auto startEdge = Style::MarginEdge::Fixed { marginStart / zoom };
    auto endEdge = Style::MarginEdge::Fixed { marginEnd / zoom };
    if (listItemWritingMode.isHorizontal()) {
        auto startIsLeft = listItemWritingMode.isInlineLeftToRight();
        markerStyle.setMarginLeft(startIsLeft ? startEdge : endEdge);
        markerStyle.setMarginRight(startIsLeft ? endEdge : startEdge);
        return;
    }
    auto startIsTop = listItemWritingMode.isInlineTopToBottom();
    markerStyle.setMarginTop(startIsTop ? startEdge : endEdge);
    markerStyle.setMarginBottom(startIsTop ? endEdge : startEdge);
}

bool listMarkerIsDisclosure(const Style::ComputedStyle& markerStyle, Document& document)
{
    auto counterStyle = markerStyle.listStyleType().tryCounterStyle();
    if (!counterStyle)
        return false;
    auto system = document.counterStyleRegistry().resolvedCounterStyle(*counterStyle)->system();
    return system == CSSCounterStyleDescriptors::System::DisclosureClosed || system == CSSCounterStyleDescriptors::System::DisclosureOpen;
}

bool listMarkerIsDisclosure(const RenderElement* renderer)
{
    if (!renderer || !renderer->style().isListMarkerStyle())
        return false;
    return listMarkerIsDisclosure(protect(renderer->style()), protect(renderer->document()));
}

RefPtr<Style::Image> listMarkerImage(const Style::ComputedStyle& markerStyle)
{
    RefPtr image = markerStyle.listStyleImage().tryStyleImage();
    if (!image || image->errorOccurred())
        return nullptr;
    return image;
}

bool listMarkerHasContent(const Style::ComputedStyle& markerStyle, Document& document)
{
    return document.settings().cssMarkerContentEnabled() && markerStyle.content().isData();
}

bool listMarkerSynthesizesGlyph(const Style::ComputedStyle& markerStyle, Document& document)
{
    // css-lists-3 §3.3: a non-normal `content` supersedes list-style-type, and a list-style-image draws in place of
    // the glyph unless it failed to load, so neither leaves us a glyph to draw.
    if (listMarkerHasContent(markerStyle, document) || listMarkerImage(markerStyle))
        return false;

    auto& listType = markerStyle.listStyleType();
    return listType.isCircle() || listType.isDisc() || listType.isSquare();
}

ListMarkerTextContent listMarkerTextContent(const Style::ComputedStyle& markerStyle, RenderListItem& listItem)
{
    return WTF::switchOn(markerStyle.listStyleType(),
        [&](const CSS::Keyword::None&) {
            return ListMarkerTextContent { .textWithSuffix = " "_s, .textWithoutSuffixLength = 0 };
        },
        [&](const Style::String& identifier) {
            return ListMarkerTextContent { .textWithSuffix = identifier.value, .textWithoutSuffixLength = identifier.value.length() };
        },
        [&](const auto& counterStyle) {
            auto counter = listItem.document().counterStyleRegistry().resolvedCounterStyle(counterStyle);
            auto text = makeString(counter->prefix().text, counter->text(listItem.value(), markerStyle.writingMode()));
            return ListMarkerTextContent { .textWithSuffix = makeString(text, counter->suffix().text), .textWithoutSuffixLength = text.length() };
        }
    );
}

} // namespace WebCore
