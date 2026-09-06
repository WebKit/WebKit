/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 * Copyright (C) 2010 Daniel Bates (dbates@intudata.com)
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
#include "RenderListOutsideMarker.h"

#include "BaselineAlignment.h"
#include "CSSCounterStyleDescriptors.h"
#include "CSSCounterStyleRegistry.h"
#include "CSSFontSelector.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "FontCascade.h"
#include "FontCascadeInlines.h"
#include "FontCascadeDescription.h"
#include "GraphicsContext.h"
#include "InlineIteratorBoxInlines.h"
#include "PaintInfoInlines.h"
#include "PseudoElement.h"
#include "RenderBlockFlow.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderImage.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderMenuList.h"
#include "RenderObjectInlines.h"
#include "RenderTable.h"
#include "RenderText.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleContent.h"
#include "StyleListStyleType.h"
#include "StyleScope.h"
#include "TextUtil.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderListOutsideMarker);

RenderListOutsideMarker::RenderListOutsideMarker(RenderListItem& listItem, Style::ComputedStyle&& style)
    : RenderBox(Type::ListOutsideMarker, listItem.document(), WTF::move(style))
    , m_listItem(listItem)
{
    setInline(true);
    setBlockLevelReplacedOrAtomicInline(true); // pretend to be replaced
    ASSERT(isRenderListOutsideMarker());
}

// Do not add any code in below destructor. Add it to willBeDestroyed() instead.
RenderListOutsideMarker::~RenderListOutsideMarker() = default;

void RenderListOutsideMarker::willBeDestroyed()
{
    if (m_image)
        protect(m_image)->removeClient(*this);
    RenderBox::willBeDestroyed();
}

static Style::Difference NODELETE adjustedStyleDifference(Style::Difference diff, const Style::ComputedStyle& oldStyle, const Style::ComputedStyle& newStyle)
{
    if (diff >= Style::DifferenceResult::Layout)
        return diff;
    // FIXME: Preferably we do this at Style::ComputedStyle::changeRequiresLayout but checking against pseudo(::marker) is not sufficient.
    auto needsLayout = oldStyle.listStylePosition() != newStyle.listStylePosition()
        || oldStyle.listStyleType() != newStyle.listStyleType()
        || oldStyle.display().isInlineType() != newStyle.display().isInlineType();
    return needsLayout ? Style::DifferenceResult::Layout : diff;
}

void RenderListOutsideMarker::styleWillChange(Style::Difference diff, const Style::ComputedStyle& newStyle)
{
    RenderBox::styleWillChange(adjustedStyleDifference(diff, style(), newStyle), newStyle);
}

void RenderListOutsideMarker::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    if (oldStyle)
        diff = adjustedStyleDifference(diff, *oldStyle, style());
    RenderBox::styleDidChange(diff, oldStyle);

    propagateStyleToAnonymousChildren(StylePropagationType::AllChildren);

    if (RefPtr newImage = style().listStyleImage().tryStyleImage(); m_image != newImage) {
        if (m_image)
            protect(m_image)->removeClient(*this);
        m_image = WTF::move(newImage);
        if (m_image)
            protect(m_image)->addClient(*this);
    }
}

bool RenderListOutsideMarker::isImage() const
{
    // `content` supersedes list-style-image (css-lists-3 §3.3), so a marker with generated content
    // is never treated as an image marker (affects inline margins, baseline, and layout attributes).
    return m_image && !protect(m_image)->errorOccurred() && !hasContentProperty();
}

bool RenderListOutsideMarker::hasContentProperty() const
{
    return document().settings().cssMarkerContentEnabled() && style().content().isData();
}

RenderBlockFlow* RenderListOutsideMarker::contentContainer() const
{
    return dynamicDowncast<RenderBlockFlow>(firstChild());
}

LayoutRect RenderListOutsideMarker::localSelectionRect()
{
    return LayoutRect(LayoutPoint(), borderBoxSize());
}

void RenderListOutsideMarker::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (style().usedVisibility() != Visibility::Visible)
        return;

    LayoutPoint boxOrigin(paintOffset + location());
    LayoutRect overflowRect(visualOverflowRect());
    overflowRect.moveBy(boxOrigin);
    if (!paintInfo.rect.intersects(overflowRect))
        return;

    CheckedPtr container = contentContainer();
    if (!container)
        return;

    if (paintInfo.phase == PaintPhase::Accessibility) {
        paintInfo.accessibilityRegionContext()->takeBounds(*this, LayoutRect(boxOrigin, borderBoxSize()));
        return;
    }

    if (paintInfo.phase == PaintPhase::Foreground && selectionState() != HighlightState::None) {
        LayoutRect selectionRect = localSelectionRect();
        selectionRect.moveBy(boxOrigin);
        paintInfo.context().fillRect(snappedIntRect(selectionRect), m_listItem->selectionBackgroundColor());
    }

    // Paint the generated-content subtree as an atomic inline: paintAsInlineBlock fans out all
    // sub-phases for Foreground and forwards Selection/EventRegion/TextClip to the child's paint().
    container->paintAsInlineBlock(paintInfo, flipForWritingModeForChild(*container, boxOrigin));
}

