/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "APIArray.h"

#include "APIString.h"

namespace API {

Ref<Array> Array::create()
{
    return create(Vector<RefPtr<Object>>());
}

Ref<Array> Array::create(Vector<RefPtr<Object>>&& elements)
{
    return adoptRef(*new Array(WTFMove(elements)));
}

Ref<Array> Array::createStringArray(const Vector<WTF::String>& strings)
{
    Vector<RefPtr<Object>> elements;
    elements.reserveInitialCapacity(strings.size());

    for (const auto& string : strings)
        elements.uncheckedAppend(API::String::create(string));

    return create(WTFMove(elements));
}

Vector<WTF::String> Array::toStringVector()
{
    Vector<WTF::String> patternsVector;

    size_t size = this->size();
    if (!size)
        return patternsVector;

    patternsVector.reserveInitialCapacity(size);
    for (auto entry : elementsOfType<API::String>())
        patternsVector.uncheckedAppend(entry->string());
    return patternsVector;
}

Ref<API::Array> Array::copy()
{
    size_t size = this->size();
    if (!size)
        return Array::create();

    Vector<RefPtr<Object>> elements;
    elements.reserveInitialCapacity(size);
    for (const auto& entry : this->elements())
        elements.uncheckedAppend(entry);
    return Array::create(WTFMove(elements));
}

Array::~Array()
{
}

} // namespace API
