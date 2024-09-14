/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2016 Google Inc. All rights reserved.
 * Copyright (C) 2023, 2024 Igalia S.L.
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
 */

#include "config.h"
#include "SVGInlineTextBox.h"

#include "FloatConversion.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LegacyInlineFlowBox.h"
#include "LegacyRenderSVGResourceSolidColor.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "PointerEventsHitRules.h"
#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderSVGText.h"
#include "RenderStyleInlines.h"
#include "RenderView.h"
#include "SVGInlineTextBoxInlines.h"
#include "SVGPaintServerHandling.h"
#include "SVGRenderStyle.h"
#include "SVGRenderingContext.h"
#include "SVGResourcesCache.h"
#include "SVGRootInlineBox.h"
#include "SVGTextBoxPainter.h"
#include "TextBoxSelectableRange.h"
#include "TextPainter.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGInlineTextBox);

struct ExpectedSVGInlineTextBoxSize : public LegacyInlineTextBox {
    float float1;
    uint32_t bitfields : 1;
    Vector<SVGTextFragment> vector;
};

static_assert(sizeof(SVGInlineTextBox) == sizeof(ExpectedSVGInlineTextBoxSize), "SVGInlineTextBox is not of expected size");

SVGInlineTextBox::SVGInlineTextBox(RenderSVGInlineText& renderer)
    : LegacyInlineTextBox(renderer)
    , m_startsNewTextChunk(false)
{
}

void SVGInlineTextBox::dirtyOwnLineBoxes()
{
    LegacyInlineTextBox::dirtyLineBoxes();

    m_textFragments = { };
}

void SVGInlineTextBox::dirtyLineBoxes()
{
    dirtyOwnLineBoxes();

    // And clear any following text fragments as the text on which they
    // depend may now no longer exist, or glyph positions may be wrong
    for (auto* nextBox = nextTextBox(); nextBox; nextBox = nextBox->nextTextBox())
        nextBox->dirtyOwnLineBoxes();
}

int SVGInlineTextBox::offsetForPositionInFragment(const SVGTextFragment& fragment, float position) const
{
    float scalingFactor = renderer().scalingFactor();
    ASSERT(scalingFactor);

    TextRun textRun = constructTextRun(renderer().style(), fragment);

    // Eventually handle lengthAdjust="spacingAndGlyphs".
    // FIXME: Handle vertical text.
    AffineTransform fragmentTransform;
    fragment.buildFragmentTransform(fragmentTransform);
    if (!fragmentTransform.isIdentity())
        textRun.setHorizontalGlyphStretch(narrowPrecisionToFloat(fragmentTransform.xScale()));

    const bool includePartialGlyphs = true;
    return fragment.characterOffset - start() + renderer().scaledFont().offsetForPosition(textRun, position * scalingFactor, includePartialGlyphs);
}

FloatRect SVGInlineTextBox::selectionRectForTextFragment(const SVGTextFragment& fragment, unsigned startPosition, unsigned endPosition, const RenderStyle& style) const
{
    ASSERT_WITH_SECURITY_IMPLICATION(startPosition < endPosition);

    float scalingFactor = renderer().scalingFactor();
    ASSERT(scalingFactor);

    const FontCascade& scaledFont = renderer().scaledFont();
    const FontMetrics& scaledFontMetrics = scaledFont.metricsOfPrimaryFont();
    FloatPoint textOrigin(fragment.x, fragment.y);
    if (scalingFactor != 1)
        textOrigin.scale(scalingFactor);

    textOrigin.move(0, -scaledFontMetrics.ascent());

    LayoutRect selectionRect { textOrigin, LayoutSize(0, LayoutUnit(fragment.height * scalingFactor)) };
    TextRun run = constructTextRun(style, fragment);
    scaledFont.adjustSelectionRectForText(renderer().canUseSimplifiedTextMeasuring().value_or(false), run, selectionRect, startPosition, endPosition);
    FloatRect snappedSelectionRect = snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), run.ltr());
    if (scalingFactor == 1)
        return snappedSelectionRect;

    snappedSelectionRect.scale(1 / scalingFactor);
    return snappedSelectionRect;
}

LayoutRect SVGInlineTextBox::localSelectionRect(unsigned start, unsigned end) const
{
    auto [clampedStart, clampedEnd] = selectableRange().clamp(start, end);

    if (clampedStart >= clampedEnd)
        return LayoutRect();

    auto& style = renderer().style();

    AffineTransform fragmentTransform;
    FloatRect selectionRect;
    unsigned fragmentStartPosition = 0;
    unsigned fragmentEndPosition = 0;

    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        const SVGTextFragment& fragment = m_textFragments.at(i);

        fragmentStartPosition = clampedStart;
        fragmentEndPosition = clampedEnd;
        if (!mapStartEndPositionsIntoFragmentCoordinates(fragment, fragmentStartPosition, fragmentEndPosition))
            continue;

        FloatRect fragmentRect = selectionRectForTextFragment(fragment, fragmentStartPosition, fragmentEndPosition, style);
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity())
            fragmentRect = fragmentTransform.mapRect(fragmentRect);

        selectionRect.unite(fragmentRect);
    }

    return enclosingIntRect(selectionRect);
}

