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
#include "CSSSkew.h"

#if ENABLE(CSS_TYPED_OM)

#include "CSSUnitValue.h"
#include "DOMMatrix.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSSkew);

ExceptionOr<Ref<CSSSkew>> CSSSkew::create(Ref<CSSNumericValue> ax, Ref<CSSNumericValue> ay)
{
    if (!ax->type().matches<CSSNumericBaseType::Angle>()
        || !ay->type().matches<CSSNumericBaseType::Angle>())
        return Exception { TypeError };
    return adoptRef(*new CSSSkew(WTFMove(ax), WTFMove(ay)));
}

CSSSkew::CSSSkew(Ref<CSSNumericValue> ax, Ref<CSSNumericValue> ay)
    : CSSTransformComponent(Is2D::Yes)
    , m_ax(WTFMove(ax))
    , m_ay(WTFMove(ay))
{
}

void CSSSkew::serialize(StringBuilder& builder) const
{
    // https://drafts.css-houdini.org/css-typed-om/#serialize-a-cssskew
    builder.append("skew(");
    m_ax->serialize(builder);
    if (!is<CSSUnitValue>(m_ay) || downcast<CSSUnitValue>(m_ay.get()).value()) {
        builder.append(", ");
        m_ay->serialize(builder);
    }
    builder.append(')');
}

ExceptionOr<Ref<DOMMatrix>> CSSSkew::toMatrix()
{
    // FIXME: Implement.
    return DOMMatrix::fromMatrix(DOMMatrixInit { });
}

} // namespace WebCore

#endif
