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

    typedef unsigned HitTestRequestType;

    HitTestRequest(HitTestRequestType requestType = ReadOnly | Active | DisallowUserAgentShadowContent)
        : m_requestType(requestType)
    {
        ASSERT(!(requestType & IncludeAllElementsUnderPoint) || (requestType & CollectMultipleElements));
    }

    bool readOnly() const { return m_requestType & ReadOnly; }
    bool active() const { return m_requestType & Active; }
    bool move() const { return m_requestType & Move; }
    bool release() const { return m_requestType & Release; }
    bool ignoreClipping() const { return m_requestType & IgnoreClipping; }
    bool svgClipContent() const { return m_requestType & SVGClipContent; }
    bool touchEvent() const { return m_requestType & TouchEvent; }
    bool mouseEvent() const { return !touchEvent(); }
    bool disallowsUserAgentShadowContent() const { return m_requestType & DisallowUserAgentShadowContent; }
    bool allowsFrameScrollbars() const { return m_requestType & AllowFrameScrollbars; }
    bool allowsChildFrameContent() const { return m_requestType & AllowChildFrameContent; }
    bool allowsVisibleChildFrameContent() const { return m_requestType & AllowVisibleChildFrameContentOnly; }
    bool isChildFrameHitTest() const { return m_requestType & ChildFrameHitTest; }
    bool resultIsElementList() const { return m_requestType & CollectMultipleElements; }
    bool includesAllElementsUnderPoint() const { return m_requestType & IncludeAllElementsUnderPoint; }

    // Convenience functions
    bool touchMove() const { return move() && touchEvent(); }
    bool touchRelease() const { return release() && touchEvent(); }

    HitTestRequestType type() const { return m_requestType; }

private:
    HitTestRequestType m_requestType;
};

} // namespace WebCore
