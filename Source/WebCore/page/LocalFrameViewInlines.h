/*
   Copyright (C) 1997 Martin Jones (mjones@kde.org)
             (C) 1998 Waldo Bastian (bastian@kde.org)
             (C) 1998, 1999 Torben Weis (weis@kde.org)
             (C) 1999 Lars Knoll (knoll@kde.org)
             (C) 1999 Antti Koivisto (koivisto@kde.org)
   Copyright (C) 2004-2025 Apple Inc. All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#pragma once

#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Page.h"

namespace WebCore {

inline void LocalFrameView::incrementVisuallyNonEmptyPixelCount(const IntSize& size)
{
    if (m_visuallyNonEmptyPixelCount > visualPixelThreshold)
        return;

    auto area = size.area<RecordOverflow>() + m_visuallyNonEmptyPixelCount;
    if (area.hasOverflowed()) [[unlikely]]
        m_visuallyNonEmptyPixelCount = std::numeric_limits<decltype(m_visuallyNonEmptyPixelCount)>::max();
    else
        m_visuallyNonEmptyPixelCount = area;
}

inline void LocalFrameView::incrementVisuallyNonEmptyCharacterCount(const String& inlineText)
{
    if (m_visuallyNonEmptyCharacterCount > visualCharacterThreshold && m_hasReachedSignificantRenderedTextThreshold)
        return;

    incrementVisuallyNonEmptyCharacterCountSlowCase(inlineText);
}

inline bool LocalFrameView::hasEnoughContentForVisualMilestones() const
{
    if (!m_frame->page())
        return false;
    return isVisuallyNonEmpty() && hasContentfulDescendants() && (!m_frame->page()->requestedLayoutMilestones().contains(LayoutMilestone::DidRenderSignificantAmountOfText) || m_renderedSignificantAmountOfText);
}

} // namespace WebCore
