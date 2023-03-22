/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSNumericArray.h"

#include "ExceptionOr.h"
#include <wtf/FixedVector.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSNumericArray);

Ref<CSSNumericArray> CSSNumericArray::create(FixedVector<CSSNumberish>&& numberishes)
{
    return adoptRef(*new CSSNumericArray(WTF::map(WTFMove(numberishes), CSSNumericValue::rectifyNumberish)));
}

Ref<CSSNumericArray> CSSNumericArray::create(Vector<Ref<CSSNumericValue>>&& values)
{
    return adoptRef(*new CSSNumericArray(WTFMove(values)));
}

CSSNumericArray::CSSNumericArray(Vector<Ref<CSSNumericValue>>&& values)
    : m_array(WTFMove(values))
{
}

CSSNumericArray::CSSNumericArray(FixedVector<Ref<CSSNumericValue>>&& values)
    : m_array(WTFMove(values))
{
}

RefPtr<CSSNumericValue> CSSNumericArray::item(size_t index)
{
    if (index >= m_array.size())
        return nullptr;
    return m_array[index].copyRef();
}

void CSSNumericArray::forEach(Function<void(const CSSNumericValue&, bool first)> function)
{
    for (size_t i = 0; i < m_array.size(); ++i)
        function(m_array[i], !i);
}

} // namespace WebCore
