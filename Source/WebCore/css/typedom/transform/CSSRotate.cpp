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
#include "CSSRotate.h"

#if ENABLE(CSS_TYPED_OM)

#include "DOMMatrix.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSRotate);

Ref<CSSRotate> CSSRotate::create(CSSNumberish&& x, CSSNumberish&& y, CSSNumberish&& z, Ref<CSSNumericValue>&& angle)
{
    return adoptRef(*new CSSRotate(WTFMove(x), WTFMove(y), WTFMove(z), WTFMove(angle)));
}

Ref<CSSRotate> CSSRotate::create(Ref<CSSNumericValue>&& angle)
{
    return adoptRef(*new CSSRotate(0.0, 0.0, 0.0, WTFMove(angle)));
}

CSSRotate::CSSRotate(CSSNumberish&& x, CSSNumberish&& y, CSSNumberish&& z, Ref<CSSNumericValue>&& angle)
    : m_x(WTFMove(x))
    , m_y(WTFMove(y))
    , m_z(WTFMove(z))
    , m_angle(WTFMove(angle))
{
}

// FIXME: Fix all the following virtual functions

String CSSRotate::toString() const
{
    return emptyString();
}

ExceptionOr<Ref<DOMMatrix>> CSSRotate::toMatrix()
{
    return DOMMatrix::fromMatrix(DOMMatrixInit { });
}

} // namespace WebCore

#endif
