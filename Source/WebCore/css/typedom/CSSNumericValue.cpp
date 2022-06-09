/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "CSSNumericValue.h"

#include "CSSMathSum.h"
#include "CSSNumericFactory.h"
#include "CSSNumericType.h"
#include "CSSUnitValue.h"
#include "ExceptionOr.h"

#if ENABLE(CSS_TYPED_OM)

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSNumericValue);

Ref<CSSNumericValue> CSSNumericValue::add(FixedVector<CSSNumberish>&& values)
{
    UNUSED_PARAM(values);
    // FIXME: add impl.

    return *this;
}

Ref<CSSNumericValue> CSSNumericValue::sub(FixedVector<CSSNumberish>&& values)
{
    UNUSED_PARAM(values);
    // FIXME: add impl.

    return *this;
}

Ref<CSSNumericValue> CSSNumericValue::mul(FixedVector<CSSNumberish>&& values)
{
    UNUSED_PARAM(values);
    // FIXME: add impl.

    return *this;
}

Ref<CSSNumericValue> CSSNumericValue::div(FixedVector<CSSNumberish>&& values)
{
    UNUSED_PARAM(values);
    // FIXME: add impl.

    return *this;
}
Ref<CSSNumericValue> CSSNumericValue::min(FixedVector<CSSNumberish>&& values)
{
    UNUSED_PARAM(values);
    // FIXME: add impl.

    return *this;
}
Ref<CSSNumericValue> CSSNumericValue::max(FixedVector<CSSNumberish>&& values)
{
    UNUSED_PARAM(values);
    // FIXME: add impl.

    return *this;
}

Ref<CSSNumericValue> CSSNumericValue::rectifyNumberish(CSSNumberish&& numberish)
{
    return WTF::switchOn(numberish, [](RefPtr<CSSNumericValue>& value) {
        RELEASE_ASSERT(!!value);
        return Ref<CSSNumericValue> { *value };
    }, [](double value) {
        return Ref<CSSNumericValue> { CSSNumericFactory::number(value) };
    });
}

bool CSSNumericValue::equals(FixedVector<CSSNumberish>&& value)
{
    UNUSED_PARAM(value);
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-equals
    // FIXME: add impl.
    return false;
}

Ref<CSSUnitValue> CSSNumericValue::to(String&& unit)
{
    UNUSED_PARAM(unit);
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-to
    // FIXME: add impl.
    return CSSUnitValue::create(1.0, "number");
}

Ref<CSSMathSum> CSSNumericValue::toSum(FixedVector<String>&& units)
{
    UNUSED_PARAM(units);
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-tosum
    // FIXME: add impl.
    return CSSMathSum::create({ 1.0 });
}

CSSNumericType CSSNumericValue::type()
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-type
    // FIXME: add impl.
    return CSSNumericType { };
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::parse(String&& cssText)
{
    UNUSED_PARAM(cssText);
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-parse
    // FIXME: add impl.
    return Exception { NotSupportedError, "Not implemented Error" };
}

} // namespace WebCore

#endif
