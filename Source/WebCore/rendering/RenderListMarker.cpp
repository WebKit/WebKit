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
#include "ListStyleType.h"
#include "RenderBoxInlines.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderView.h"
#include "StyleScope.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderListMarker);

RenderListMarker::RenderListMarker(RenderListItem& listItem, RenderStyle&& style)
    : RenderBox(Type::ListMarker, listItem.document(), WTFMove(style))
    , m_listItem(listItem)
{
    setInline(true);
    setReplacedOrInlineBlock(true); // pretend to be replaced
    ASSERT(isRenderListMarker());
}

// Do not add any code in below destructor. Add it to willBeDestroyed() instead.
RenderListMarker::~RenderListMarker() = default;

void RenderListMarker::willBeDestroyed()
{
    if (m_image)
        m_image->removeClient(*this);
    RenderBox::willBeDestroyed();
}

static StyleDifference adjustedStyleDifference(StyleDifference diff, const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    if (diff >= StyleDifference::Layout)
        return diff;
    // FIXME: Preferably we do this at RenderStyle::changeRequiresLayout but checking against pseudo(::marker) is not sufficient.
    auto needsLayout = oldStyle.listStylePosition() != newStyle.listStylePosition() || oldStyle.listStyleType() != newStyle.listStyleType() || oldStyle.isDisplayInlineType() != newStyle.isDisplayInlineType();
    return needsLayout ? StyleDifference::Layout : diff;
}

void RenderListMarker::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    RenderBox::styleWillChange(adjustedStyleDifference(diff, style(), newStyle), newStyle);
}

void RenderListMarker::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    if (oldStyle)
        diff = adjustedStyleDifference(diff, *oldStyle, style());
    RenderBox::styleDidChange(diff, oldStyle);

    if (m_image != style().listStyleImage()) {
        if (m_image)
            m_image->removeClient(*this);
        m_image = style().listStyleImage();
        if (m_image)
            m_image->addClient(*this);
    }
}

bool RenderListMarker::isImage() const
{
    return m_image && !m_image->errorOccurred();
}

LayoutRect RenderListMarker::localSelectionRect()
{
    return LayoutRect(LayoutPoint(), size());
}

static String reversed(StringView string)
{
    auto length = string.length();
    if (length <= 1)
        return string.toString();
    std::span<UChar> characters;
    auto result = String::createUninitialized(length, characters);
    for (unsigned i = 0; i < length; ++i)
        characters[i] = string[length - i - 1];
    return result;
}

struct TextRunWithUnderlyingString {
    TextRun textRun;
    String underlyingString;
    operator const TextRun&() const { return textRun; }
};

static auto textRunForContent(ListMarkerTextContent textContent, const RenderStyle& style) -> TextRunWithUnderlyingString
{
    ASSERT(!textContent.isEmpty());

    // Since the bidi algorithm doesn't run on this text, we instead reorder the characters here.
    // We use u_charDirection to figure out if the marker text is RTL and assume the suffix matches the surrounding direction.
    String textForRun;
    if (textContent.textDirection == TextDirection::LTR) {
        if (style.writingMode().isBidiLTR())
            textForRun = textContent.textWithSuffix;
        else
            textForRun = makeString(reversed(textContent.suffix()), textContent.textWithoutSuffix());
    } else {
        if (!style.writingMode().isBidiLTR())
            textForRun = reversed(textContent.textWithSuffix);
        else
            textForRun = makeString(reversed(textContent.textWithoutSuffix()), textContent.suffix());
    }
    auto textRun = RenderBlock::constructTextRun(textForRun, style);
    return { WTFMove(textRun), WTFMove(textForRun) };
}

