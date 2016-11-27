/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "HTMLElement.h"
#include <wtf/Optional.h>

namespace WebCore {

class HTMLOListElement final : public HTMLElement {
public:
    static Ref<HTMLOListElement> create(Document&);
    static Ref<HTMLOListElement> create(const QualifiedName&, Document&);

    // FIXME: The reason we have this start() function which does not trigger layout is because it is called
    // from rendering code and this is unfortunately one of the few cases where the render tree is mutated
    // while in layout.
    int start() const { return m_start ? m_start.value() : (m_isReversed ? itemCount() : 1); }
    int startForBindings() const { return m_start ? m_start.value() : (m_isReversed ? itemCountAfterLayout() : 1); }

    WEBCORE_EXPORT void setStartForBindings(int);

    bool isReversed() const { return m_isReversed; }

    void itemCountChanged() { m_shouldRecalculateItemCount = true; }

private:
    HTMLOListElement(const QualifiedName&, Document&);
        
    void updateItemValues();

    WEBCORE_EXPORT unsigned itemCountAfterLayout() const;
    WEBCORE_EXPORT unsigned itemCount() const;

    WEBCORE_EXPORT void recalculateItemCount();

    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    bool isPresentationAttribute(const QualifiedName&) const final;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) final;

    std::optional<int> m_start;
    unsigned m_itemCount;

    bool m_isReversed : 1;
    bool m_shouldRecalculateItemCount : 1;
};

} //namespace