void RenderListOutsideMarker::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    updateInlineMargins();
    if (CheckedPtr container = contentContainer())
        layoutContentContainer(*container);

    setMarginStart(0);
    setMarginEnd(0);

    if (auto fixedStartMargin = style().marginStart().tryFixed())
        setMarginStart(LayoutUnit(fixedStartMargin->resolveZoom(style().usedZoomForLength())));
    if (auto fixedEndMargin = style().marginEnd().tryFixed())
        setMarginEnd(LayoutUnit(fixedEndMargin->resolveZoom(style().usedZoomForLength())));

    clearNeedsLayout();
}

void RenderListOutsideMarker::layoutContentContainer(RenderBlockFlow& container)
{
    // The marker participates in its list item's line as a single atomic inline. Lay its
    // generated-content subtree out at its max-content (shrink-to-fit) width, like an
    // inline-block, then adopt the resulting size and baseline.
    auto contentLogicalWidth = container.maxContentLogicalWidthContribution();
    container.setOverridingBorderBoxLogicalWidth(contentLogicalWidth);
    container.setNeedsLayout(MarkingBehavior::MarkOnlyThis);
    container.layoutIfNeeded();
    container.clearOverridingBorderBoxLogicalWidth();

    setLogicalWidth(container.logicalWidth());

    auto contentBaseline = container.firstLineBaseline().value_or(container.logicalHeight());

    if (CheckedPtr imageRenderer = isImage() ? dynamicDowncast<RenderBox>(container.firstChild()) : nullptr) {
        container.setLogicalTop(-imageRenderer->logicalTop());
        setLogicalHeight(imageRenderer->logicalHeight());
        m_layoutBounds = { imageRenderer->logicalHeight(), 0 };
    } else {
        // The inline formatting context aligns a text marker box on the marker's primary-font baseline
        // (see InlineLineBoxBuilder), then we paint the content box at the marker box origin. Offset the
        // content box along the block axis so its own first-line baseline lands on that font baseline —
        // otherwise the content's line-box half-leading shifts it off the list item's baseline. Logical
        // setters keep this correct in vertical writing modes.
        auto markerAscent = LayoutUnit { style().metricsOfPrimaryFont().ascent(BaselineAlignment::dominantBaseline(writingMode())) };
        container.setLogicalTop(markerAscent - contentBaseline);

        if (hasContentProperty()) {
            setLogicalHeight(container.logicalHeight());
            m_layoutBounds = { contentBaseline, container.logicalHeight() - contentBaseline };
        } else {
            setLogicalHeight(style().metricsOfPrimaryFont().intHeight());
            m_layoutBounds = layoutBoundForTextContent(m_listItem->markerText());
        }
    }

    // The content box can extend outside the marker's border box (its baseline offset may be
    // negative, or the content taller than the marker font), so record its overflow. The marker
    // overrides layout() and otherwise reports no overflow, which would clip/mis-repaint the content.
    clearOverflow();
    addLayoutOverflow({ container.location(), container.borderBoxSize() });
    auto contentVisualOverflow = container.visualOverflowRect();
    contentVisualOverflow.moveBy(container.location());
    addVisualOverflow(contentVisualOverflow);
}

void RenderListOutsideMarker::imageChanged(WrappedImagePtr o, const IntRect* rect)
{
    if (parent()) {
        RefPtr image = m_image;
        if (image && o == image->data()) {
            if (image->errorOccurred()) {
                // A failed image turns this into a text marker, and that text needs renderers the marker was not built with.
                RefPtr element = m_listItem ? m_listItem->element() : nullptr;
                if (RefPtr pseudoElement = dynamicDowncast<PseudoElement>(element.get()))
                    element = pseudoElement->hostElement();
                if (element)
                    element->invalidateStyleAndRenderersForSubtree();
            }
            if (borderBoxSize() != LayoutSize(image->imageSize(this, style().usedZoom())) || image->errorOccurred()) {
                updateInlineMarginsAndContent();
                setNeedsLayoutAndInvalidateContentLogicalWidths();
            } else
                repaint();
        }
    }
    RenderBox::imageChanged(o, rect);
}

void RenderListOutsideMarker::updateInlineMarginsAndContent()
{
    updateContent();
    updateInlineMargins();
}

