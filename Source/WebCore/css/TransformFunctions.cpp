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
#include <wtf/text/StringConcatenateNumbers.h>

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
    return primitiveValue ? primitiveValue->convertToLength<FixedFloatConversion | PercentConversion | CalculatedConversion>(conversionData) : Length(LengthType::Undefined);
}

// FIXME: This should return std::optional<TransformOperations>
bool transformsForValue(const CSSValue& value, const CSSToLengthConversionData& conversionData, TransformOperations& outOperations)
{
    ASSERT(!outOperations.size());
    if (!is<CSSValueList>(value))
        return false;

    auto& operations = outOperations.operations();
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
                sy = firstValue.doubleValueDividingBy100IfPercentage();
            else {
                sx = firstValue.doubleValueDividingBy100IfPercentage();
                if (transformValue.name() != CSSValueScaleX) {
                    if (transformValue.length() > 1) {
                        auto& secondValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(1));
                        sy = secondValue.doubleValueDividingBy100IfPercentage();
                    } else
                        sy = sx;
                }
            }
            operations.append(ScaleTransformOperation::create(sx, sy, 1.0, transformOperationType(transformValue.name())));
            break;
        }
        case CSSValueScaleZ:
        case CSSValueScale3d: {
            double sx = 1.0;
            double sy = 1.0;
            double sz = 1.0;
            if (transformValue.name() == CSSValueScaleZ)
                sz = firstValue.doubleValueDividingBy100IfPercentage();
            else if (transformValue.name() == CSSValueScaleY)
                sy = firstValue.doubleValueDividingBy100IfPercentage();
            else {
                sx = firstValue.doubleValueDividingBy100IfPercentage();
                if (transformValue.name() != CSSValueScaleX) {
                    if (transformValue.length() > 2) {
                        auto& thirdValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(2));
                        sz = thirdValue.doubleValueDividingBy100IfPercentage();
                    }
                    if (transformValue.length() > 1) {
                        auto& secondValue = downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(1));
                        sy = secondValue.doubleValueDividingBy100IfPercentage();
                    } else
                        sy = sx;
                }
            }
            operations.append(ScaleTransformOperation::create(sx, sy, sz, transformOperationType(transformValue.name())));
            break;
        }
        case CSSValueTranslate:
        case CSSValueTranslateX:
        case CSSValueTranslateY: {
            Length tx = Length(0, LengthType::Fixed);
            Length ty = Length(0, LengthType::Fixed);
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

            if (tx.isUndefined() || ty.isUndefined()) {
                operations.clear();
                return false;
            }

            operations.append(TranslateTransformOperation::create(tx, ty, Length(0, LengthType::Fixed), transformOperationType(transformValue.name())));
            break;
        }
        case CSSValueTranslateZ:
        case CSSValueTranslate3d: {
            Length tx = Length(0, LengthType::Fixed);
            Length ty = Length(0, LengthType::Fixed);
            Length tz = Length(0, LengthType::Fixed);
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

            if (tx.isUndefined() || ty.isUndefined() || tz.isUndefined()) {
                operations.clear();
                return false;
            }

            operations.append(TranslateTransformOperation::create(tx, ty, tz, transformOperationType(transformValue.name())));
            break;
        }
        case CSSValueRotate: {
            double angle = firstValue.computeDegrees();
            operations.append(RotateTransformOperation::create(0, 0, 1, angle, transformOperationType(transformValue.name())));
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
            operations.append(RotateTransformOperation::create(x, y, z, angle, transformOperationType(transformValue.name())));
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
            operations.append(RotateTransformOperation::create(x, y, z, angle, transformOperationType(transformValue.name())));
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
            operations.append(SkewTransformOperation::create(angleX, angleY, transformOperationType(transformValue.name())));
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
            operations.append(MatrixTransformOperation::create(a, b, c, d, e, f));
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
                conversionData.zoom() * downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(14)).doubleValue(),
                downcast<CSSPrimitiveValue>(*transformValue.itemWithoutBoundsCheck(15)).doubleValue());
            operations.append(Matrix3DTransformOperation::create(matrix));
            break;
        }
        case CSSValuePerspective: {
            std::optional<Length> perspectiveLength;
            if (!firstValue.isValueID()) {
                if (firstValue.isLength())
                    perspectiveLength = convertToFloatLength(&firstValue, conversionData);
                else {
                    // This is a quirk that should go away when 3d transforms are finalized.
                    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=232669
                    // This does not deal properly with calc(), because we aren't passing conversionData here.
                    double doubleValue = firstValue.doubleValue();
                    if (doubleValue < 0) {
                        operations.clear();
                        return false;
                    }
                    perspectiveLength = Length(clampToPositiveInteger(doubleValue), LengthType::Fixed);
                }
            } else
                ASSERT(firstValue.valueID() == CSSValueNone);

            operations.append(PerspectiveTransformOperation::create(perspectiveLength));
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    return true;
}