void RenderListMarker::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::Accessibility)
        return;

    if (style().usedVisibility() != Visibility::Visible)
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
        if (RefPtr markerImage = m_image->image(this, markerRect.size()))
            context.drawImage(*markerImage, markerRect);
        if (selectionState() != HighlightState::None) {
            LayoutRect selectionRect = localSelectionRect();
            selectionRect.moveBy(boxOrigin);
            context.fillRect(snappedIntRect(selectionRect), m_listItem->selectionBackgroundColor());
        }
        return;
    }

    if (selectionState() != HighlightState::None) {
        LayoutRect selectionRect = localSelectionRect();
        selectionRect.moveBy(boxOrigin);
        context.fillRect(snappedIntRect(selectionRect), m_listItem->selectionBackgroundColor());
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
    if (m_textContent.isEmpty())
        return;

    GraphicsContextStateSaver stateSaver(context, false);
    if (!writingMode().isHorizontal()) {
        markerRect.moveBy(-boxOrigin);
        markerRect = markerRect.transposedRect();
        markerRect.moveBy(FloatPoint(box.x(), box.y() - logicalHeight()));
        stateSaver.save();
        context.translate(markerRect.x(), markerRect.maxY());
        context.rotate(static_cast<float>(deg2rad(90.)));
        context.translate(-markerRect.x(), -markerRect.maxY());
    }

    FloatPoint textOrigin = FloatPoint(markerRect.x(), markerRect.y() + style().metricsOfPrimaryFont().intAscent());
    textOrigin = roundPointToDevicePixels(LayoutPoint(textOrigin), document().deviceScaleFactor(), writingMode().isLogicalLeftInlineStart());
    context.drawText(style().fontCascade(), textRunForContent(m_textContent, style()), textOrigin);
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

void RenderListMarker::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    LayoutUnit blockOffset;
    for (auto* ancestor = parentBox(*this); ancestor && ancestor != m_listItem.get(); ancestor = parentBox(*ancestor))
        blockOffset += ancestor->logicalTop();

    m_lineLogicalOffsetForListItem = m_listItem->logicalLeftOffsetForLine(blockOffset, 0_lu);
    m_lineOffsetForListItem = writingMode().isLogicalLeftInlineStart() ? m_lineLogicalOffsetForListItem : m_listItem->logicalRightOffsetForLine(blockOffset, 0_lu);

    if (isImage()) {
        updateInlineMarginsAndContent();
        setWidth(m_image->imageSize(this, style().usedZoom()).width());
        setHeight(m_image->imageSize(this, style().usedZoom()).height());
    } else {
        setLogicalWidth(minPreferredLogicalWidth());
        setLogicalHeight(style().metricsOfPrimaryFont().intHeight());
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
    if (parent()) {
        if (m_image && o == m_image->data()) {
            if (width() != m_image->imageSize(this, style().usedZoom()).width() || height() != m_image->imageSize(this, style().usedZoom()).height() || m_image->errorOccurred())
                setNeedsLayoutAndPrefWidthsRecalc();
            else
                repaint();
        }
    }
    RenderBox::imageChanged(o, rect);
}

void RenderListMarker::updateInlineMarginsAndContent()
{
    // FIXME: It's messy to use the preferredLogicalWidths dirty bit for this optimization, also unclear if this is premature optimization.
    if (preferredLogicalWidthsDirty())
        updateContent();
    updateInlineMargins();
}

void RenderListMarker::updateContent()
{
    if (isImage()) {
        // FIXME: This is a somewhat arbitrary width.
        LayoutUnit bulletWidth = style().metricsOfPrimaryFont().intAscent() / 2_lu;
        LayoutSize defaultBulletSize(bulletWidth, bulletWidth);
        LayoutSize imageSize = calculateImageIntrinsicDimensions(m_image.get(), defaultBulletSize, ScaleByUsedZoom::No);
        m_image->setContainerContextForRenderer(*this, imageSize, style().usedZoom());
        m_textContent = {
            .textWithSuffix = emptyString(),
            .textWithoutSuffixLength = 0,
            .textDirection = TextDirection::LTR,
        };
        return;
    }

    auto contentTextDirection = [&](auto content) {
        if (!content.length())
            return TextDirection::LTR;
        // FIXME: Depending on the string value, we may need the real bidi algorithm. (rdar://106139180)
        // Also we may need to start checking for the entire content for directionality (and whether we need to check for additional
        // directionality characters like U_RIGHT_TO_LEFT_EMBEDDING).
        auto bidiCategory = u_charDirection(content[0]);
        if (bidiCategory != U_RIGHT_TO_LEFT && bidiCategory != U_RIGHT_TO_LEFT_ARABIC)
            return TextDirection::LTR;
        return TextDirection::RTL;
    };

    auto styleType = style().listStyleType();
    switch (styleType.type) {
    case ListStyleType::Type::String: {
        m_textContent = {
            .textWithSuffix = styleType.identifier,
            .textWithoutSuffixLength = styleType.identifier.length(),
            .textDirection = contentTextDirection(StringView { styleType.identifier }),
        };
        break;
    }
    case ListStyleType::Type::CounterStyle: {
        auto counter = counterStyle();
        ASSERT(counter);

        auto text = makeString(counter->prefix().text, counter->text(m_listItem->value(), writingMode()));
        m_textContent = {
            .textWithSuffix = makeString(text, counter->suffix().text),
            .textWithoutSuffixLength = text.length(),
            .textDirection = contentTextDirection(text),
        };
        break;
    }
    case ListStyleType::Type::None:
        m_textContent = {
            .textWithSuffix = " "_s,
            .textWithoutSuffixLength = 0,
            .textDirection = TextDirection::LTR,
        };
        break;
    }
}

void RenderListMarker::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());
    updateContent();

    if (isImage()) {
        LayoutSize imageSize = LayoutSize(m_image->imageSize(this, style().usedZoom()));
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = writingMode().isHorizontal() ? imageSize.width() : imageSize.height();
        setPreferredLogicalWidthsDirty(false);
        updateInlineMargins();
        return;
    }

    const FontCascade& font = style().fontCascade();

    LayoutUnit logicalWidth;
    if (widthUsesMetricsOfPrimaryFont())
        logicalWidth = (font.metricsOfPrimaryFont().intAscent() * 2 / 3 + 1) / 2 + 2;
    else if (!m_textContent.isEmpty())
        logicalWidth = font.width(textRunForContent(m_textContent, style()));

    m_minPreferredLogicalWidth = logicalWidth;
    m_maxPreferredLogicalWidth = logicalWidth;

    setPreferredLogicalWidthsDirty(false);

    updateInlineMargins();
}

