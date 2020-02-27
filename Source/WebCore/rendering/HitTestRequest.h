/*
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#pragma once

#include <wtf/Assertions.h>
#include <wtf/OptionSet.h>

namespace WebCore {

class HitTestRequest {
public:
    enum RequestType {
        ReadOnly = 1 << 1,
        Active = 1 << 2,
        Move = 1 << 3,
        Release = 1 << 4,
        IgnoreClipping = 1 << 5,
        SVGClipContent = 1 << 6,
        TouchEvent = 1 << 7,
        DisallowUserAgentShadowContent = 1 << 8,
        AllowFrameScrollbars = 1 << 9,
        AllowChildFrameContent = 1 << 10,
        AllowVisibleChildFrameContentOnly = 1 << 11,
        ChildFrameHitTest = 1 << 12,
        AccessibilityHitTest = 1 << 13,
        // Collect a list of nodes instead of just one. Used for elementsFromPoint and rect-based tests.
        CollectMultipleElements = 1 << 14,
        // When using list-based testing, continue hit testing even after a hit has been found.
        IncludeAllElementsUnderPoint = 1 << 15
    };

    HitTestRequest(OptionSet<RequestType> requestType = { ReadOnly, Active, DisallowUserAgentShadowContent })
        : m_requestType { requestType }
    {
        ASSERT_IMPLIES(requestType.contains(IncludeAllElementsUnderPoint), requestType.contains(CollectMultipleElements));
    }

    bool readOnly() const { return m_requestType.contains(ReadOnly); }
    bool active() const { return m_requestType.contains(Active); }
    bool move() const { return m_requestType.contains(Move); }
    bool release() const { return m_requestType.contains(Release); }
    bool ignoreClipping() const { return m_requestType.contains(IgnoreClipping); }
    bool svgClipContent() const { return m_requestType.contains(SVGClipContent); }
    bool touchEvent() const { return m_requestType.contains(TouchEvent); }
    bool mouseEvent() const { return !touchEvent(); }
    bool disallowsUserAgentShadowContent() const { return m_requestType.contains(DisallowUserAgentShadowContent); }
    bool allowsFrameScrollbars() const { return m_requestType.contains(AllowFrameScrollbars); }
    bool allowsChildFrameContent() const { return m_requestType.contains(AllowChildFrameContent); }
    bool allowsVisibleChildFrameContent() const { return m_requestType.contains(AllowVisibleChildFrameContentOnly); }
    bool isChildFrameHitTest() const { return m_requestType.contains(ChildFrameHitTest); }
    bool resultIsElementList() const { return m_requestType.contains(CollectMultipleElements); }
    bool includesAllElementsUnderPoint() const { return m_requestType.contains(IncludeAllElementsUnderPoint); }

    // Convenience functions
    bool touchMove() const { return move() && touchEvent(); }
    bool touchRelease() const { return release() && touchEvent(); }

    OptionSet<RequestType> type() const { return m_requestType; }

private:
    OptionSet<RequestType> m_requestType;
};

} // namespace WebCore
