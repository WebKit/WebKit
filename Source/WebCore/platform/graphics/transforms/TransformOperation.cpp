/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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
#include "TransformOperation.h"

#include "IdentityTransformOperation.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

void IdentityTransformOperation::dump(TextStream& ts) const
{
    ts << type();
}

TextStream& operator<<(TextStream& ts, TransformOperation::OperationType type)
{
    switch (type) {
    case TransformOperation::SCALE_X: ts << "scaleX"; break;
    case TransformOperation::SCALE_Y: ts << "scaleY"; break;
    case TransformOperation::SCALE: ts << "scale"; break;
    case TransformOperation::TRANSLATE_X: ts << "translateX"; break;
    case TransformOperation::TRANSLATE_Y: ts << "translateY"; break;
    case TransformOperation::TRANSLATE: ts << "translate"; break;
    case TransformOperation::ROTATE: ts << "rotate"; break;
    case TransformOperation::SKEW_X: ts << "skewX"; break;
    case TransformOperation::SKEW_Y: ts << "skewY"; break;
    case TransformOperation::SKEW: ts << "skew"; break;
    case TransformOperation::MATRIX: ts << "matrix"; break;
    case TransformOperation::SCALE_Z: ts << "scaleX"; break;
    case TransformOperation::SCALE_3D: ts << "scale3d"; break;
    case TransformOperation::TRANSLATE_Z: ts << "translateZ"; break;
    case TransformOperation::TRANSLATE_3D: ts << "translate3d"; break;
    case TransformOperation::ROTATE_X: ts << "rotateX"; break;
    case TransformOperation::ROTATE_Y: ts << "rotateY"; break;
    case TransformOperation::ROTATE_Z: ts << "rotateZ"; break;
    case TransformOperation::ROTATE_3D: ts << "rotate3d"; break;
    case TransformOperation::MATRIX_3D: ts << "matrix3d"; break;
    case TransformOperation::PERSPECTIVE: ts << "perspective"; break;
    case TransformOperation::IDENTITY: ts << "identity"; break;
    case TransformOperation::NONE: ts << "none"; break;
    }
    
    return ts;
}

TextStream& operator<<(TextStream& ts, const TransformOperation& operation)
{
    operation.dump(ts);
    return ts;
}

std::optional<TransformOperation::OperationType> TransformOperation::sharedPrimitiveType(OperationType other) const
{
    // https://drafts.csswg.org/css-transforms-2/#interpolation-of-transform-functions
    // "If both transform functions share a primitive in the two-dimensional space, both transform
    // functions get converted to the two-dimensional primitive. If one or both transform functions
    // are three-dimensional transform functions, the common three-dimensional primitive is used."
    auto type = primitiveType();
    if (type == other)
        return type;
    static constexpr OperationType sharedPrimitives[][2] = {
        { ROTATE, ROTATE_3D },
        { SCALE, SCALE_3D },
        { TRANSLATE, TRANSLATE_3D }
    };
    for (auto typePair : sharedPrimitives) {
        if ((type == typePair[0] || type == typePair[1]) && (other == typePair[0] || other == typePair[1]))
            return typePair[1];
    }
    return std::nullopt;
}

std::optional<TransformOperation::OperationType> TransformOperation::sharedPrimitiveType(const TransformOperation* other) const
{
    // Blending with a null operation is always supported via blending with identity.
    if (!other)
        return type();

    // In case we have the same type, make sure to preserve it.
    if (other->type() == type())
        return type();

    return sharedPrimitiveType(other->primitiveType());
}

} // namespace WebCore
