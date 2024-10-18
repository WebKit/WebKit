/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <optional>
#include <wtf/Forward.h>

namespace WebCore {

namespace Style {
class BuilderState;
}

class BasicShape;
class BasicShapeCenterCoordinate;
class BasicShapePath;
class BasicShapeShape;
class CSSPathValue;
class CSSShapeValue;
class CSSToLengthConversionData;
class CSSValue;
class RenderStyle;
enum class CoordinateAffinity : uint8_t;
struct Length;
struct LengthPoint;
struct LengthSize;

enum class SVGPathConversion : bool { None, ForceAbsolute };

Length convertToLength(const CSSToLengthConversionData&, const CSSValue&);
LengthSize convertToLengthSize(const CSSToLengthConversionData&, const CSSValue&);
LengthPoint coordinatePairToLengthPoint(const CSSToLengthConversionData&, const CSSValue&);
LengthPoint positionOrCoordinatePairToLengthPoint(const CSSValue&, CoordinateAffinity, const Style::BuilderState&);

Ref<CSSValue> valueForBasicShape(const RenderStyle&, const BasicShape&, SVGPathConversion = SVGPathConversion::None);
Ref<CSSValue> valueForSVGPath(const BasicShapePath&, SVGPathConversion = SVGPathConversion::None);

Ref<BasicShape> basicShapeForValue(const CSSValue&, const Style::BuilderState&, std::optional<float> zoom = std::nullopt);
Ref<BasicShapePath> basicShapePathForValue(const CSSPathValue&, const Style::BuilderState&, std::optional<float> zoom = std::nullopt);
Ref<BasicShapeShape> basicShapeShapeForValue(const CSSShapeValue&, const Style::BuilderState&);

float floatValueForCenterCoordinate(const BasicShapeCenterCoordinate&, float);

}