void RenderListMarker::updateInlineMargins()
{
    constexpr int markerPadding = 7;
    const FontMetrics& fontMetrics = style().metricsOfPrimaryFont();

    auto marginsForInsideMarker = [&]() -> std::pair<LayoutUnit, LayoutUnit> {
        if (isImage())
            return { 0, markerPadding };

        if (widthUsesMetricsOfPrimaryFont())
            return { -1, fontMetrics.intAscent() - minPreferredLogicalWidth() + 1 };

        return { };
    };

    auto marginsForOutsideMarker = [&]() -> std::pair<LayoutUnit, LayoutUnit> {
        if (isImage())
            return { -minPreferredLogicalWidth() - markerPadding, markerPadding };

        int offset = fontMetrics.intAscent() * 2 / 3;
        if (widthUsesMetricsOfPrimaryFont())
            return { -offset - markerPadding - 1, offset + markerPadding + 1 - minPreferredLogicalWidth() };

        if (m_textContent.isEmpty())
            return { };

        if (style().listStyleType().type == ListStyleType::Type::String)
            return { -minPreferredLogicalWidth(), 0 };

        return { -minPreferredLogicalWidth() - offset / 2, offset / 2 };
    };

    auto [marginStart, marginEnd] = isInside() ? marginsForInsideMarker() : marginsForOutsideMarker();
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
    return style().listStylePosition() == ListStylePosition::Inside;
}

const RenderListItem* RenderListMarker::listItem() const
{
    return m_listItem.get();
}

FloatRect RenderListMarker::relativeMarkerRect()
{
    if (isImage())
        return FloatRect(0, 0, m_image->imageSize(this, style().usedZoom()).width(), m_image->imageSize(this, style().usedZoom()).height());

    FloatRect relativeRect;
    if (widthUsesMetricsOfPrimaryFont()) {
        // FIXME: Are these particular rounding rules necessary?
        const FontMetrics& fontMetrics = style().metricsOfPrimaryFont();
        int ascent = fontMetrics.intAscent();
        int bulletWidth = (ascent * 2 / 3 + 1) / 2;
        relativeRect = FloatRect(1, 3 * (ascent - ascent * 2 / 3) / 2, bulletWidth, bulletWidth);
    } else {
        if (m_textContent.isEmpty())
            return FloatRect();
        auto& font = style().fontCascade();
        relativeRect = FloatRect(0, 0, font.width(textRunForContent(m_textContent, style())), font.metricsOfPrimaryFont().intHeight());
    }

    if (!writingMode().isHorizontal()) {
        relativeRect = relativeRect.transposedRect();
        relativeRect.setX(width() - relativeRect.x() - relativeRect.width());
    }

    return relativeRect;
}

LayoutRect RenderListMarker::selectionRectForRepaint(const RenderLayerModelObject*, bool)
{
    ASSERT(!needsLayout());

    return LayoutRect();
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
