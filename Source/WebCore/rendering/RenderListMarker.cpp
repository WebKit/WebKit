/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2022 Apple Inc. All rights reserved.
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
#include "RenderListMarker.h"

#include "CSSCounterStyleRegistry.h"
#include "Document.h"
#include "FontCascade.h"
#include "GraphicsContext.h"
#include "LegacyInlineElementBox.h"
#include "ListStyleType.h"
#include "RenderBoxInlines.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderView.h"
#include "StyleScope.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderListMarker);

constexpr int cMarkerPadding = 7;

RenderListMarker::RenderListMarker(RenderListItem& listItem, RenderStyle&& style)
    : RenderBox(Type::ListMarker, listItem.document(), WTFMove(style))
    , m_listItem(listItem)
{
    setInline(true);
    setReplacedOrInlineBlock(true); // pretend to be replaced
    ASSERT(isRenderListMarker());
}

RenderListMarker::~RenderListMarker()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderListMarker::willBeDestroyed()
{
    if (m_image)
        m_image->removeClient(*this);
    RenderBox::willBeDestroyed();
}

void RenderListMarker::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);
    if (oldStyle) {
        if (style().listStylePosition() != oldStyle->listStylePosition() || style().listStyleType() != oldStyle->listStyleType())
            setNeedsLayoutAndPrefWidthsRecalc();
        if (oldStyle->isDisplayInlineType() && !style().isDisplayInlineType()) {
            setNeedsLayoutAndPrefWidthsRecalc();
            if (m_inlineBoxWrapper)
                m_inlineBoxWrapper->dirtyLineBoxes();
            m_inlineBoxWrapper = nullptr;
        }
    }

    if (m_image != style().listStyleImage()) {
        if (m_image)
            m_image->removeClient(*this);
        m_image = style().listStyleImage();
        if (m_image)
            m_image->addClient(*this);
    }
}

std::unique_ptr<LegacyInlineElementBox> RenderListMarker::createInlineBox()
{
    auto box = RenderBox::createInlineBox();
    box->setBehavesLikeText(!isImage());
    return box;
}

bool RenderListMarker::isImage() const
{
    return m_image && !m_image->errorOccurred();
}

LayoutRect RenderListMarker::localSelectionRect()
{
    LegacyInlineBox* box = inlineBoxWrapper();
    if (!box)
        return LayoutRect(LayoutPoint(), size());
    const LegacyRootInlineBox& rootBox = m_inlineBoxWrapper->root();
    LayoutUnit newLogicalTop { rootBox.blockFlow().style().isFlippedBlocksWritingMode() ? m_inlineBoxWrapper->logicalBottom() - rootBox.selectionBottom() : rootBox.selectionTop() - m_inlineBoxWrapper->logicalTop() };
    if (rootBox.blockFlow().style().isHorizontalWritingMode())
        return LayoutRect(0_lu, newLogicalTop, width(), rootBox.selectionHeight());
    return LayoutRect(newLogicalTop, 0_lu, rootBox.selectionHeight(), height());
}

static String reversed(StringView string)
{
    auto length = string.length();
    if (length <= 1)
        return string.toString();
    UChar* characters;
    auto result = String::createUninitialized(length, characters);
    for (unsigned i = 0; i < length; ++i)
        *characters++ = string[length - i - 1];
    return result;
}

struct RenderListMarker::TextRunWithUnderlyingString {
    TextRun textRun;
    String underlyingString;
    operator const TextRun&() const { return textRun; }
};

