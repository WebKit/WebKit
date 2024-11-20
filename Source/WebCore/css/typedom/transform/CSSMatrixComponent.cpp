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

#include "CSSFunctionValue.h"
#include "CSSNumericFactory.h"
#include "CSSPrimitiveValue.h"
#include "CSSStyleValueFactory.h"
#include "CSSUnitValue.h"
#include "DOMMatrix.h"
#include "DOMMatrixInit.h"
#include "ExceptionOr.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSMatrixComponent);

Ref<CSSTransformComponent> CSSMatrixComponent::create(Ref<DOMMatrixReadOnly>&& matrix, CSSMatrixComponentOptions&& options)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssmatrixcomponent-cssmatrixcomponent
    auto is2D = options.is2D.value_or(matrix->is2D());
    return adoptRef(*new CSSMatrixComponent(WTFMove(matrix), is2D ? Is2D::Yes : Is2D::No));
}

template<typename T, typename F> static ExceptionOr<Ref<CSSTransformComponent>> makeMatrix(const T& matrix, F&& create)
{
    Vector<double> components;
    for (const auto& number : matrix.value) {
        auto raw = number.raw();
        if (!raw)
            return Exception { ExceptionCode::TypeError, "Expected a CSSUnitValue."_s };
        components.append(raw->value);
    }
    return create(WTFMove(components));
};

ExceptionOr<Ref<CSSTransformComponent>> CSSMatrixComponent::create(CSS::Matrix matrix)
{
    return makeMatrix(matrix, [](Vector<double>&& components) {
        auto domMatrix = DOMMatrixReadOnly::create({
            components[0],
            components[1],
            components[2],
            components[3],
            components[4],
            components[5]
        }, DOMMatrixReadOnly::Is2D::Yes);
        return CSSMatrixComponent::create(WTFMove(domMatrix));
    });
}

ExceptionOr<Ref<CSSTransformComponent>> CSSMatrixComponent::create(CSS::Matrix3D matrix)
{
    return makeMatrix(matrix, [](Vector<double>&& components) {
        auto domMatrix = DOMMatrixReadOnly::create({
            components[0],  components[1],  components[2],  components[3],
            components[4],  components[5],  components[6],  components[7],
            components[8],  components[9],  components[10], components[11],
            components[12], components[13], components[14], components[15]
        }, DOMMatrixReadOnly::Is2D::No);
        return CSSMatrixComponent::create(WTFMove(domMatrix));
    });
}

CSSMatrixComponent::CSSMatrixComponent(Ref<DOMMatrixReadOnly>&& matrix, Is2D is2D)
    : CSSTransformComponent(is2D)
    , m_matrix(matrix->cloneAsDOMMatrix())
{
}

void CSSMatrixComponent::serialize(StringBuilder& builder) const
{
    if (is2D()) {
        builder.append("matrix("_s, m_matrix->a(), ", "_s,
        m_matrix->b(), ", "_s, m_matrix->c(), ", "_s,
        m_matrix->d(), ", "_s, m_matrix->e(), ", "_s,
        m_matrix->f(), ')');
    } else {
        builder.append("matrix3d("_s, m_matrix->m11(), ", "_s,
        m_matrix->m12(), ", "_s, m_matrix->m13(), ", "_s,
        m_matrix->m14(), ", "_s, m_matrix->m21(), ", "_s,
        m_matrix->m22(), ", "_s, m_matrix->m23(), ", "_s,
        m_matrix->m24(), ", "_s, m_matrix->m31(), ", "_s,
        m_matrix->m32(), ", "_s, m_matrix->m33(), ", "_s,
        m_matrix->m34(), ", "_s, m_matrix->m41(), ", "_s,
        m_matrix->m42(), ", "_s, m_matrix->m43(), ", "_s,
        m_matrix->m44(), ')');
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

std::optional<CSS::TransformFunction> CSSMatrixComponent::toCSS() const
{
    if (is2D()) {
        return CSS::TransformFunction {
            CSS::MatrixFunction { {
                .value = {
                    CSS::NumberRaw<> { m_matrix->a() },
                    CSS::NumberRaw<> { m_matrix->b() },
                    CSS::NumberRaw<> { m_matrix->c() },
                    CSS::NumberRaw<> { m_matrix->d() },
                    CSS::NumberRaw<> { m_matrix->e() },
                    CSS::NumberRaw<> { m_matrix->f() },
                }
            } }
        };
    }

    return CSS::TransformFunction {
        CSS::Matrix3DFunction { {
            .value = {
                CSS::NumberRaw<> { m_matrix->m11() }, CSS::NumberRaw<> { m_matrix->m12() }, CSS::NumberRaw<> { m_matrix->m13() }, CSS::NumberRaw<> { m_matrix->m14() },
                CSS::NumberRaw<> { m_matrix->m21() }, CSS::NumberRaw<> { m_matrix->m22() }, CSS::NumberRaw<> { m_matrix->m23() }, CSS::NumberRaw<> { m_matrix->m24() },
                CSS::NumberRaw<> { m_matrix->m31() }, CSS::NumberRaw<> { m_matrix->m32() }, CSS::NumberRaw<> { m_matrix->m33() }, CSS::NumberRaw<> { m_matrix->m34() },
                CSS::NumberRaw<> { m_matrix->m41() }, CSS::NumberRaw<> { m_matrix->m42() }, CSS::NumberRaw<> { m_matrix->m43() }, CSS::NumberRaw<> { m_matrix->m44() },
            }
        } }
    };
}

} // namespace WebCore
