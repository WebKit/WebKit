/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "SVGPropertyTraits.h"
#include "SVGValueProperty.h"

namespace WebCore {

class SVGRect : public SVGValueProperty<FloatRect> {
    using Base = SVGValueProperty<FloatRect>;
    using Base::Base;
    using Base::m_value;

public:
    static Ref<SVGRect> create(const FloatRect& value = { })
    {
        return adoptRef(*new SVGRect(value));
    }

    static Ref<SVGRect> create(SVGPropertyOwner* owner, SVGPropertyAccess access, const FloatRect& value = { })
    {
        return adoptRef(*new SVGRect(owner, access, value));
    }

    template<typename T>
    static ExceptionOr<Ref<SVGRect>> create(ExceptionOr<T>&& value)
    {
        if (value.hasException())
            return value.releaseException();
        return adoptRef(*new SVGRect(value.releaseReturnValue()));
    }

    float x() { return m_value.x(); }

    ExceptionOr<void> setX(float xValue)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        m_value.setX(xValue);
        commitChange();
        return { };
    }

    float y() { return m_value.y(); }

    ExceptionOr<void> setY(float xValue)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        m_value.setY(xValue);
        commitChange();
        return { };
    }

    float width() { return m_value.width(); }

    ExceptionOr<void> setWidth(float widthValue)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        m_value.setWidth(widthValue);
        commitChange();
        return { };
    }

    float height() { return m_value.height(); }

    ExceptionOr<void> setHeight(float heightValue)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        m_value.setHeight(heightValue);
        commitChange();
        return { };
    }
    
    String valueAsString() const override
    {
        return SVGPropertyTraits<FloatRect>::toString(m_value);
    }
};

}
