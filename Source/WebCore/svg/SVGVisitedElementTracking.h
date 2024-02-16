/*
 * Copyright (c) 2024 Igalia S.L.
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

#pragma once

#include "SVGElement.h"
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class SVGVisitedElementTracking {
public:
    using VisitedSet = WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData>;

    SVGVisitedElementTracking(VisitedSet& visitedSet)
        : m_visitedElements(visitedSet)
    {
    }

    ~SVGVisitedElementTracking() = default;

    bool isEmpty() const { return m_visitedElements.isEmptyIgnoringNullReferences(); }
    bool isVisiting(const SVGElement& element) { return m_visitedElements.contains(element); }

    class Scope {
    public:
        Scope(SVGVisitedElementTracking& tracking, const SVGElement& element)
            : m_tracking(tracking)
            , m_element(element)
        {
            m_tracking.addUnique(element);
        }

        ~Scope()
        {
            if (RefPtr element = m_element.get())
                m_tracking.removeUnique(*element);
        }

    private:
        SVGVisitedElementTracking& m_tracking;
        WeakPtr<SVGElement, WeakPtrImplWithEventTargetData> m_element;
    };

private:
    friend class Scope;

    void addUnique(const SVGElement& element)
    {
        auto result = m_visitedElements.add(element);
        ASSERT_UNUSED(result, result.isNewEntry);
    }

    void removeUnique(const SVGElement& element)
    {
        bool result = m_visitedElements.remove(element);
        ASSERT_UNUSED(result, result);
    }

    VisitedSet& m_visitedElements;
};

}; // namespace WebCore