auto RenderListMarker::textRun() const -> TextRunWithUnderlyingString
{
    ASSERT(!m_textWithSuffix.isEmpty());

    // Since the bidi algorithm doesn't run on this text, we instead reorder the characters here.
    // We use u_charDirection to figure out if the marker text is RTL and assume the suffix matches the surrounding direction.
    String textForRun;
    if (m_textIsLeftToRightDirection) {
        if (style().isLeftToRightDirection())
            textForRun = m_textWithSuffix;
        else {
            if (style().listStyleType().isDisclosureClosed())
                textForRun = { &blackLeftPointingSmallTriangle, 1 };
            else
                textForRun = makeString(reversed(StringView(m_textWithSuffix).substring(m_textWithoutSuffixLength)), m_textWithSuffix.left(m_textWithoutSuffixLength));
        }
    } else {
        if (!style().isLeftToRightDirection())
            textForRun = reversed(m_textWithSuffix);
        else
            textForRun = makeString(reversed(StringView(m_textWithSuffix).left(m_textWithoutSuffixLength)), m_textWithSuffix.substring(m_textWithoutSuffixLength));
    }
    auto textRun = RenderBlock::constructTextRun(textForRun, style());
    return { WTFMove(textRun), WTFMove(textForRun) };
}

void RenderListMarker::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::Accessibility)
        return;

    if (style().visibility() != Visibility::Visible)
        return;

    LayoutPoint boxOrigin(paintOffset + location());
    LayoutRect overflowRect(visualOverflowRect());
    overflowRect.moveBy(boxOrigin);
    if (!paintInfo.rect.intersects(overflowRect))
        return;

    LayoutRect box(boxOrigin, size());

    auto markerRect = relativeMarkerRect();
    markerRect.moveBy(boxOrigin);

    if (paintInfo.phase == PaintPhase::Accessibility) {
        paintInfo.accessibilityRegionContext()->takeBounds(*this, markerRect);
        return;
    }

    if (markerRect.isEmpty())
        return;

    GraphicsContext& context = paintInfo.context();

    if (isImage()) {
        if (RefPtr<Image> markerImage = m_image->image(this, markerRect.size()))
            context.drawImage(*markerImage, markerRect);
        if (selectionState() != HighlightState::None) {
            LayoutRect selRect = localSelectionRect();
            selRect.moveBy(boxOrigin);
            context.fillRect(snappedIntRect(selRect), m_listItem->selectionBackgroundColor());
        }
        return;
    }

    if (selectionState() != HighlightState::None) {
        LayoutRect selRect = localSelectionRect();
        selRect.moveBy(boxOrigin);
        context.fillRect(snappedIntRect(selRect), m_listItem->selectionBackgroundColor());
    }

    auto color = style().visitedDependentColorWithColorFilter(CSSPropertyColor);
    context.setStrokeColor(color);
    context.setStrokeStyle(StrokeStyle::SolidStroke);
    context.setStrokeThickness(1.0f);
    context.setFillColor(color);

    auto listStyleType = style().listStyleType();
    if (listStyleType.isDisc()) {
        context.fillEllipse(markerRect);
        return;
    }
    if (listStyleType.isCircle()) {
        context.strokeEllipse(markerRect);
        return;
    }
    if (listStyleType.isSquare()) {
        context.fillRect(markerRect);
        return;
    }
    if (m_textWithSuffix.isEmpty())
        return;

    GraphicsContextStateSaver stateSaver(context, false);
    if (!style().isHorizontalWritingMode()) {
        markerRect.moveBy(-boxOrigin);
        markerRect = markerRect.transposedRect();
        markerRect.moveBy(FloatPoint(box.x(), box.y() - logicalHeight()));
        stateSaver.save();
        context.translate(markerRect.x(), markerRect.maxY());
        context.rotate(static_cast<float>(deg2rad(90.)));
        context.translate(-markerRect.x(), -markerRect.maxY());
    }

    FloatPoint textOrigin = FloatPoint(markerRect.x(), markerRect.y() + style().metricsOfPrimaryFont().ascent());
    textOrigin = roundPointToDevicePixels(LayoutPoint(textOrigin), document().deviceScaleFactor(), style().isLeftToRightDirection());
    context.drawText(style().fontCascade(), textRun(), textOrigin);
}