void RenderListOutsideMarker::updateContent()
{
    if (hasContentProperty()) {
        // css-lists-3 §3.3: `content` (not normal) supersedes list-style-image/type. The generated
        // content lives in the anonymous inline-block contentContainer(); the marker has no text/image.
        return;
    }

    if (isImage()) {
        // FIXME: This is a somewhat arbitrary width.
        LayoutUnit bulletWidth = style().metricsOfPrimaryFont().intAscent() / 2_lu;
        LayoutSize defaultBulletSize(bulletWidth, bulletWidth);
        setContentContainerImageSize(calculateImageIntrinsicDimensions(protect(m_image).get(), defaultBulletSize, ScaleByUsedZoom::Yes));
        return;
    }

    // The marker text is only known here (counter values resolve at layout), while the renderers
    // holding it were created from style. Push the text down so the inline formatting context has something to lay out.
    updateContentContainerText(listMarkerTextContent(style(), *m_listItem));
}

void RenderListOutsideMarker::setContentContainerImageSize(LayoutSize imageSize)
{
    CheckedPtr container = contentContainer();
    CheckedPtr imageRenderer = container ? dynamicDowncast<RenderImage>(container->firstChild()) : nullptr;
    if (!imageRenderer)
        return;

    auto usedZoom = imageRenderer->style().usedZoomForLength();
    auto logicalWidth = Style::PreferredSize { Style::PreferredSize::Fixed { imageSize.width() / usedZoom.value } };
    auto logicalHeight = Style::PreferredSize { Style::PreferredSize::Fixed { imageSize.height() / usedZoom.value } };
    if (imageRenderer->style().width() == logicalWidth && imageRenderer->style().height() == logicalHeight)
        return;

    imageRenderer->mutableStyle().setWidth(WTF::move(logicalWidth));
    imageRenderer->mutableStyle().setHeight(WTF::move(logicalHeight));
    imageRenderer->setNeedsLayout();
}

void RenderListOutsideMarker::updateContentContainerText(const ListMarkerTextContent& textContent)
{
    CheckedPtr container = contentContainer();
    if (!container) {
        ASSERT_NOT_REACHED();
        return;
    }

    CheckedPtr textRenderer = dynamicDowncast<RenderText>(container->firstChild());
    if (!textRenderer)
        return;

    if (synthesizesGlyph()) {
        textRenderer->setText(textContent.textWithoutSuffix().toString());
        return;
    }

    textRenderer->setText(textContent.textWithSuffix);
}

void RenderListOutsideMarker::computeIntrinsicLogicalWidthContributions()
{
    ASSERT(hasInvalidContentLogicalWidths());

    CheckedPtr container = contentContainer();
    ASSERT(container);

    // The marker is non-wrapping, so its min- and max-content widths are both the content's max-content width.
    auto logicalWidth = container ? container->maxContentLogicalWidthContribution() : 0_lu;
    m_minContentLogicalWidthContribution = logicalWidth;
    m_maxContentLogicalWidthContribution = logicalWidth;
    clearContentLogicalWidthsInvalidation();
    updateInlineMargins();
}

