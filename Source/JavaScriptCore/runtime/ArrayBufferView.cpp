/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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
#include "ArrayBufferView.h"

#include "DataView.h"
#include "TypedArrayInlines.h"

namespace JSC {

ArrayBufferView::ArrayBufferView(TypedArrayType type, RefPtr<ArrayBuffer>&& buffer, size_t byteOffset, std::optional<size_t> byteLength)
    : m_type(type)
    , m_isResizableNonShared(buffer->isResizableNonShared())
    , m_isGrowableShared(buffer->isGrowableShared())
    , m_isAutoLength(buffer->isResizableOrGrowableShared() && !byteLength)
    , m_byteOffset(byteOffset)
    , m_byteLength(byteLength.value_or(0))
    , m_buffer(WTFMove(buffer))
{
    if (byteLength) {
        // If it is resizable, then it can be possible that length exceeds byteLength, and this is fine since it just becomes OOB array.
        if (!isResizableOrGrowableShared()) {
            Checked<size_t, CrashOnOverflow> length(byteOffset);
            length += byteLength.value();
            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(length <= m_buffer->byteLength());
        }
    } else
        ASSERT(isAutoLength());

    if (m_buffer)
        m_baseAddress = BaseAddress(static_cast<char*>(m_buffer->data()) + m_byteOffset, m_byteLength);
}

template<typename Visitor> constexpr decltype(auto) ArrayBufferView::visitDerived(Visitor&& visitor)
{
    switch (m_type) {
    case TypedArrayType::NotTypedArray:
    case TypedArrayType::TypeDataView:
        return std::invoke(std::forward<Visitor>(visitor), static_cast<DataView&>(*this));
#define DECLARE_TYPED_ARRAY_TYPE(name) \
    case TypedArrayType::Type##name: \
        return std::invoke(std::forward<Visitor>(visitor), static_cast<name##Array&>(*this));
    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(DECLARE_TYPED_ARRAY_TYPE)
#undef DECLARE_TYPED_ARRAY_TYPE
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename Visitor> constexpr decltype(auto) ArrayBufferView::visitDerived(Visitor&& visitor) const
{
    return const_cast<ArrayBufferView&>(*this).visitDerived([&](auto& value) {
        return std::invoke(std::forward<Visitor>(visitor), std::as_const(value));
    });
}

JSArrayBufferView* ArrayBufferView::wrap(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject)
{
    return visitDerived([&](auto& derived) { return derived.wrapImpl(lexicalGlobalObject, globalObject); });
}

void ArrayBufferView::operator delete(ArrayBufferView* value, std::destroying_delete_t)
{
    value->visitDerived([](auto& value) {
        using T = std::decay_t<decltype(value)>;
        std::destroy_at(&value);
        T::freeAfterDestruction(&value);
    });
}

void ArrayBufferView::setDetachable(bool flag)
{
    if (flag == m_isDetachable)
        return;
    
    m_isDetachable = flag;
    
    if (!m_buffer)
        return;
    
    if (flag)
        m_buffer->unpin();
    else
        m_buffer->pin();
}

} // namespace JSC
