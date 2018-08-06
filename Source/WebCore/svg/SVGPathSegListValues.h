/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "SVGListProperty.h"
#include "SVGPathSeg.h"
#include "SVGPropertyTraits.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class SVGElement;
class SVGPathSegList;

template<typename T> 
class SVGPropertyTearOff;

class SVGPathSegListValues : public Vector<RefPtr<SVGPathSeg>> {
public:
    using Base = Vector<RefPtr<SVGPathSeg>>;
    
    explicit SVGPathSegListValues(SVGPathSegRole role)
        : m_role(role)
    {
    }
    
    SVGPathSegListValues(const SVGPathSegListValues&) = default;
    SVGPathSegListValues(SVGPathSegListValues&&) = default;
    
    SVGPathSegListValues& operator=(const SVGPathSegListValues& other)
    {
        clearContextAndRoles();
        return static_cast<SVGPathSegListValues&>(Base::operator=(other));
    }

    SVGPathSegListValues& operator=(SVGPathSegListValues&& other)
    {
        clearContextAndRoles();
        return static_cast<SVGPathSegListValues&>(Base::operator=(WTFMove(other)));
    }
    
    void clear()
    {
        clearContextAndRoles();
        Base::clear();
    }

    String valueAsString() const;

    void commitChange(SVGElement& contextElement, ListModification);
    void clearItemContextAndRole(unsigned index);

private:
    void clearContextAndRoles();

    SVGPathSegRole m_role;
};

template<> struct SVGPropertyTraits<SVGPathSegListValues> {
    static SVGPathSegListValues initialValue() { return SVGPathSegListValues(PathSegUndefinedRole); }
    static String toString(const SVGPathSegListValues& list) { return list.valueAsString(); }

    using ListItemType = RefPtr<SVGPathSeg>;
    using ListItemTearOff = SVGPropertyTearOff<RefPtr<SVGPathSeg>>;
    using ListPropertyTearOff = SVGPathSegList;
};

} // namespace WebCore
