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
#include "SVGRenderStyle.h"
#include "SVGRenderingContext.h"
#include "SVGResourcesCache.h"
#include "SVGRootInlineBox.h"
#include "SVGTextFragment.h"
#include "TextBoxSelectableRange.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGInlineTextBox);

SVGInlineTextBox::SVGInlineTextBox(RenderSVGInlineText& renderer)
    : LegacyInlineTextBox(renderer)
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

void SVGInlineTextBox::setTextFragments(Vector<SVGTextFragment>&& fragments)
{
    m_textFragments = WTFMove(fragments);
}

} // namespace WebCore