RefPtr<TranslateTransformOperation> translateForValue(const CSSValue& value, const CSSToLengthConversionData& conversionData)
{
    if (!is<CSSValueList>(value))
        return nullptr;

    auto& valueList = downcast<CSSValueList>(value);
    if (!valueList.length())
        return nullptr;

    auto type = TransformOperation::TRANSLATE;
    Length tx = Length(0, LengthType::Fixed);
    Length ty = Length(0, LengthType::Fixed);
    Length tz = Length(0, LengthType::Fixed);
    for (unsigned i = 0; i < valueList.length(); ++i) {
        auto* valueItem = valueList.itemWithoutBoundsCheck(i);
        if (!is<CSSPrimitiveValue>(valueItem))
            return nullptr;
        if (!i)
            tx = convertToFloatLength(downcast<CSSPrimitiveValue>(valueItem), conversionData);
        else if (i == 1)
            ty = convertToFloatLength(downcast<CSSPrimitiveValue>(valueItem), conversionData);
        else if (i == 2) {
            type = TransformOperation::TRANSLATE_3D;
            tz = convertToFloatLength(downcast<CSSPrimitiveValue>(valueItem), conversionData);
        }
    }

    return TranslateTransformOperation::create(tx, ty, tz, type);
}

RefPtr<ScaleTransformOperation> scaleForValue(const CSSValue& value)
{
    if (!is<CSSValueList>(value))
        return nullptr;

    auto& valueList = downcast<CSSValueList>(value);
    if (!valueList.length())
        return nullptr;

    auto type = TransformOperation::SCALE;
    double sx = 1.0;
    double sy = 1.0;
    double sz = 1.0;
    for (unsigned i = 0; i < valueList.length(); ++i) {
        auto* valueItem = valueList.itemWithoutBoundsCheck(i);
        if (!is<CSSPrimitiveValue>(valueItem))
            return nullptr;
        if (!i) {
            sx = downcast<CSSPrimitiveValue>(*valueItem).doubleValueDividingBy100IfPercentage();
            sy = sx;
        } else if (i == 1)
            sy = downcast<CSSPrimitiveValue>(*valueItem).doubleValueDividingBy100IfPercentage();
        else if (i == 2) {
            type = TransformOperation::SCALE_3D;
            sz = downcast<CSSPrimitiveValue>(*valueItem).doubleValueDividingBy100IfPercentage();
        }
    }

    return ScaleTransformOperation::create(sx, sy, sz, type);
}

RefPtr<RotateTransformOperation> rotateForValue(const CSSValue& value)
{
    if (!is<CSSValueList>(value))
        return nullptr;

    auto& valueList = downcast<CSSValueList>(value);
    auto numberOfItems = valueList.length();

    // There are three scenarios here since the rotation axis is defined either as:
    //     - no value: implicit 2d rotation
    //     - 1 value: an axis identifier (x/y/z)
    //     - 3 values: three numbers defining an x/y/z vector
    // The angle is specified as the last value.
    if (numberOfItems != 1 && numberOfItems != 2 && numberOfItems != 4)
        return nullptr;

    auto* lastValue = valueList.itemWithoutBoundsCheck(numberOfItems - 1);
    if (!is<CSSPrimitiveValue>(lastValue))
        return nullptr;
    auto angle = downcast<CSSPrimitiveValue>(*lastValue).computeDegrees();

    if (numberOfItems == 1)
        return RotateTransformOperation::create(angle, TransformOperation::ROTATE);

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    auto type = TransformOperation::ROTATE;

    if (numberOfItems == 2) {
        // An axis identifier was specified.
        auto* axisIdentifierItem = valueList.itemWithoutBoundsCheck(0);
        if (!is<CSSPrimitiveValue>(axisIdentifierItem))
            return nullptr;
        auto axisIdentifier = downcast<CSSPrimitiveValue>(*axisIdentifierItem).valueID();
        if (axisIdentifier == CSSValueX) {
            type = TransformOperation::ROTATE_X;
            x = 1.0;
        } else if (axisIdentifier == CSSValueY) {
            type = TransformOperation::ROTATE_Y;
            y = 1.0;
        } else if (axisIdentifier == CSSValueZ) {
            type = TransformOperation::ROTATE_3D;
            z = 1.0;
        } else
            return nullptr;
    } else if (numberOfItems == 4) {
        // The axis was specified using a vector.
        type = TransformOperation::ROTATE;
        for (unsigned i = 0; i < 3; ++i) {
            auto* valueItem = valueList.itemWithoutBoundsCheck(i);
            if (!is<CSSPrimitiveValue>(valueItem))
                return nullptr;
            if (!i)
                x = downcast<CSSPrimitiveValue>(*valueItem).doubleValue();
            else if (i == 1)
                y = downcast<CSSPrimitiveValue>(*valueItem).doubleValue();
            else if (i == 2) {
                type = TransformOperation::ROTATE_3D;
                z = downcast<CSSPrimitiveValue>(*valueItem).doubleValue();
            }
        }
    }

    return RotateTransformOperation::create(x, y, z, angle, type);
}

}
