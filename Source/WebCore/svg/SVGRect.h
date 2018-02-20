/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "SVGPropertyTearOff.h"
#include "SVGPropertyTraits.h"

namespace WebCore {

class SVGRect : public SVGPropertyTearOff<FloatRect> {
public:
    static Ref<SVGRect> create(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, FloatRect& value)
    {
        return adoptRef(*new SVGRect(animatedProperty, role, value));
    }

    static Ref<SVGRect> create(const FloatRect& initialValue = { })
    {
        return adoptRef(*new SVGRect(initialValue));
    }

    template<typename T> static ExceptionOr<Ref<SVGRect>> create(ExceptionOr<T>&& initialValue)
    {
        if (initialValue.hasException())
            return initialValue.releaseException();
        return create(initialValue.releaseReturnValue());
    }

    float x()
    {
        return propertyReference().x();
    }

    ExceptionOr<void> setX(float xValue)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setX(xValue);
        commitChange();

        return { };
    }

    float y()
    {
        return propertyReference().y();
    }

    ExceptionOr<void> setY(float xValue)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setY(xValue);
        commitChange();

        return { };
    }

    float width()
    {
        return propertyReference().width();
    }

    ExceptionOr<void> setWidth(float widthValue)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setWidth(widthValue);
        commitChange();

        return { };
    }

    float height()
    {
        return propertyReference().height();
    }

    ExceptionOr<void> setHeight(float heightValue)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setHeight(heightValue);
        commitChange();

        return { };
    }

private:
    SVGRect(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, FloatRect& value)
        : SVGPropertyTearOff<FloatRect>(&animatedProperty, role, value)
    {
    }

    explicit SVGRect(const FloatRect& initialValue)
        : SVGPropertyTearOff<FloatRect>(initialValue)
    {
    }
};


} // namespace WebCore
