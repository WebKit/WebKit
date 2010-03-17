/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
#if ENABLE(PROGRESS_TAG)

#include "RenderProgress.h"

#include "HTMLProgressElement.h"
#include "RenderTheme.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

static const int defaultProgressWidth = 120;
static const int defaultProgressHeight = 20;

RenderProgress::RenderProgress(HTMLProgressElement* element)
    : RenderBlock(element)
    , m_position(-1)
{
    setSize(IntSize(defaultProgressWidth, defaultProgressHeight));
    setReplaced(true);
}

int RenderProgress::baselinePosition(bool, bool) const
{
    return height() + marginTop();
}

void RenderProgress::calcPrefWidths()
{
    m_minPrefWidth = 0;
    m_maxPrefWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPrefWidth = m_maxPrefWidth = calcContentBoxWidth(style()->width().value());
    else
        m_maxPrefWidth = defaultProgressWidth * style()->effectiveZoom();

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPrefWidth = max(m_maxPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minPrefWidth = max(m_minPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minPrefWidth = 0;
    else
        m_minPrefWidth = m_maxPrefWidth;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxPrefWidth = min(m_maxPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minPrefWidth = min(m_minPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }

    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight();
    m_minPrefWidth += toAdd;
    m_maxPrefWidth += toAdd;

    setPrefWidthsDirty(false);
}

void RenderProgress::layout()
{
    ASSERT(needsLayout());

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    calcWidth();
    calcHeight();

    m_overflow.clear();

    repainter.repaintAfterLayout();

    setNeedsLayout(false);
}

void RenderProgress::updateFromElement()
{
    HTMLProgressElement* element = static_cast<HTMLProgressElement*>(node());
    double position = element->position();
    int oldPosition = m_position;
    bool canOptimize = theme()->getNumberOfPixelsForProgressPosition(position, m_position);
    if (oldPosition == m_position)
        return;

    IntRect paintRect = contentBoxRect();
    if (canOptimize) {
        // FIXME: Need to handle indeterminate progress bar and RTL
        int adjustedPosition = (m_position >= 0) ? m_position : 0;
        int adjustedOldPosition = (oldPosition >= 0) ? oldPosition : 0;
        paintRect.setX(std::min(adjustedPosition, adjustedOldPosition));
        paintRect.setWidth(std::max(adjustedPosition, adjustedOldPosition) - paintRect.x());
    }
    repaintRectangle(paintRect);
}

} // namespace WebCore
#endif
