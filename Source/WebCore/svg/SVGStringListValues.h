/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "QualifiedName.h"
#include "SVGAttributeAccessor.h"
#include "SVGPropertyTraits.h"

#include <wtf/Vector.h>

namespace WebCore {

class SVGElement;
class SVGStringList;
class SVGStringListValues;

template<typename T> 
class SVGPropertyTearOff;

using SVGStringListValuesAttribute = SVGPropertyAttribute<SVGStringListValues>;

template<typename OwnerType>
using SVGStringListValuesAttributeAccessor = SVGPropertyAttributeAccessor<OwnerType, SVGStringListValuesAttribute>;

class SVGStringListValues final : public Vector<String> {
public:
    SVGStringListValues(const QualifiedName& attributeName)
        : m_attributeName(attributeName)
    {
    }

    void reset(const String&);
    void parse(const String&, UChar delimiter = ',');

    // Only used by SVGStringList.
    void commitChange(SVGElement& contextElement);

    String valueAsString() const;

private:
    const QualifiedName& m_attributeName;
};

template<> struct SVGPropertyTraits<SVGStringListValues> {
    static String toString(const SVGStringListValues& list) { return list.valueAsString(); }

    using ListItemType = String;
    using ListItemTearOff = SVGPropertyTearOff<String>;
    using ListPropertyTearOff = SVGStringList;
};

} // namespace WebCore