RenderBox* RenderListMarker::parentBox(RenderBox& box)
{
    ASSERT(m_listItem);
    CheckedPtr multiColumnFlow = dynamicDowncast<RenderMultiColumnFlow>(m_listItem->enclosingFragmentedFlow());
    if (!multiColumnFlow)
        return box.parentBox();
    auto* placeholder = multiColumnFlow->findColumnSpannerPlaceholder(&box);
    return placeholder ? placeholder->parentBox() : box.parentBox();
};

void RenderListMarker::addOverflowFromListMarker()
{
    ASSERT(m_listItem);
    if (!parent() || !parent()->isRenderBox())
        return;

    if (isInside() || !inlineBoxWrapper())
        return;

    LayoutUnit markerOldLogicalLeft = logicalLeft();
    LayoutUnit blockOffset;
    LayoutUnit lineOffset;
    for (auto* ancestor = parentBox(*this); ancestor && ancestor != m_listItem.get(); ancestor = parentBox(*ancestor)) {
        blockOffset += ancestor->logicalTop();
        lineOffset += ancestor->logicalLeft();
    }

    bool adjustOverflow = false;
    LayoutUnit markerLogicalLeft;
    bool hitSelfPaintingLayer = false;

    const LegacyRootInlineBox& rootBox = inlineBoxWrapper()->root();
    LayoutUnit lineTop = rootBox.lineTop();
    LayoutUnit lineBottom = rootBox.lineBottom();

    // FIXME: Need to account for relative positioning in the layout overflow.
    if (m_listItem->style().isLeftToRightDirection()) {
        markerLogicalLeft = m_lineOffsetForListItem - lineOffset - m_listItem->paddingStart() - m_listItem->borderStart() + marginStart();
        inlineBoxWrapper()->adjustLineDirectionPosition(markerLogicalLeft - markerOldLogicalLeft);
        for (auto* box = inlineBoxWrapper()->parent(); box; box = box->parent()) {
            auto newLogicalVisualOverflowRect = box->logicalVisualOverflowRect(lineTop, lineBottom);
            auto newLogicalLayoutOverflowRect = box->logicalLayoutOverflowRect(lineTop, lineBottom);
            if (markerLogicalLeft < newLogicalVisualOverflowRect.x() && !hitSelfPaintingLayer) {
                newLogicalVisualOverflowRect.setWidth(newLogicalVisualOverflowRect.maxX() - markerLogicalLeft);
                newLogicalVisualOverflowRect.setX(markerLogicalLeft);
                if (box == &rootBox)
                    adjustOverflow = true;
            }
            if (markerLogicalLeft < newLogicalLayoutOverflowRect.x()) {
                newLogicalLayoutOverflowRect.setWidth(newLogicalLayoutOverflowRect.maxX() - markerLogicalLeft);
                newLogicalLayoutOverflowRect.setX(markerLogicalLeft);
                if (box == &rootBox)
                    adjustOverflow = true;
            }
            box->setOverflowFromLogicalRects(newLogicalLayoutOverflowRect, newLogicalVisualOverflowRect, lineTop, lineBottom);
            if (box->renderer().hasSelfPaintingLayer())
                hitSelfPaintingLayer = true;
        }
    } else {
        markerLogicalLeft = m_lineOffsetForListItem - lineOffset + m_listItem->paddingStart() + m_listItem->borderStart() + marginEnd();
        inlineBoxWrapper()->adjustLineDirectionPosition(markerLogicalLeft - markerOldLogicalLeft);
        for (auto* box = inlineBoxWrapper()->parent(); box; box = box->parent()) {
            auto newLogicalVisualOverflowRect = box->logicalVisualOverflowRect(lineTop, lineBottom);
            auto newLogicalLayoutOverflowRect = box->logicalLayoutOverflowRect(lineTop, lineBottom);
            if (markerLogicalLeft + logicalWidth() > newLogicalVisualOverflowRect.maxX() && !hitSelfPaintingLayer) {
                newLogicalVisualOverflowRect.setWidth(markerLogicalLeft + logicalWidth() - newLogicalVisualOverflowRect.x());
                if (box == &rootBox)
                    adjustOverflow = true;
            }
            if (markerLogicalLeft + logicalWidth() > newLogicalLayoutOverflowRect.maxX()) {
                newLogicalLayoutOverflowRect.setWidth(markerLogicalLeft + logicalWidth() - newLogicalLayoutOverflowRect.x());
                if (box == &rootBox)
                    adjustOverflow = true;
            }
            box->setOverflowFromLogicalRects(newLogicalLayoutOverflowRect, newLogicalVisualOverflowRect, lineTop, lineBottom);
            if (box->renderer().hasSelfPaintingLayer())
                hitSelfPaintingLayer = true;
        }
    }

    if (adjustOverflow) {
        LayoutRect markerRect(markerLogicalLeft + lineOffset, blockOffset, width(), height());
        if (!m_listItem->style().isHorizontalWritingMode())
            markerRect = markerRect.transposedRect();
        RenderBox* markerAncestor = this;
        bool propagateVisualOverflow = true;
        bool propagateLayoutOverflow = true;
        do {
            markerAncestor = parentBox(*markerAncestor);
            if (markerAncestor->hasNonVisibleOverflow())
                propagateVisualOverflow = false;
            if (CheckedPtr markerAncestorBlock = dynamicDowncast<RenderBlock>(*markerAncestor)) {
                if (propagateVisualOverflow)
                    markerAncestorBlock->addVisualOverflow(markerRect);
                if (propagateLayoutOverflow)
                    markerAncestorBlock->addLayoutOverflow(markerRect);
            }
            if (markerAncestor->hasNonVisibleOverflow())
                propagateLayoutOverflow = false;
            if (markerAncestor->hasSelfPaintingLayer())
                propagateVisualOverflow = false;
            markerRect.moveBy(-markerAncestor->location());
        } while (markerAncestor != m_listItem.get() && propagateVisualOverflow && propagateLayoutOverflow);
    }
}