void RenderListOutsideMarker::updateInlineMargins()
{
    constexpr int markerPadding = listMarkerImagePadding;
    const FontMetrics& fontMetrics = style().metricsOfPrimaryFont();

    auto marginsForOutsideMarker = [&]() -> std::pair<LayoutUnit, LayoutUnit> {
        if (isImage())
            return { -minContentLogicalWidthContribution() - markerPadding, markerPadding };

        int offset = fontMetrics.intAscent() * 2 / 3;
        if (synthesizesGlyph())
            return { -offset - markerPadding - 1, offset + markerPadding + 1 - minContentLogicalWidthContribution() };

        return { -minContentLogicalWidthContribution(), 0 };
    };

    auto [marginStart, marginEnd] = marginsForOutsideMarker();
    setListMarkerInlineMargins(mutableStyle(), m_listItem->writingMode(), marginStart, marginEnd);
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

bool RenderListOutsideMarker::isDisclosureMarker() const
{
    return listMarkerIsDisclosure(style(), protect(document()));
}

RenderListItem* RenderListOutsideMarker::listItem() const
{
    return m_listItem.get();
}

void RenderListOutsideMarker::setExcludedPosition(ExcludedPosition excludedPosition)
{
    ASSERT(excludedPosition.firstFormattedLineRoot);

    m_excludedPosition = excludedPosition;
}

void RenderListOutsideMarker::invalidateExcludedMarkerContainer()
{
    ASSERT(m_excludedPosition);

    if (!m_excludedPosition || !m_excludedPosition->firstFormattedLineRoot) {
        m_excludedPosition = { };
        return;
    }

    if (m_excludedPosition->firstFormattedLineRoot->isDescendantOf(m_listItem.get()))
        return m_excludedPosition->firstFormattedLineRoot->setNeedsLayout();

    m_excludedPosition = { };
}

Node* RenderListOutsideMarker::nodeForHitTest() const
{
    return m_listItem ? m_listItem->element() : nullptr;
}

LayoutRect RenderListOutsideMarker::selectionRectForRepaint(const RenderLayerModelObject*, bool)
{
    ASSERT(!needsLayout());
    return { };
}

static RefPtr<CSSRegisteredCounterStyle> counterStyleFor(const Style::ComputedStyle& markerStyle, Document& document)
{
    auto counterStyle = markerStyle.listStyleType().tryCounterStyle();
    if (!counterStyle)
        return nullptr;
    return document.counterStyleRegistry().resolvedCounterStyle(*counterStyle);
}

bool listMarkerIsDisclosure(const Style::ComputedStyle& markerStyle, Document& document)
{
    RefPtr counterStyle = counterStyleFor(markerStyle, document);
    if (!counterStyle)
        return false;
    auto system = counterStyle->system();
    return system == CSSCounterStyleDescriptors::System::DisclosureClosed || system == CSSCounterStyleDescriptors::System::DisclosureOpen;
}

bool listMarkerIsDisclosure(const RenderElement* renderer)
{
    if (!renderer || !renderer->style().isListMarkerStyle())
        return false;
    return listMarkerIsDisclosure(renderer->style(), protect(renderer->document()));
}

bool listMarkerShowsImage(const Style::ComputedStyle& markerStyle)
{
    // css-lists-3 §3.3: a non-normal `content` supersedes list-style-image, which in turn draws in place of the
    // counter style's text unless it failed to load.
    if (markerStyle.content().isData())
        return false;
    RefPtr image = markerStyle.listStyleImage().tryStyleImage();
    return image && !image->errorOccurred();
}

bool listMarkerSynthesizesGlyph(const Style::ComputedStyle& markerStyle)
{
    if (markerStyle.content().isData() || listMarkerShowsImage(markerStyle))
        return false;

    auto& listType = markerStyle.listStyleType();
    return listType.isCircle() || listType.isDisc() || listType.isSquare();
}

ListMarkerTextContent listMarkerTextContent(const Style::ComputedStyle& markerStyle, RenderListItem& listItem)
{
    ListMarkerTextContent textContent;
    WTF::switchOn(markerStyle.listStyleType(),
        [&](const CSS::Keyword::None&) {
            textContent = { .textWithSuffix = " "_s, .textWithoutSuffixLength = 0 };
        },
        [&](const Style::String& identifier) {
            textContent = { .textWithSuffix = identifier.value, .textWithoutSuffixLength = identifier.value.length() };
        },
        [&](const Style::CounterStyle&) {
            auto counter = counterStyleFor(markerStyle, protect(listItem.document()));
            ASSERT(counter);
            if (!counter)
                return;
            auto text = makeString(counter->prefix().text, counter->text(listItem.value(), markerStyle.writingMode()));
            textContent = { .textWithSuffix = makeString(text, counter->suffix().text), .textWithoutSuffixLength = text.length() };
        }
    );
    return textContent;
}

bool RenderListOutsideMarker::synthesizesGlyph() const
{
    // `content` supersedes list-style-type, so a content marker never draws in place of a glyph, and a list-style-image marker draws the image instead.
    if (hasContentProperty() || isImage())
        return false;
    auto& listType = style().listStyleType();
    return listType.isCircle() || listType.isDisc() || listType.isSquare();
}

std::pair<float, float> RenderListOutsideMarker::layoutBoundForTextContent(String text) const
{
    // FIXME: This should be part of InlineBoxBuilder (webkit.org/b/294342)
    // This is essentially what we do in LineBoxBuilder::enclosingAscentDescentWithFallbackFonts.
    auto ascentAndDescent = [&] (auto& fontMetrics) {
        auto ascent = fontMetrics.ascent();
        auto descent = fontMetrics.descent();
        auto halfLeading = (fontMetrics.lineSpacing() - (ascent + descent)) / 2.f;
        return std::pair<float, float> { ascent + halfLeading, descent + halfLeading };
    };
    auto& style = this->style();
    auto& metricsOfPrimaryFont = style.metricsOfPrimaryFont();
    auto primaryFontHeight = metricsOfPrimaryFont.height();

    if (style.lineHeight().isNormal()) {
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
    auto halfLeading = (style.computedLineHeight() - (primaryFontAscentAndDescent.first + primaryFontAscentAndDescent.second)) / 2.f;
    return { primaryFontAscentAndDescent.first + halfLeading, primaryFontAscentAndDescent.second + halfLeading };
}

} // namespace WebCore
