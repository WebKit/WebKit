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

#include "DOMMatrix.h"
#include "DOMMatrixInit.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSMatrixComponent);

Ref<CSSTransformComponent> CSSMatrixComponent::create(Ref<DOMMatrixReadOnly>&& matrix, CSSMatrixComponentOptions&& options)
{
    return adoptRef(*new CSSMatrixComponent(WTFMove(matrix), WTFMove(options)));
}

CSSMatrixComponent::CSSMatrixComponent(Ref<DOMMatrixReadOnly>&& matrix, CSSMatrixComponentOptions&& options)
    : m_matrix(matrix->cloneAsDOMMatrix())
    , m_options(WTFMove(options))
{
}

// FIXME: Fix all the following virtual functions

String CSSMatrixComponent::toString() const
{
    return emptyString();
}

ExceptionOr<Ref<DOMMatrix>> CSSMatrixComponent::toMatrix()
{
    return DOMMatrix::fromMatrix(DOMMatrixInit { });
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
