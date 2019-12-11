/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005-2008, 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#pragma once

#include "FloatSize.h"
#include "TransformationMatrix.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

// CSS Transforms (may become part of CSS3)

class TransformOperation : public RefCounted<TransformOperation> {
public:
    enum OperationType {
        SCALE_X, SCALE_Y, SCALE, 
        TRANSLATE_X, TRANSLATE_Y, TRANSLATE, 
        ROTATE,
        ROTATE_Z = ROTATE,
        SKEW_X, SKEW_Y, SKEW, 
        MATRIX,
        SCALE_Z, SCALE_3D,
        TRANSLATE_Z, TRANSLATE_3D,
        ROTATE_X, ROTATE_Y, ROTATE_3D,
        MATRIX_3D,
        PERSPECTIVE,
        IDENTITY, NONE
    };

    TransformOperation(OperationType type)
        : m_type(type)
    {
    }
    virtual ~TransformOperation() = default;

    virtual Ref<TransformOperation> clone() const = 0;

    virtual bool operator==(const TransformOperation&) const = 0;
    bool operator!=(const TransformOperation& o) const { return !(*this == o); }

    virtual bool isIdentity() const = 0;

    // Return true if the borderBoxSize was used in the computation, false otherwise.
    virtual bool apply(TransformationMatrix&, const FloatSize& borderBoxSize) const = 0;

    virtual Ref<TransformOperation> blend(const TransformOperation* from, double progress, bool blendToIdentity = false) = 0;

    OperationType type() const { return m_type; }
    bool isSameType(const TransformOperation& other) const { return type() == other.type(); }

    virtual bool isAffectedByTransformOrigin() const { return false; }
    
    bool is3DOperation() const
    {
        OperationType opType = type();
        return opType == SCALE_Z ||
               opType == SCALE_3D ||
               opType == TRANSLATE_Z ||
               opType == TRANSLATE_3D ||
               opType == ROTATE_X ||
               opType == ROTATE_Y ||
               opType == ROTATE_3D ||
               opType == MATRIX_3D ||
               opType == PERSPECTIVE;
    }
    
    virtual bool isRepresentableIn2D() const { return true; }

    bool isRotateTransformOperationType() const
    {
        return type() == ROTATE_X || type() == ROTATE_Y || type() == ROTATE_Z || type() == ROTATE || type() == ROTATE_3D;
    }

    bool isScaleTransformOperationType() const
    {
        return type() == SCALE_X || type() == SCALE_Y || type() == SCALE_Z || type() == SCALE || type() == SCALE_3D;
    }

    bool isSkewTransformOperationType() const
    {
        return type() == SKEW_X || type() == SKEW_Y || type() == SKEW;
    }

    bool isTranslateTransformOperationType() const
    {
        return type() == TRANSLATE_X || type() == TRANSLATE_Y || type() == TRANSLATE_Z || type() == TRANSLATE || type() == TRANSLATE_3D;
    }
    
    virtual void dump(WTF::TextStream&) const = 0;

private:
    OperationType m_type;
};

WTF::TextStream& operator<<(WTF::TextStream&, TransformOperation::OperationType);
WTF::TextStream& operator<<(WTF::TextStream&, const TransformOperation&);

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_TRANSFORMOPERATION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
    static bool isType(const WebCore::TransformOperation& operation) { return operation.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
