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

#pragma once

#include "APIObject.h"
#include <wtf/Function.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#endif

OBJC_CLASS NSData;

namespace API {

class Data : public ObjectImpl<API::Object::Type::Data> {
public:
    using FreeDataFunction = WTF::Function<void()>;

    static Ref<Data> createWithoutCopying(std::span<const uint8_t> bytes, FreeDataFunction&& freeDataFunction)
    {
        return adoptRef(*new Data(bytes, WTFMove(freeDataFunction)));
    }

    static Ref<Data> create(std::span<const uint8_t> bytes)
    {
        MallocSpan<uint8_t> copiedBytes;

        if (!bytes.empty()) {
            copiedBytes = MallocSpan<uint8_t>::malloc(bytes.size_bytes());
            memcpySpan(copiedBytes.mutableSpan(), bytes);
        }

        auto data = copiedBytes.span();
        return createWithoutCopying(data, [copiedBytes = WTFMove(copiedBytes)] () { });
    }
    
    static Ref<Data> create(const Vector<unsigned char>& buffer)
    {
        return create(buffer.span());
    }

    static Ref<Data> create(Vector<unsigned char>&& vector)
    {
        auto buffer = vector.releaseBuffer();
        auto span = buffer.span();
        return createWithoutCopying(span, [buffer = WTFMove(buffer)] { });
    }

#if PLATFORM(COCOA)
    static Ref<Data> createWithoutCopying(RetainPtr<NSData>);
#endif

    ~Data()
    {
        m_freeDataFunction();
    }

    size_t size() const { return m_span.size(); }
    std::span<const uint8_t> span() const { return m_span; }

private:
    Data(std::span<const uint8_t> span, FreeDataFunction&& freeDataFunction)
        : m_span(span)
        , m_freeDataFunction(WTFMove(freeDataFunction))
    {
    }

    std::span<const uint8_t> m_span;
    FreeDataFunction m_freeDataFunction;
};

} // namespace API

SPECIALIZE_TYPE_TRAITS_API_OBJECT(Data);
