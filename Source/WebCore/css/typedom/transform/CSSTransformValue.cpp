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

#if ENABLE(CSS_TYPED_OM)

#include "CSSTransformComponent.h"
#include "DOMMatrix.h"
#include "ExceptionOr.h"
#include <wtf/Algorithms.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSTransformValue);

ExceptionOr<Ref<CSSTransformValue>> CSSTransformValue::create(Vector<RefPtr<CSSTransformComponent>>&& transforms)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-csstransformvalue-csstransformvalue
    if (transforms.isEmpty())
        return Exception { TypeError };
    return adoptRef(*new CSSTransformValue(WTFMove(transforms)));
}

ExceptionOr<RefPtr<CSSTransformComponent>> CSSTransformValue::item(size_t index)
{
    if (index >= m_components.size())
        return Exception { RangeError, makeString("Index ", index, " exceeds the range of CSSTransformValue.") };

    return RefPtr<CSSTransformComponent> { m_components[index] };
}

ExceptionOr<RefPtr<CSSTransformComponent>> CSSTransformValue::setItem(size_t index, Ref<CSSTransformComponent>&& value)
{
    if (index > m_components.size())
        return Exception { RangeError, makeString("Index ", index, " exceeds the range of CSSTransformValue.") };

    if (index == m_components.size())
        m_components.append(WTFMove(value));
    else
        m_components[index] = WTFMove(value);

    return RefPtr<CSSTransformComponent> { m_components[index] };
}

bool CSSTransformValue::is2D() const
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-csstransformvalue-is2d
    return WTF::allOf(m_components, [] (auto& component) {
        return component && component->is2D();
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

CSSTransformValue::CSSTransformValue(Vector<RefPtr<CSSTransformComponent>>&& transforms)
    : m_components(WTFMove(transforms))
{
}

void CSSTransformValue::serialize(StringBuilder& builder, OptionSet<SerializationArguments>) const
{
    // https://drafts.css-houdini.org/css-typed-om/#serialize-a-csstransformvalue
    for (size_t i = 0; i < m_components.size(); ++i) {
        if (i)
            builder.append(' ');
        m_components[i]->serialize(builder);
    }
}

} // namespace WebCore

#endif
