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
#include "CSSScale.h"

#if ENABLE(CSS_TYPED_OM)

#include "CSSUnitValue.h"
#include "CSSUnits.h"
#include "DOMMatrix.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSScale);

ExceptionOr<Ref<CSSScale>> CSSScale::create(CSSNumberish x, CSSNumberish y, std::optional<CSSNumberish> z)
{
    auto rectifiedX = CSSNumericValue::rectifyNumberish(WTFMove(x));
    auto rectifiedY = CSSNumericValue::rectifyNumberish(WTFMove(y));
    auto rectifiedZ = z ? CSSNumericValue::rectifyNumberish(WTFMove(*z)) : Ref<CSSNumericValue> { CSSUnitValue::create(1.0, CSSUnitType::CSS_NUMBER) };

    // https://drafts.css-houdini.org/css-typed-om/#dom-cssscale-cssscale
    if (!rectifiedX->type().matchesNumber()
        || !rectifiedY->type().matchesNumber()
        || (!rectifiedZ->type().matchesNumber()))
        return Exception { TypeError };

    return adoptRef(*new CSSScale(z ? Is2D::No : Is2D::Yes, WTFMove(rectifiedX), WTFMove(rectifiedY), WTFMove(rectifiedZ)));
}

CSSScale::CSSScale(CSSTransformComponent::Is2D is2D, Ref<CSSNumericValue> x, Ref<CSSNumericValue> y, Ref<CSSNumericValue> z)
    : CSSTransformComponent(is2D)
    , m_x(WTFMove(x))
    , m_y(WTFMove(y))
    , m_z(WTFMove(z))
{
}

void CSSScale::setX(CSSNumberish x)
{
    m_x = CSSNumericValue::rectifyNumberish(WTFMove(x));
}

void CSSScale::setY(CSSNumberish y)
{
    m_y = CSSNumericValue::rectifyNumberish(WTFMove(y));
}

void CSSScale::setZ(CSSNumberish z)
{
    m_z = CSSNumericValue::rectifyNumberish(WTFMove(z));
}

void CSSScale::serialize(StringBuilder& builder) const
{
    // https://drafts.css-houdini.org/css-typed-om/#serialize-a-cssscale
    builder.append(is2D() ? "scale(" : "scale3d(");
    m_x->serialize(builder);
    builder.append(", ");
    m_y->serialize(builder);
    if (!is2D()) {
        builder.append(", ");
        m_z->serialize(builder);
    }
    builder.append(')');
}

ExceptionOr<Ref<DOMMatrix>> CSSScale::toMatrix()
{
    // FIXME: Implement.
    return DOMMatrix::fromMatrix(DOMMatrixInit { });
}

} // namespace WebCore

#endif
