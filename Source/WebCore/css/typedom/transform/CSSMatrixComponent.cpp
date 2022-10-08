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
#include "CSSMatrixComponent.h"

#if ENABLE(CSS_TYPED_OM)

#include "CSSFunctionValue.h"
#include "CSSNumericFactory.h"
#include "CSSStyleValueFactory.h"
#include "CSSUnitValue.h"
#include "DOMMatrix.h"
#include "DOMMatrixInit.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSMatrixComponent);

Ref<CSSTransformComponent> CSSMatrixComponent::create(Ref<DOMMatrixReadOnly>&& matrix, CSSMatrixComponentOptions&& options)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssmatrixcomponent-cssmatrixcomponent
    auto is2D = options.is2D.value_or(matrix->is2D());
    return adoptRef(*new CSSMatrixComponent(WTFMove(matrix), is2D ? Is2D::Yes : Is2D::No));
}

ExceptionOr<Ref<CSSTransformComponent>> CSSMatrixComponent::create(CSSFunctionValue& cssFunctionValue)
{
    auto makeMatrix = [&](const Function<Ref<CSSTransformComponent>(Vector<double>&&)>& create, size_t expectedNumberOfComponents) -> ExceptionOr<Ref<CSSTransformComponent>> {
        Vector<double> components;
        for (auto componentCSSValue : cssFunctionValue) {
            auto valueOrException = CSSStyleValueFactory::reifyValue(componentCSSValue);
            if (valueOrException.hasException())
                return valueOrException.releaseException();
            if (!is<CSSUnitValue>(valueOrException.returnValue()))
                return Exception { TypeError, "Expected a CSSUnitValue."_s };
            components.append(downcast<CSSUnitValue>(valueOrException.releaseReturnValue().get()).value());
        }
        if (components.size() != expectedNumberOfComponents) {
            ASSERT_NOT_REACHED();
            return Exception { TypeError, "Unexpected number of values."_s };
        }
        return create(WTFMove(components));
    };

    switch (cssFunctionValue.name()) {
    case CSSValueMatrix:
        return makeMatrix([](Vector<double>&& components) {
            auto domMatrix = DOMMatrixReadOnly::create({ components[0], components[1], components[2], components[3], components[4], components[5] }, DOMMatrixReadOnly::Is2D::Yes);
            return CSSMatrixComponent::create(WTFMove(domMatrix));
        }, 6);
    case CSSValueMatrix3d:
        return makeMatrix([](Vector<double>&& components) {
            auto domMatrix = DOMMatrixReadOnly::create({
                components[0], components[1], components[2], components[3],
                components[4], components[5], components[6], components[7],
                components[8], components[9], components[10], components[11],
                components[12], components[13], components[14], components[15]
            }, DOMMatrixReadOnly::Is2D::No);
            return CSSMatrixComponent::create(WTFMove(domMatrix));
        }, 16);
    default:
        ASSERT_NOT_REACHED();
        auto domMatrix = DOMMatrixReadOnly::create({ }, DOMMatrixReadOnly::Is2D::Yes);
        return { CSSMatrixComponent::create(WTFMove(domMatrix)) };
    }
}

CSSMatrixComponent::CSSMatrixComponent(Ref<DOMMatrixReadOnly>&& matrix, Is2D is2D)
    : CSSTransformComponent(is2D)
    , m_matrix(matrix->cloneAsDOMMatrix())
{
}

void CSSMatrixComponent::serialize(StringBuilder& builder) const
{
    if (is2D()) {
        builder.append("matrix(");
        builder.append(String::number(m_matrix->a()));
        builder.append(", ");
        builder.append(String::number(m_matrix->b()));
        builder.append(", ");
        builder.append(String::number(m_matrix->c()));
        builder.append(", ");
        builder.append(String::number(m_matrix->d()));
        builder.append(", ");
        builder.append(String::number(m_matrix->e()));
        builder.append(", ");
        builder.append(String::number(m_matrix->f()));
        builder.append(")");
    } else {
        builder.append("matrix3d(");
        builder.append(String::number(m_matrix->m11()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m12()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m13()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m14()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m21()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m22()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m23()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m24()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m31()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m32()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m33()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m34()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m41()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m42()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m43()));
        builder.append(", ");
        builder.append(String::number(m_matrix->m44()));
        builder.append(")");
    }
}

ExceptionOr<Ref<DOMMatrix>> CSSMatrixComponent::toMatrix()
{
    if (!is2D())
        return { m_matrix.get() };

    // Flatten to 2d.
    return { DOMMatrix::create({
        m_matrix->a(),
        m_matrix->b(),
        m_matrix->c(),
        m_matrix->d(),
        m_matrix->e(),
        m_matrix->f() }, DOMMatrixReadOnly::Is2D::Yes) };
}

DOMMatrix& CSSMatrixComponent::matrix()
{
    return m_matrix.get();
}

void CSSMatrixComponent::setMatrix(Ref<DOMMatrix>&& matrix)
{
    m_matrix = WTFMove(matrix);
}

} // namespace WebCore

#endif
