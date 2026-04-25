/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "StyleTransformFunctionBase.h"

#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

TextStream& operator<<(TextStream& ts, TransformFunctionBase::Type type)
{
    switch (type) {
    case TransformFunctionBase::Type::ScaleX: ts << "scaleX"_s; break;
    case TransformFunctionBase::Type::ScaleY: ts << "scaleY"_s; break;
    case TransformFunctionBase::Type::Scale: ts << "scale"_s; break;
    case TransformFunctionBase::Type::TranslateX: ts << "translateX"_s; break;
    case TransformFunctionBase::Type::TranslateY: ts << "translateY"_s; break;
    case TransformFunctionBase::Type::Translate: ts << "translate"_s; break;
    case TransformFunctionBase::Type::Rotate: ts << "rotate"_s; break;
    case TransformFunctionBase::Type::SkewX: ts << "skewX"_s; break;
    case TransformFunctionBase::Type::SkewY: ts << "skewY"_s; break;
    case TransformFunctionBase::Type::Skew: ts << "skew"_s; break;
    case TransformFunctionBase::Type::Matrix: ts << "matrix"_s; break;
    case TransformFunctionBase::Type::ScaleZ: ts << "scaleX"_s; break;
    case TransformFunctionBase::Type::Scale3D: ts << "scale3d"_s; break;
    case TransformFunctionBase::Type::TranslateZ: ts << "translateZ"_s; break;
    case TransformFunctionBase::Type::Translate3D: ts << "translate3d"_s; break;
    case TransformFunctionBase::Type::RotateX: ts << "rotateX"_s; break;
    case TransformFunctionBase::Type::RotateY: ts << "rotateY"_s; break;
    case TransformFunctionBase::Type::RotateZ: ts << "rotateZ"_s; break;
    case TransformFunctionBase::Type::Rotate3D: ts << "rotate3d"_s; break;
    case TransformFunctionBase::Type::Matrix3D: ts << "matrix3d"_s; break;
    case TransformFunctionBase::Type::Perspective: ts << "perspective"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const TransformFunctionBase& operation)
{
    operation.dump(ts);
    return ts;
}

std::optional<TransformFunctionBase::Type> TransformFunctionBase::sharedPrimitiveType(Type other) const
{
    // https://drafts.csswg.org/css-transforms-2/#interpolation-of-transform-functions
    // "If both transform functions share a primitive in the two-dimensional space, both transform
    // functions get converted to the two-dimensional primitive. If one or both transform functions
    // are three-dimensional transform functions, the common three-dimensional primitive is used."
    auto type = primitiveType();
    if (type == other)
        return type;
    static constexpr std::array sharedPrimitives {
        std::array { Type::Rotate, Type::Rotate3D },
        std::array { Type::Scale, Type::Scale3D },
        std::array { Type::Translate, Type::Translate3D }
    };
    for (auto typePair : sharedPrimitives) {
        if ((type == typePair[0] || type == typePair[1]) && (other == typePair[0] || other == typePair[1]))
            return typePair[1];
    }
    return std::nullopt;
}

std::optional<TransformFunctionBase::Type> TransformFunctionBase::sharedPrimitiveType(const TransformFunctionBase* other) const
{
    // Blending with a null operation is always supported via blending with identity.
    if (!other)
        return type();

    // In case we have the same type, make sure to preserve it.
    if (other->type() == type())
        return type();

    return sharedPrimitiveType(other->primitiveType());
}

auto ToPlatform<TransformFunctionType>::operator()(TransformFunctionType type) -> TransformOperationType
{
    switch (type) {
    case TransformFunctionType::ScaleX:         return TransformOperationType::ScaleX;
    case TransformFunctionType::ScaleY:         return TransformOperationType::ScaleY;
    case TransformFunctionType::Scale:          return TransformOperationType::Scale;
    case TransformFunctionType::TranslateX:     return TransformOperationType::TranslateX;
    case TransformFunctionType::TranslateY:     return TransformOperationType::TranslateY;
    case TransformFunctionType::Translate:      return TransformOperationType::Translate;
    case TransformFunctionType::RotateX:        return TransformOperationType::RotateX;
    case TransformFunctionType::RotateY:        return TransformOperationType::RotateY;
    case TransformFunctionType::Rotate:         return TransformOperationType::Rotate;
    case TransformFunctionType::SkewX:          return TransformOperationType::SkewX;
    case TransformFunctionType::SkewY:          return TransformOperationType::SkewY;
    case TransformFunctionType::Skew:           return TransformOperationType::Skew;
    case TransformFunctionType::Matrix:         return TransformOperationType::Matrix;
    case TransformFunctionType::ScaleZ:         return TransformOperationType::ScaleZ;
    case TransformFunctionType::Scale3D:        return TransformOperationType::Scale3D;
    case TransformFunctionType::TranslateZ:     return TransformOperationType::TranslateZ;
    case TransformFunctionType::Translate3D:    return TransformOperationType::Translate3D;
    case TransformFunctionType::RotateZ:        return TransformOperationType::RotateZ;
    case TransformFunctionType::Rotate3D:       return TransformOperationType::Rotate3D;
    case TransformFunctionType::Matrix3D:       return TransformOperationType::Matrix3D;
    case TransformFunctionType::Perspective:    return TransformOperationType::Perspective;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace Style
} // namespace WebCore
