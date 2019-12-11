/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
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

#include "config.h"
#include "TransformFunctions.h"

#include "CSSFunctionValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSValueList.h"
#include "Matrix3DTransformOperation.h"
#include "MatrixTransformOperation.h"
#include "PerspectiveTransformOperation.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "SkewTransformOperation.h"
#include "TranslateTransformOperation.h"

namespace WebCore {

static TransformOperation::OperationType transformOperationType(CSSValueID type)
{
    switch (type) {
    case CSSValueScale:
        return TransformOperation::SCALE;
    case CSSValueScaleX:
        return TransformOperation::SCALE_X;
    case CSSValueScaleY:
        return TransformOperation::SCALE_Y;
    case CSSValueScaleZ:
        return TransformOperation::SCALE_Z;
    case CSSValueScale3d:
        return TransformOperation::SCALE_3D;
    case CSSValueTranslate:
        return TransformOperation::TRANSLATE;
    case CSSValueTranslateX:
        return TransformOperation::TRANSLATE_X;
    case CSSValueTranslateY:
        return TransformOperation::TRANSLATE_Y;
    case CSSValueTranslateZ:
        return TransformOperation::TRANSLATE_Z;
    case CSSValueTranslate3d:
        return TransformOperation::TRANSLATE_3D;
    case CSSValueRotate:
        return TransformOperation::ROTATE;
    case CSSValueRotateX:
        return TransformOperation::ROTATE_X;
    case CSSValueRotateY:
        return TransformOperation::ROTATE_Y;
    case CSSValueRotateZ:
        return TransformOperation::ROTATE_Z;
    case CSSValueRotate3d:
        return TransformOperation::ROTATE_3D;
    case CSSValueSkew:
        return TransformOperation::SKEW;
    case CSSValueSkewX:
        return TransformOperation::SKEW_X;
    case CSSValueSkewY:
        return TransformOperation::SKEW_Y;
    case CSSValueMatrix:
        return TransformOperation::MATRIX;
    case CSSValueMatrix3d:
        return TransformOperation::MATRIX_3D;
    case CSSValuePerspective:
        return TransformOperation::PERSPECTIVE;
    default:
        break;
    }
    return TransformOperation::NONE;
}

Length convertToFloatLength(const CSSPrimitiveValue* primitiveValue, const CSSToLengthConversionData& conversionData)
{
    return primitiveValue ? primitiveValue->convertToLength<FixedFloatConversion | PercentConversion | CalculatedConversion>(conversionData) : Length(Undefined);
}

bool transformsForValue(const CSSValue& value, const CSSToLengthConversionData& conversionData, TransformOperations& outOperations)
{
    if (!is<CSSValueList>(value)) {
        outOperations.clear();
        return false;
    }

    TransformOperations operations;
    for (auto& currentValue : downcast<CSSValueList>(value)) {
        if (!is<CSSFunctionValue>(currentValue))
            continue;

        auto& transformValue = downcast<CSSFunctionValue>(currentValue.get());
        if (!transformValue.length())
            continue;

        bool haveNonPrimitiveValue = false;
        for (unsigned j = 0; j < transformValue.length(); ++j) {
            if (!is<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(j))) {
                haveNonPrimitiveValue = true;
                break;
            }
        }
        if (haveNonPrimitiveValue)
            continue;

        auto& firstValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(0));

