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
    enum class Type {
        ReadOnly = 1 << 0,
        Active = 1 << 1,
        Move = 1 << 2,
        Release = 1 << 3,
        IgnoreCSSPointerEventsProperty = 1 << 4,
        IgnoreClipping = 1 << 5,
        SVGClipContent = 1 << 6,
        TouchEvent = 1 << 7,
        DisallowUserAgentShadowContent = 1 << 8,
        DisallowUserAgentShadowContentExceptForImageOverlays = 1 << 9,
        AllowFrameScrollbars = 1 << 10,
        AllowChildFrameContent = 1 << 11,
        AllowVisibleChildFrameContentOnly = 1 << 12,
        ChildFrameHitTest = 1 << 13,
        AccessibilityHitTest = 1 << 14,
        // Collect a list of nodes instead of just one. Used for elementsFromPoint and rect-based tests.
        CollectMultipleElements = 1 << 15,
        // When using list-based testing, continue hit testing even after a hit has been found.
        IncludeAllElementsUnderPoint = 1 << 16,
    };

    HitTestRequest(OptionSet<Type> type = { Type::ReadOnly, Type::Active, Type::DisallowUserAgentShadowContent })
        : m_type { type }
    {
        ASSERT(!type.containsAll({ Type::DisallowUserAgentShadowContentExceptForImageOverlays, Type::DisallowUserAgentShadowContent }));
        ASSERT_IMPLIES(type.contains(Type::IncludeAllElementsUnderPoint), type.contains(Type::CollectMultipleElements));
    }

    bool readOnly() const { return m_type.contains(Type::ReadOnly); }
    bool active() const { return m_type.contains(Type::Active); }
    bool move() const { return m_type.contains(Type::Move); }
    bool release() const { return m_type.contains(Type::Release); }
    bool ignoreCSSPointerEventsProperty() const { return m_type.contains(Type::IgnoreCSSPointerEventsProperty); }
    bool ignoreClipping() const { return m_type.contains(Type::IgnoreClipping); }
    bool svgClipContent() const { return m_type.contains(Type::SVGClipContent); }
    bool touchEvent() const { return m_type.contains(Type::TouchEvent); }
    bool mouseEvent() const { return !touchEvent(); }
    bool disallowsUserAgentShadowContent() const { return m_type.contains(Type::DisallowUserAgentShadowContent); }
    bool disallowsUserAgentShadowContentExceptForImageOverlays() const { return m_type.contains(Type::DisallowUserAgentShadowContentExceptForImageOverlays); }
    bool allowsFrameScrollbars() const { return m_type.contains(Type::AllowFrameScrollbars); }
    bool allowsChildFrameContent() const { return m_type.contains(Type::AllowChildFrameContent); }
    bool allowsVisibleChildFrameContent() const { return m_type.contains(Type::AllowVisibleChildFrameContentOnly); }
    bool isChildFrameHitTest() const { return m_type.contains(Type::ChildFrameHitTest); }
    bool resultIsElementList() const { return m_type.contains(Type::CollectMultipleElements); }
    bool includesAllElementsUnderPoint() const { return m_type.contains(Type::IncludeAllElementsUnderPoint); }

    // Convenience functions
    bool touchMove() const { return move() && touchEvent(); }
    bool touchRelease() const { return release() && touchEvent(); }

    OptionSet<Type> type() const { return m_type; }

private:
    OptionSet<Type> m_type;
};

} // namespace WebCore