void SVGInlineTextBox::paintSelectionBackground(PaintInfo& paintInfo)
{
    auto painter = LegacySVGTextBoxPainter { *this, paintInfo, { } };
    painter.paintSelectionBackground();
}

void SVGInlineTextBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit, LayoutUnit)
{
    auto painter = LegacySVGTextBoxPainter { *this, paintInfo, paintOffset };
    painter.paint();
}

TextRun SVGInlineTextBox::constructTextRun(const RenderStyle& style, const SVGTextFragment& fragment) const
{
    TextRun run(StringView(renderer().text()).substring(fragment.characterOffset, fragment.length),
        0, /* xPos, only relevant with allowTabs=true */
        0, /* padding, only relevant for justified text, not relevant for SVG */
        ExpansionBehavior::allowRightOnly(),
        direction(),
        style.rtlOrdering() == Order::Visual /* directionalOverride */);

    // We handle letter & word spacing ourselves.
    run.disableSpacing();
    return run;
}

bool SVGInlineTextBox::mapStartEndPositionsIntoFragmentCoordinates(const SVGTextFragment& fragment, unsigned& startPosition, unsigned& endPosition) const
{
    unsigned startFragment = fragment.characterOffset - start();
    unsigned endFragment = startFragment + fragment.length;

    // Find intersection between the intervals: [startFragment..endFragment) and [startPosition..endPosition)
    startPosition = std::max(startFragment, startPosition);
    endPosition = std::min(endFragment, endPosition);

    if (startPosition >= endPosition)
        return false;

    startPosition -= startFragment;
    endPosition -= startFragment;

    return true;
}

FloatRect SVGInlineTextBox::calculateBoundaries() const
{
    FloatRect textRect;

    float scalingFactor = renderer().scalingFactor();
    ASSERT(scalingFactor);

    float baseline = renderer().scaledFont().metricsOfPrimaryFont().ascent() / scalingFactor;

    AffineTransform fragmentTransform;
    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        const SVGTextFragment& fragment = m_textFragments.at(i);
        FloatRect fragmentRect(fragment.x, fragment.y - baseline, fragment.width, fragment.height);
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity())
            fragmentRect = fragmentTransform.mapRect(fragmentRect);

        textRect.unite(fragmentRect);
    }

    return textRect;
}

bool SVGInlineTextBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit, LayoutUnit, HitTestAction)
{
    // FIXME: integrate with LegacyInlineTextBox::nodeAtPoint better.
    ASSERT(!isLineBreak());

    PointerEventsHitRules hitRules(PointerEventsHitRules::HitTestingTargetType::SVGText, request, renderer().usedPointerEvents());
    if (isVisibleToHitTesting(renderer().style(), request) || !hitRules.requireVisible) {
        if ((hitRules.canHitStroke && (renderer().style().svgStyle().hasStroke() || !hitRules.requireStroke))
            || (hitRules.canHitFill && (renderer().style().svgStyle().hasFill() || !hitRules.requireFill))) {
            FloatRect rect(topLeft(), FloatSize(logicalWidth(), logicalHeight()));
            rect.moveBy(accumulatedOffset);
            if (locationInContainer.intersects(rect)) {

                float scalingFactor = renderer().scalingFactor();
                ASSERT(scalingFactor);
                
                float baseline = renderer().scaledFont().metricsOfPrimaryFont().ascent() / scalingFactor;

                AffineTransform fragmentTransform;
                for (auto& fragment : m_textFragments) {
                    FloatQuad fragmentQuad(FloatRect(fragment.x, fragment.y - baseline, fragment.width, fragment.height));
                    fragment.buildFragmentTransform(fragmentTransform);
                    if (!fragmentTransform.isIdentity())
                        fragmentQuad = fragmentTransform.mapQuad(fragmentQuad);
                    
                    if (fragmentQuad.containsPoint(locationInContainer.point())) {
                        renderer().updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
                        if (result.addNodeToListBasedTestResult(renderer().protectedNodeForHitTest().get(), request, locationInContainer, rect) == HitTestProgress::Stop)
                            return true;
                    }
                }
             }
        }
    }
    return false;
}

} // namespace WebCore
