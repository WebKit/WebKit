/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include "CSSPropertyNames.h"
#include <wtf/Vector.h>

namespace WebCore {

class StylePropertyShorthand {
public:
    StylePropertyShorthand() = default;

    template<unsigned numProperties>
    StylePropertyShorthand(CSSPropertyID id, const CSSPropertyID (&properties)[numProperties])
        : m_properties(properties)
        , m_length(numProperties)
        , m_shorthandID(id)
    {
    }

    const CSSPropertyID* begin() const { return properties(); }
    const CSSPropertyID* end() const { return properties() + length(); }

    const CSSPropertyID* properties() const { return m_properties; }
    unsigned length() const { return m_length; }
    CSSPropertyID id() const { return m_shorthandID; }

private:
    const CSSPropertyID* m_properties { nullptr };
    unsigned m_length { 0 };
    CSSPropertyID m_shorthandID { CSSPropertyInvalid };
};

// Custom StylePropertyShorthand functions.
StylePropertyShorthand animationShorthandForParsing();
StylePropertyShorthand transitionShorthandForParsing();

// Returns empty value if the property is not a shorthand.
// The implementation is generated in StylePropertyShorthandFunctions.cpp.
StylePropertyShorthand shorthandForProperty(CSSPropertyID);

// Return the list of shorthands for a given longhand.
// The implementation is generated in StylePropertyShorthandFunctions.cpp.
using StylePropertyShorthandVector = Vector<StylePropertyShorthand, 4>;
StylePropertyShorthandVector matchingShorthandsForLonghand(CSSPropertyID);

unsigned indexOfShorthandForLonghand(CSSPropertyID, const StylePropertyShorthandVector&);

bool isShorthandCSSProperty(CSSPropertyID);

} // namespace WebCore
