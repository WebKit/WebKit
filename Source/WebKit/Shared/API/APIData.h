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
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#endif

OBJC_CLASS NSData;

namespace API {

class Data : public ObjectImpl<API::Object::Type::Data> {
public:
    using FreeDataFunction = void (*)(uint8_t*, const void* context);

    static Ref<Data> createWithoutCopying(std::span<const uint8_t> bytes, FreeDataFunction freeDataFunction, const void* context)
    {
        return adoptRef(*new Data(bytes, freeDataFunction, context));
    }

    static Ref<Data> create(std::span<const uint8_t> bytes)
    {
        uint8_t* copiedBytes = nullptr;

        if (bytes.size()) {
            copiedBytes = static_cast<uint8_t*>(fastMalloc(bytes.size()));
            memcpy(copiedBytes, bytes.data(), bytes.size());
        }

        return createWithoutCopying({ copiedBytes, bytes.size() }, [] (uint8_t* bytes, const void*) {
            if (bytes)
                fastFree(static_cast<void*>(bytes));
        }, nullptr);
    }
    
    static Ref<Data> create(const Vector<unsigned char>& buffer)
    {
        return create(buffer.span());
    }

    static Ref<Data> create(Vector<unsigned char>&& buffer)
    {
        auto bufferSize = buffer.size();
        auto bufferPointer = buffer.releaseBuffer().leakPtr();
        return createWithoutCopying({ bufferPointer, bufferSize }, [] (uint8_t* bytes, const void*) {
            if (bytes)
                WTF::VectorMalloc::free(bytes);
        }, nullptr);
    }

#if PLATFORM(COCOA)
    static Ref<Data> createWithoutCopying(RetainPtr<NSData>);
#endif

    ~Data()
    {
        m_freeDataFunction(const_cast<uint8_t*>(m_span.data()), m_context);
    }

    size_t size() const { return m_span.size(); }
    std::span<const uint8_t> span() const { return m_span; }

private:
    Data(std::span<const uint8_t> span, FreeDataFunction freeDataFunction, const void* context)
        : m_span(span)
        , m_freeDataFunction(freeDataFunction)
        , m_context(context)
    {
    }

    std::span<const uint8_t> m_span;
    FreeDataFunction m_freeDataFunction { nullptr };
    const void* m_context { nullptr };
};

} // namespace API

SPECIALIZE_TYPE_TRAITS_API_OBJECT(Data);
