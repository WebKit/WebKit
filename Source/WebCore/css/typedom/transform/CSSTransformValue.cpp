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
#include "CSSTransformList.h"
#include "CSSTransformPropertyValue.h"
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

static ExceptionOr<Ref<CSSTransformComponent>> createTransformComponent(const CSS::IsTransformFunction auto& function)
{
    auto makeTransformComponent = [&](auto exceptionOrTransformComponent) -> ExceptionOr<Ref<CSSTransformComponent>> {
        if (exceptionOrTransformComponent.hasException())
            return exceptionOrTransformComponent.releaseException();
        return Ref<CSSTransformComponent> { exceptionOrTransformComponent.releaseReturnValue() };
    };

    return WTF::switchOn(function,
        [&](const CSS::MatrixFunction& value) {
            return makeTransformComponent(CSSMatrixComponent::create(*value));
        },
        [&](const CSS::Matrix3DFunction& value) {
            return makeTransformComponent(CSSMatrixComponent::create(*value));
        },
        [&](const CSS::RotateFunction& value) {
            return makeTransformComponent(CSSRotate::create(*value));
        },
        [&](const CSS::Rotate3DFunction& value) {
            return makeTransformComponent(CSSRotate::create(*value));
        },
        [&](const CSS::RotateXFunction& value) {
            return makeTransformComponent(CSSRotate::create(*value));
        },
        [&](const CSS::RotateYFunction& value) {
            return makeTransformComponent(CSSRotate::create(*value));
        },
        [&](const CSS::RotateZFunction& value) {
            return makeTransformComponent(CSSRotate::create(*value));
        },
        [&](const CSS::SkewFunction& value) {
            return makeTransformComponent(CSSSkew::create(*value));
        },
        [&](const CSS::SkewXFunction& value) {
            return makeTransformComponent(CSSSkewX::create(*value));
        },
        [&](const CSS::SkewYFunction& value) {
            return makeTransformComponent(CSSSkewY::create(*value));
        },
        [&](const CSS::ScaleFunction& value) {
            return makeTransformComponent(CSSScale::create(*value));
        },
        [&](const CSS::Scale3DFunction& value) {
            return makeTransformComponent(CSSScale::create(*value));
        },
        [&](const CSS::ScaleXFunction& value) {
            return makeTransformComponent(CSSScale::create(*value));
        },
        [&](const CSS::ScaleYFunction& value) {
            return makeTransformComponent(CSSScale::create(*value));
        },
        [&](const CSS::ScaleZFunction& value) {
            return makeTransformComponent(CSSScale::create(*value));
        },
        [&](const CSS::TranslateFunction& value) {
            return makeTransformComponent(CSSTranslate::create(*value));
        },
        [&](const CSS::Translate3DFunction& value) {
            return makeTransformComponent(CSSTranslate::create(*value));
        },
        [&](const CSS::TranslateXFunction& value) {
            return makeTransformComponent(CSSTranslate::create(*value));
        },
        [&](const CSS::TranslateYFunction& value) {
            return makeTransformComponent(CSSTranslate::create(*value));
        },
        [&](const CSS::TranslateZFunction& value) {
            return makeTransformComponent(CSSTranslate::create(*value));
        },
        [&](const CSS::PerspectiveFunction& value) {
            return makeTransformComponent(CSSPerspective::create(*value));
        }
    );
}

ExceptionOr<Ref<CSSTransformValue>> CSSTransformValue::create(const CSS::TransformList& list)
{
    Vector<Ref<CSSTransformComponent>> components;
    for (auto function : list) {
        auto component = createTransformComponent(function);
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
    CSS::TransformList::List::VariantList functions;
    for (auto& component : m_components) {
        if (auto cssComponent = component->toCSS())
            WTF::switchOn(WTFMove(*cssComponent), [&](auto&& alternative) { functions.append(WTFMove(alternative)); });
    }
    return CSSTransformPropertyValue::create(CSS::TransformProperty {
        CSS::TransformList { { WTFMove(functions) } }
    });
}

} // namespace WebCore
