/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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

#include "ContainerNode.h"
#include <wtf/MainThread.h>

namespace WebCore {

class NoEventDispatchAssertion {
public:
    NoEventDispatchAssertion()
    {
#if !ASSERT_DISABLED
        if (!isMainThread())
            return;
        ++s_count;
#endif
    }

    NoEventDispatchAssertion(const NoEventDispatchAssertion&)
        : NoEventDispatchAssertion()
    {
    }

    ~NoEventDispatchAssertion()
    {
#if !ASSERT_DISABLED
        if (!isMainThread())
            return;
        ASSERT(s_count);
        s_count--;
#endif
    }

    static bool isEventAllowedInMainThread()
    {
#if ASSERT_DISABLED
        return true;
#else
        return !isMainThread() || !s_count;
#endif
    }

    static bool isEventDispatchAllowedInSubtree(Node& node)
    {
        return isEventAllowedInMainThread() || EventAllowedScope::isAllowedNode(node);
    }

#if !ASSERT_DISABLED
    class EventAllowedScope {
    public:
        explicit EventAllowedScope(ContainerNode& userAgentContentRoot)
            : m_eventAllowedTreeRoot(userAgentContentRoot)
            , m_previousScope(s_currentScope)
        {
            s_currentScope = this;
        }

        ~EventAllowedScope()
        {
            s_currentScope = m_previousScope;
        }

        static bool isAllowedNode(Node& node)
        {
            return s_currentScope && s_currentScope->isAllowedNodeInternal(node);
        }

    private:
        bool isAllowedNodeInternal(Node& node)
        {
            return m_eventAllowedTreeRoot->contains(&node) || (m_previousScope && m_previousScope->isAllowedNodeInternal(node));
        }

        Ref<ContainerNode> m_eventAllowedTreeRoot;

        EventAllowedScope* m_previousScope;
        static EventAllowedScope* s_currentScope;
    };
#else
    class EventAllowedScope {
    public:
        explicit EventAllowedScope(ContainerNode&) { }
        static bool isAllowedNode(Node&) { return true; }
    };
#endif

#if !ASSERT_DISABLED
    class DisableAssertionsInScope {
    public:
        DisableAssertionsInScope()
        {
            if (!isMainThread())
                return;
            s_existingCount = s_count;
            s_count = 0;
        }

        ~DisableAssertionsInScope()
        {
            s_count = s_existingCount;
            s_existingCount = 0;
        }
    private:
        WEBCORE_EXPORT static unsigned s_existingCount;
    };
#else
    class DisableAssertionsInScope {
    public:
        DisableAssertionsInScope() { }
    };
#endif

#if !ASSERT_DISABLED
private:
    WEBCORE_EXPORT static unsigned s_count;
#endif
};

} // namespace WebCore
