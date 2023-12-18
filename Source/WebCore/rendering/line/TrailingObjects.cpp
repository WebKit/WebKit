/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 ChangSeok Oh <shivamidow@gmail.com>
 * Copyright (C) 2013 Adobe Systems Inc. All right reserved.
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
#include "TrailingObjects.h"

#include "LegacyInlineIterator.h"
#include "RenderStyleInlines.h"

namespace WebCore {

void TrailingObjects::updateWhitespaceCollapsingTransitionsForTrailingBoxes(LineWhitespaceCollapsingState& lineWhitespaceCollapsingState, const LegacyInlineIterator& lBreak, CollapseFirstSpace collapseFirstSpace)
{
    if (!m_whitespace)
        return;

    // This object is either going to be part of the last transition, or it is going to be the actual endpoint.
    // In both cases we just decrease our pos by 1 level to exclude the space, allowing it to - in effect - collapse into the newline.
    if (lineWhitespaceCollapsingState.numTransitions() % 2) {
        // Find the trailing space object's transition.
        int trailingSpaceTransition = lineWhitespaceCollapsingState.numTransitions() - 1;
        for ( ; trailingSpaceTransition > 0 && lineWhitespaceCollapsingState.transitions()[trailingSpaceTransition].renderer() != m_whitespace; --trailingSpaceTransition) { }
        ASSERT(trailingSpaceTransition >= 0);
        if (collapseFirstSpace == CollapseFirstSpace::Yes)
            lineWhitespaceCollapsingState.decrementTransitionAt(trailingSpaceTransition);

        // Now make sure every single trailingPositionedBox following the trailingSpaceTransition properly stops and starts
        // ignoring spaces.
        size_t currentTransition = trailingSpaceTransition + 1;
        for (size_t i = 0; i < m_boxes.size(); ++i) {
            if (currentTransition >= lineWhitespaceCollapsingState.numTransitions()) {
                // We don't have a transition for this box yet.
                lineWhitespaceCollapsingState.ensureLineBoxInsideIgnoredSpaces(m_boxes[i]);
            } else {
                ASSERT(lineWhitespaceCollapsingState.transitions()[currentTransition].renderer() == &(m_boxes[i].get()));
                ASSERT(lineWhitespaceCollapsingState.transitions()[currentTransition + 1].renderer() == &(m_boxes[i].get()));
            }
            currentTransition += 2;
        }
    } else if (!lBreak.renderer()) {
        ASSERT(m_whitespace->isRenderText());
        ASSERT(collapseFirstSpace == CollapseFirstSpace::Yes);
        // Add a new end transition that stops right at the very end.
        unsigned length = m_whitespace->text().length();
        unsigned pos = length >= 2 ? length - 2 : UINT_MAX;
        LegacyInlineIterator endMid(0, m_whitespace, pos);
        lineWhitespaceCollapsingState.startIgnoringSpaces(endMid);
        for (size_t i = 0; i < m_boxes.size(); ++i)
            lineWhitespaceCollapsingState.ensureLineBoxInsideIgnoredSpaces(m_boxes[i]);
    }
}

}
