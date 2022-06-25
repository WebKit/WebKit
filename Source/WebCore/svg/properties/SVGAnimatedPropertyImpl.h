/*
 * Copyright (C) 2018-2019 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SVGAngle.h"
#include "SVGAnimatedDecoratedProperty.h"
#include "SVGAnimatedPrimitiveProperty.h"
#include "SVGAnimatedPropertyList.h"
#include "SVGAnimatedValueProperty.h"
#include "SVGDecoratedEnumeration.h"
#include "SVGLength.h"
#include "SVGLengthList.h"
#include "SVGMarkerTypes.h"
#include "SVGNumberList.h"
#include "SVGPathSegList.h"
#include "SVGPointList.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGRect.h"
#include "SVGTransformList.h"

namespace WebCore {

using SVGAnimatedBoolean = SVGAnimatedPrimitiveProperty<bool>;
using SVGAnimatedInteger = SVGAnimatedPrimitiveProperty<int>;
using SVGAnimatedNumber = SVGAnimatedPrimitiveProperty<float>;
using SVGAnimatedString = SVGAnimatedPrimitiveProperty<String>;

using SVGAnimatedEnumeration = SVGAnimatedDecoratedProperty<SVGDecoratedEnumeration, unsigned>;

using SVGAnimatedAngle = SVGAnimatedValueProperty<SVGAngle>;
using SVGAnimatedLength = SVGAnimatedValueProperty<SVGLength>;
using SVGAnimatedRect = SVGAnimatedValueProperty<SVGRect>;
using SVGAnimatedPreserveAspectRatio = SVGAnimatedValueProperty<SVGPreserveAspectRatio>;

using SVGAnimatedLengthList = SVGAnimatedPropertyList<SVGLengthList>;
using SVGAnimatedNumberList = SVGAnimatedPropertyList<SVGNumberList>;
using SVGAnimatedPointList = SVGAnimatedPropertyList<SVGPointList>;
using SVGAnimatedTransformList = SVGAnimatedPropertyList<SVGTransformList>;

class SVGAnimatedOrientType : public SVGAnimatedEnumeration {
public:
    using SVGAnimatedEnumeration::SVGAnimatedEnumeration;

    static Ref<SVGAnimatedOrientType> create(SVGElement* contextElement, SVGMarkerOrientType baseValue)
    {
        return SVGAnimatedEnumeration::create<SVGMarkerOrientType, SVGAnimatedOrientType>(contextElement, baseValue);
    }
};

class SVGAnimatedPathSegList : public SVGAnimatedPropertyList<SVGPathSegList> {
    using Base = SVGAnimatedPropertyList<SVGPathSegList>;
    using Base::Base;

public:
    static Ref<SVGAnimatedPathSegList> create(SVGElement* contextElement)
    {
        return adoptRef(*new SVGAnimatedPathSegList(contextElement));
    }

    SVGPathByteStream& currentPathByteStream()
    {
        return isAnimating() ? animVal()->pathByteStream() : baseVal()->pathByteStream();
    }

    Path currentPath()
    {
        return isAnimating() ? animVal()->path() : baseVal()->path();
    }

    size_t approximateMemoryCost() const
    {
        if (isAnimating())
            return baseVal()->approximateMemoryCost() + animVal()->approximateMemoryCost();
        return baseVal()->approximateMemoryCost();
    }
};

}
