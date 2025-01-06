/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "CSSTransformValue.h"

#include "CSSFunctionValue.h"
#include "CSSMatrixComponent.h"
#include "CSSPerspective.h"
#include "CSSRotate.h"
#include "CSSScale.h"
#include "CSSSkew.h"
#include "CSSSkewX.h"
#include "CSSSkewY.h"
#include "CSSTransformComponent.h"
#include "CSSTransformListValue.h"
#include "CSSTranslate.h"
#include "CSSValueKeywords.h"
#include "DOMMatrix.h"
#include "ExceptionOr.h"
#include <wtf/Algorithms.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSTransformValue);

static ExceptionOr<Ref<CSSTransformComponent>> createTransformComponent(Ref<const CSSFunctionValue> functionValue)
{
    auto makeTransformComponent = [&](auto exceptionOrTransformComponent) -> ExceptionOr<Ref<CSSTransformComponent>> {
        if (exceptionOrTransformComponent.hasException())
            return exceptionOrTransformComponent.releaseException();
        return Ref<CSSTransformComponent> { exceptionOrTransformComponent.releaseReturnValue() };
    };

    switch (functionValue->name()) {
    case CSSValueTranslateX:
    case CSSValueTranslateY:
    case CSSValueTranslateZ:
    case CSSValueTranslate:
    case CSSValueTranslate3d:
        return makeTransformComponent(CSSTranslate::create(WTFMove(functionValue)));
    case CSSValueScaleX:
    case CSSValueScaleY:
    case CSSValueScaleZ:
    case CSSValueScale:
    case CSSValueScale3d:
        return makeTransformComponent(CSSScale::create(WTFMove(functionValue)));
    case CSSValueRotateX:
    case CSSValueRotateY:
    case CSSValueRotateZ:
    case CSSValueRotate:
    case CSSValueRotate3d:
        return makeTransformComponent(CSSRotate::create(WTFMove(functionValue)));
    case CSSValueSkewX:
        return makeTransformComponent(CSSSkewX::create(WTFMove(functionValue)));
    case CSSValueSkewY:
        return makeTransformComponent(CSSSkewY::create(WTFMove(functionValue)));
    case CSSValueSkew:
        return makeTransformComponent(CSSSkew::create(WTFMove(functionValue)));
    case CSSValuePerspective:
        return makeTransformComponent(CSSPerspective::create(WTFMove(functionValue)));
    case CSSValueMatrix:
    case CSSValueMatrix3d:
        return makeTransformComponent(CSSMatrixComponent::create(WTFMove(functionValue)));
    default:
        return Exception { ExceptionCode::TypeError, "Unexpected function value type"_s };
    }
}

ExceptionOr<Ref<CSSTransformValue>> CSSTransformValue::create(Ref<const CSSTransformListValue> list)
{
    Vector<Ref<CSSTransformComponent>> components;
    for (auto& value : list.get()) {
        RefPtr functionValue = dynamicDowncast<CSSFunctionValue>(value);
        if (!functionValue)
            return Exception { ExceptionCode::TypeError, "Expected only function values in a transform list."_s };
        auto component = createTransformComponent(functionValue.releaseNonNull());
        if (component.hasException())
            return component.releaseException();
        components.append(component.releaseReturnValue());
    }
    return adoptRef(*new CSSTransformValue(WTFMove(components)));
}

ExceptionOr<Ref<CSSTransformValue>> CSSTransformValue::create(Vector<Ref<CSSTransformComponent>>&& transforms)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-csstransformvalue-csstransformvalue
    if (transforms.isEmpty())
        return Exception { ExceptionCode::TypeError };
    return adoptRef(*new CSSTransformValue(WTFMove(transforms)));
}

RefPtr<CSSTransformComponent> CSSTransformValue::item(size_t index)
{
    return index < m_components.size() ? m_components[index].ptr() : nullptr;
}

ExceptionOr<Ref<CSSTransformComponent>> CSSTransformValue::setItem(size_t index, Ref<CSSTransformComponent>&& value)
{
    if (index > m_components.size())
        return Exception { ExceptionCode::RangeError, makeString("Index "_s, index, " exceeds the range of CSSTransformValue."_s) };

    if (index == m_components.size())
        m_components.append(WTFMove(value));
    else
        m_components[index] = WTFMove(value);

    return Ref<CSSTransformComponent> { m_components[index] };
}

bool CSSTransformValue::is2D() const
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-csstransformvalue-is2d
    return WTF::allOf(m_components, [] (auto& component) {
        return component->is2D();
    });
}

ExceptionOr<Ref<DOMMatrix>> CSSTransformValue::toMatrix()
{
    auto matrix = TransformationMatrix();
    auto is2D = DOMMatrixReadOnly::Is2D::Yes;

    for (auto component : m_components) {
        auto componentMatrixOrException = component->toMatrix();
        if (componentMatrixOrException.hasException())
            return componentMatrixOrException.releaseException();
        auto componentMatrix = componentMatrixOrException.returnValue();
        if (!componentMatrix->is2D())
            is2D = DOMMatrixReadOnly::Is2D::No;
        matrix.multiply(componentMatrix->transformationMatrix());
    }

    return DOMMatrix::create(WTFMove(matrix), is2D);
}

CSSTransformValue::CSSTransformValue(Vector<Ref<CSSTransformComponent>>&& transforms)
    : m_components(WTFMove(transforms))
{
}

CSSTransformValue::~CSSTransformValue() = default;

void CSSTransformValue::serialize(StringBuilder& builder, OptionSet<SerializationArguments>) const
{
    // https://drafts.css-houdini.org/css-typed-om/#serialize-a-csstransformvalue
    builder.append(interleave(m_components, [](auto& builder, auto& transform) { transform->serialize(builder); }, ' '));
}

RefPtr<CSSValue> CSSTransformValue::toCSSValue() const
{
    CSSValueListBuilder builder;
    for (auto& component : m_components) {
        if (auto cssComponent = component->toCSSValue())
            builder.append(cssComponent.releaseNonNull());
    }
    return CSSTransformListValue::create(WTFMove(builder));
}

} // namespace WebCore