void RenderListMarker::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    LayoutUnit blockOffset;
    for (auto* ancestor = parentBox(*this); ancestor && ancestor != m_listItem.get(); ancestor = parentBox(*ancestor))
        blockOffset += ancestor->logicalTop();

    m_lineLogicalOffsetForListItem = m_listItem->logicalLeftOffsetForLine(blockOffset, DoNotIndentText, 0_lu);
    m_lineOffsetForListItem = style().isLeftToRightDirection() ? m_lineLogicalOffsetForListItem : m_listItem->logicalRightOffsetForLine(blockOffset, DoNotIndentText, 0_lu);

    if (isImage()) {
        updateMarginsAndContent();
        setWidth(m_image->imageSize(this, style().effectiveZoom()).width());
        setHeight(m_image->imageSize(this, style().effectiveZoom()).height());
    } else {
        setLogicalWidth(minPreferredLogicalWidth());
        setLogicalHeight(style().metricsOfPrimaryFont().height());
    }

    setMarginStart(0);
    setMarginEnd(0);

    Length startMargin = style().marginStart();
    Length endMargin = style().marginEnd();
    if (startMargin.isFixed())
        setMarginStart(LayoutUnit(startMargin.value()));
    if (endMargin.isFixed())
        setMarginEnd(LayoutUnit(endMargin.value()));

    clearNeedsLayout();
}

void RenderListMarker::imageChanged(WrappedImagePtr o, const IntRect* rect)
{
    if (m_image && o == m_image->data()) {
        if (width() != m_image->imageSize(this, style().effectiveZoom()).width() || height() != m_image->imageSize(this, style().effectiveZoom()).height() || m_image->errorOccurred())
            setNeedsLayoutAndPrefWidthsRecalc();
        else
            repaint();
    }
    RenderBox::imageChanged(o, rect);
}

