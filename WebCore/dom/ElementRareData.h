/**
 *
 * Copyright (C) 2008 Apple Computer, Inc.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
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

#ifndef ElementRareData_h
#define ElementRareData_h

#include "Element.h"
#include "NodeRareData.h"

namespace WebCore {

class ElementRareData : public NodeRareData {
public:
    ElementRareData(Element*);

    void resetComputedStyle(Element*);

    using NodeRareData::needsFocusAppearanceUpdateSoonAfterAttach;
    using NodeRareData::setNeedsFocusAppearanceUpdateSoonAfterAttach;

    IntSize m_minimumSizeForResizing;
    RefPtr<RenderStyle> m_computedStyle;
};

inline IntSize defaultMinimumSizeForResizing()
{
    return IntSize(INT_MAX, INT_MAX);
}

inline ElementRareData::ElementRareData(Element* e)
    : m_minimumSizeForResizing(defaultMinimumSizeForResizing())
{
}

inline void ElementRareData::resetComputedStyle(Element* element)
{
    m_computedStyle.clear();
}

}
#endif // ElementRareData_h