        switch (transformValue.name()) {
        case CSSValueScale:
        case CSSValueScaleX:
        case CSSValueScaleY: {
            double sx = 1.0;
            double sy = 1.0;
            if (transformValue.name() == CSSValueScaleY)
                sy = firstValue.doubleValue();
            else {
                sx = firstValue.doubleValue();
                if (transformValue.name() != CSSValueScaleX) {
                    if (transformValue.length() > 1) {
                        auto& secondValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(1));
                        sy = secondValue.doubleValue();
                    } else
                        sy = sx;
                }
            }
            operations.operations().append(ScaleTransformOperation::create(sx, sy, 1.0, transformOperationType(transformValue.name())));
            break;
        }
        case CSSValueScaleZ:
        case CSSValueScale3d: {
            double sx = 1.0;
            double sy = 1.0;
            double sz = 1.0;
            if (transformValue.name() == CSSValueScaleZ)
                sz = firstValue.doubleValue();
            else if (transformValue.name() == CSSValueScaleY)
                sy = firstValue.doubleValue();
            else {
                sx = firstValue.doubleValue();
                if (transformValue.name() != CSSValueScaleX) {
                    if (transformValue.length() > 2) {
                        auto& thirdValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(2));
                        sz = thirdValue.doubleValue();
                    }
                    if (transformValue.length() > 1) {
                        auto& secondValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(1));
                        sy = secondValue.doubleValue();
                    } else
                        sy = sx;
                }
            }
            operations.operations().append(ScaleTransformOperation::create(sx, sy, sz, transformOperationType(transformValue.name())));
            break;
        }
        case CSSValueTranslate:
        case CSSValueTranslateX:
        case CSSValueTranslateY: {
            Length tx = Length(0, Fixed);
            Length ty = Length(0, Fixed);
            if (transformValue.name() == CSSValueTranslateY)
                ty = convertToFloatLength(&firstValue, conversionData);
            else {
                tx = convertToFloatLength(&firstValue, conversionData);
                if (transformValue.name() != CSSValueTranslateX) {
                    if (transformValue.length() > 1) {
                        auto& secondValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(1));
                        ty = convertToFloatLength(&secondValue, conversionData);
                    }
                }
            }

            if (tx.isUndefined() || ty.isUndefined())
                return false;

            operations.operations().append(TranslateTransformOperation::create(tx, ty, Length(0, Fixed), transformOperationType(transformValue.name())));
            break;
        }
        case CSSValueTranslateZ:
        case CSSValueTranslate3d: {
            Length tx = Length(0, Fixed);
            Length ty = Length(0, Fixed);
            Length tz = Length(0, Fixed);
            if (transformValue.name() == CSSValueTranslateZ)
                tz = convertToFloatLength(&firstValue, conversionData);
            else if (transformValue.name() == CSSValueTranslateY)
                ty = convertToFloatLength(&firstValue, conversionData);
            else {
                tx = convertToFloatLength(&firstValue, conversionData);
                if (transformValue.name() != CSSValueTranslateX) {
                    if (transformValue.length() > 2) {
                        auto& thirdValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(2));
                        tz = convertToFloatLength(&thirdValue, conversionData);
                    }
                    if (transformValue.length() > 1) {
                        auto& secondValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(1));
                        ty = convertToFloatLength(&secondValue, conversionData);
                    }
                }
            }

            if (tx.isUndefined() || ty.isUndefined() || tz.isUndefined())
                return false;

            operations.operations().append(TranslateTransformOperation::create(tx, ty, tz, transformOperationType(transformValue.name())));
            break;
        }
        case CSSValueRotate: {
            double angle = firstValue.computeDegrees();
            operations.operations().append(RotateTransformOperation::create(0, 0, 1, angle, transformOperationType(transformValue.name())));
            break;
        }
        case CSSValueRotateX:
        case CSSValueRotateY:
        case CSSValueRotateZ: {
            double x = 0;
            double y = 0;
            double z = 0;
            double angle = firstValue.computeDegrees();

            if (transformValue.name() == CSSValueRotateX)
                x = 1;
            else if (transformValue.name() == CSSValueRotateY)
                y = 1;
            else
                z = 1;
            operations.operations().append(RotateTransformOperation::create(x, y, z, angle, transformOperationType(transformValue.name())));
            break;
        }
        case CSSValueRotate3d: {
            if (transformValue.length() < 4)
                break;
            auto& secondValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(1));
            auto& thirdValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(2));
            auto& fourthValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(3));
            double x = firstValue.doubleValue();
            double y = secondValue.doubleValue();
            double z = thirdValue.doubleValue();
            double angle = fourthValue.computeDegrees();
            operations.operations().append(RotateTransformOperation::create(x, y, z, angle, transformOperationType(transformValue.name())));
            break;
        }
        case CSSValueSkew:
        case CSSValueSkewX:
        case CSSValueSkewY: {
            double angleX = 0;
            double angleY = 0;
            double angle = firstValue.computeDegrees();
            if (transformValue.name() == CSSValueSkewY)
                angleY = angle;
            else {
                angleX = angle;
                if (transformValue.name() == CSSValueSkew) {
                    if (transformValue.length() > 1) {
                        auto& secondValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(1));
                        angleY = secondValue.computeDegrees();
                    }
                }
            }
            operations.operations().append(SkewTransformOperation::create(angleX, angleY, transformOperationType(transformValue.name())));
            break;
        }
        case CSSValueMatrix: {
            if (transformValue.length() < 6)
                break;
            double a = firstValue.doubleValue();
            double b = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(1)).doubleValue();
            double c = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(2)).doubleValue();
            double d = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(3)).doubleValue();
            double e = conversionData.zoom() * downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(4)).doubleValue();
            double f = conversionData.zoom() * downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(5)).doubleValue();
            operations.operations().append(MatrixTransformOperation::create(a, b, c, d, e, f));
            break;
        }
        case CSSValueMatrix3d: {
            if (transformValue.length() < 16)
                break;
            TransformationMatrix matrix(downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(0)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(1)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(2)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(3)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(4)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(5)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(6)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(7)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(8)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(9)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(10)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(11)).doubleValue(),
                conversionData.zoom() * downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(12)).doubleValue(),
                conversionData.zoom() * downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(13)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(14)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(15)).doubleValue());
            operations.operations().append(Matrix3DTransformOperation::create(matrix));
            break;
        }
        case CSSValuePerspective: {
            Length p = Length(0, Fixed);
            if (firstValue.isLength())
                p = convertToFloatLength(&firstValue, conversionData);
            else {
                // This is a quirk that should go away when 3d transforms are finalized.
                double val = firstValue.doubleValue();
                p = val >= 0 ? Length(clampToPositiveInteger(val), Fixed) : Length(Undefined);
            }

            if (p.isUndefined())
                return false;

            operations.operations().append(PerspectiveTransformOperation::create(p));
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    outOperations = operations;
    return true;
}

}