void RenderListMarker::updateMarginsAndContent()
{
    // FIXME: It's messy to use the preferredLogicalWidths dirty bit for this optimization, also unclear if this is premature optimization.
    if (preferredLogicalWidthsDirty())
        updateContent();
    updateMargins();
}

void RenderListMarker::updateContent()
{
    if (isImage()) {
        // FIXME: This is a somewhat arbitrary width.  Generated images for markers really won't become particularly useful
        // until we support the CSS3 marker pseudoclass to allow control over the width and height of the marker box.
        LayoutUnit bulletWidth = style().metricsOfPrimaryFont().ascent() / 2_lu;
        LayoutSize defaultBulletSize(bulletWidth, bulletWidth);
        LayoutSize imageSize = calculateImageIntrinsicDimensions(m_image.get(), defaultBulletSize, DoNotScaleByEffectiveZoom);
        m_image->setContainerContextForRenderer(*this, imageSize, style().effectiveZoom());
        m_textWithSuffix = emptyString();
        m_textWithoutSuffixLength = 0;
        m_textIsLeftToRightDirection = true;
        return;
    }

    auto styleType = style().listStyleType();
    switch (styleType.type) {
    case ListStyleType::Type::String:
        m_textWithSuffix = styleType.identifier;
        m_textWithoutSuffixLength = m_textWithSuffix.length();
        // FIXME: Depending on the string value, we may need the real bidi algorithm. (rdar://106139180)
        m_textIsLeftToRightDirection = u_charDirection(m_textWithSuffix[0]) != U_RIGHT_TO_LEFT;
        break;
    case ListStyleType::Type::CounterStyle: {
        auto counter = counterStyle();
        ASSERT(counter);
        auto text = makeString(counter->prefix().text, counter->text(m_listItem->value()));
        m_textWithSuffix = makeString(text, counter->suffix().text);
        m_textWithoutSuffixLength = text.length();
        m_textIsLeftToRightDirection = u_charDirection(text[0]) != U_RIGHT_TO_LEFT;
        break;
    }
    case ListStyleType::Type::None:
        m_textWithSuffix = " "_s;
        m_textWithoutSuffixLength = 0;
        m_textIsLeftToRightDirection = true;
        break;
    }
}

void RenderListMarker::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());
    updateContent();

    if (isImage()) {
        LayoutSize imageSize = LayoutSize(m_image->imageSize(this, style().effectiveZoom()));
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = style().isHorizontalWritingMode() ? imageSize.width() : imageSize.height();
        setPreferredLogicalWidthsDirty(false);
        updateMargins();
        return;
    }

    const FontCascade& font = style().fontCascade();

    LayoutUnit logicalWidth;
    if (widthUsesMetricsOfPrimaryFont())
        logicalWidth = (font.metricsOfPrimaryFont().ascent() * 2 / 3 + 1) / 2 + 2;
    else if (!m_textWithSuffix.isEmpty())
            logicalWidth = font.width(textRun());

    m_minPreferredLogicalWidth = logicalWidth;
    m_maxPreferredLogicalWidth = logicalWidth;

    setPreferredLogicalWidthsDirty(false);

    updateMargins();
}

