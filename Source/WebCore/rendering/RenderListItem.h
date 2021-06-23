/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2018 Apple Inc. All rights reserved.
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

#include "RenderBlockFlow.h"
#include "RenderListMarker.h"

namespace WebCore {

class HTMLOListElement;

class RenderListItem final : public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED(RenderListItem);
public:
    RenderListItem(Element&, RenderStyle&&);
    virtual ~RenderListItem();

    Element& element() const { return downcast<Element>(nodeForNonAnonymous()); }

    int value() const;
    void updateValue();

    std::optional<int> explicitValue() const { return m_valueWasSetExplicitly ? m_value : std::nullopt; }
    void setExplicitValue(std::optional<int>);

    void setNotInList(bool notInList) { m_notInList = notInList; }
    bool notInList() const { return m_notInList; }

    WEBCORE_EXPORT StringView markerTextWithoutSuffix() const;
    StringView markerTextWithSuffix() const;

    void updateListMarkerNumbers();

    static void updateItemValuesForOrderedList(const HTMLOListElement&);
    static unsigned itemCountForOrderedList(const HTMLOListElement&);

    RenderStyle computeMarkerStyle() const;

    RenderListMarker* markerRenderer() const { return m_marker.get(); }
    void setMarkerRenderer(RenderListMarker& marker) { m_marker = makeWeakPtr(marker); }

    bool isInReversedOrderedList() const;

private:
    const char* renderName() const final { return "RenderListItem"; }

    bool isListItem() const final { return true; }
    
    void insertedIntoTree(IsInternalMove) final;
    void willBeRemovedFromTree(IsInternalMove) final;

    void paint(PaintInfo&, const LayoutPoint&) final;

    void layout() final;

    void positionListMarker();

    void addOverflowFromChildren() final;
    void computePreferredLogicalWidths() final;

    void updateValueNow() const;
    void explicitValueChanged();

    WeakPtr<RenderListMarker> m_marker;
    mutable std::optional<int> m_value;
    bool m_valueWasSetExplicitly { false };
    bool m_notInList { false };
};

bool isHTMLListElement(const Node&);

inline int RenderListItem::value() const
{
    if (!m_value)
        updateValueNow();
    return m_value.value();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderListItem, isListItem())