void RenderListMarker::updateMargins()
{
    const FontMetrics& fontMetrics = style().metricsOfPrimaryFont();

    LayoutUnit marginStart;
    LayoutUnit marginEnd;

    if (isInside()) {
        if (isImage())
            marginEnd = cMarkerPadding;
        else if (widthUsesMetricsOfPrimaryFont()) {
                marginStart = -1;
                marginEnd = fontMetrics.ascent() - minPreferredLogicalWidth() + 1;
        }
    } else if (isImage()) {
        marginStart = -minPreferredLogicalWidth() - cMarkerPadding;
        marginEnd = cMarkerPadding;
    } else {
        int offset = fontMetrics.ascent() * 2 / 3;
        if (widthUsesMetricsOfPrimaryFont()) {
            marginStart = -offset - cMarkerPadding - 1;
            marginEnd = offset + cMarkerPadding + 1 - minPreferredLogicalWidth();
        } else if (style().listStyleType().type == ListStyleType::Type::String) {
            if (!m_textWithSuffix.isEmpty())
                marginStart = -minPreferredLogicalWidth();
        } else {
            if (!m_textWithSuffix.isEmpty()) {
                marginStart = -minPreferredLogicalWidth() - offset / 2;
                marginEnd = offset / 2;
            }
        }
    }

    mutableStyle().setMarginStart(Length(marginStart, LengthType::Fixed));
    mutableStyle().setMarginEnd(Length(marginEnd, LengthType::Fixed));
}

LayoutUnit RenderListMarker::lineHeight(bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    if (!isImage())
        return m_listItem->lineHeight(firstLine, direction, PositionOfInteriorLineBoxes);
    return RenderBox::lineHeight(firstLine, direction, linePositionMode);
}

LayoutUnit RenderListMarker::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    if (!isImage())
        return m_listItem->baselinePosition(baselineType, firstLine, direction, PositionOfInteriorLineBoxes);
    return RenderBox::baselinePosition(baselineType, firstLine, direction, linePositionMode);
}

bool RenderListMarker::isInside() const
{
    return m_listItem->notInList() || style().listStylePosition() == ListStylePosition::Inside;
}

const RenderListItem* RenderListMarker::listItem() const
{
    return m_listItem.get();
}

FloatRect RenderListMarker::relativeMarkerRect()
{
    if (isImage())
        return FloatRect(0, 0, m_image->imageSize(this, style().effectiveZoom()).width(), m_image->imageSize(this, style().effectiveZoom()).height());

    FloatRect relativeRect;
    if (widthUsesMetricsOfPrimaryFont()) {
        // FIXME: Are these particular rounding rules necessary?
        const FontMetrics& fontMetrics = style().metricsOfPrimaryFont();
        int ascent = fontMetrics.ascent();
        int bulletWidth = (ascent * 2 / 3 + 1) / 2;
        relativeRect = FloatRect(1, 3 * (ascent - ascent * 2 / 3) / 2, bulletWidth, bulletWidth);
    } else {
        if (m_textWithSuffix.isEmpty())
            return FloatRect();
        auto& font = style().fontCascade();
        relativeRect = FloatRect(0, 0, font.width(textRun()), font.metricsOfPrimaryFont().height());
    }

    if (!style().isHorizontalWritingMode()) {
        relativeRect = relativeRect.transposedRect();
        relativeRect.setX(width() - relativeRect.x() - relativeRect.width());
    }

    return relativeRect;
}

LayoutRect RenderListMarker::selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent)
{
    ASSERT(!needsLayout());

    if (selectionState() == HighlightState::None || !inlineBoxWrapper())
        return LayoutRect();

    LegacyRootInlineBox& rootBox = inlineBoxWrapper()->root();
    LayoutRect rect(0_lu, rootBox.selectionTop() - y(), width(), rootBox.selectionHeight());
            
    if (clipToVisibleContent)
        return computeRectForRepaint(rect, repaintContainer);
    return localToContainerQuad(FloatRect(rect), repaintContainer).enclosingBoundingBox();
}

StringView RenderListMarker::textWithoutSuffix() const
{
    return StringView { m_textWithSuffix }.left(m_textWithoutSuffixLength);
}

RefPtr<CSSCounterStyle> RenderListMarker::counterStyle() const
{
    return document().counterStyleRegistry().resolvedCounterStyle(style().listStyleType());
}

bool RenderListMarker::widthUsesMetricsOfPrimaryFont() const
{
    auto listType = style().listStyleType();
    return listType.isCircle() || listType.isDisc() || listType.isSquare();
}

} // namespace WebCore
