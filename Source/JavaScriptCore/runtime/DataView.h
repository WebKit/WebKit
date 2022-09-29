/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ArrayBufferView.h"
#include <wtf/FlipBytes.h>

namespace JSC {

class DataView final : public ArrayBufferView {
public:
    JS_EXPORT_PRIVATE static Ref<DataView> create(RefPtr<ArrayBuffer>&&, size_t byteOffset, size_t length);
    static Ref<DataView> create(RefPtr<ArrayBuffer>&&);

    JSArrayBufferView* wrapImpl(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject);
    
    template<typename T>
    T get(size_t offset, bool littleEndian, bool* status = nullptr)
    {
        if (status) {
            if (offset + sizeof(T) > byteLength()) {
                *status = false;
                return T();
            }
            *status = true;
        } else
            RELEASE_ASSERT(offset + sizeof(T) <= byteLength());
        return flipBytesIfLittleEndian(
            *reinterpret_cast<T*>(static_cast<uint8_t*>(m_baseAddress.get(byteLength())) + offset),
            littleEndian);
    }
    
    template<typename T>
    T read(size_t& offset, bool littleEndian, bool* status = nullptr)
    {
        T result = this->template get<T>(offset, littleEndian, status);
        if (!status || *status)
            offset += sizeof(T);
        return result;
    }
    
    template<typename T>
    void set(size_t offset, T value, bool littleEndian, bool* status = nullptr)
    {
        if (status) {
            if (offset + sizeof(T) > byteLength()) {
                *status = false;
                return;
            }
            *status = true;
        } else
            RELEASE_ASSERT(offset + sizeof(T) <= byteLength());
        *reinterpret_cast<T*>(static_cast<uint8_t*>(m_baseAddress.get(byteLength())) + offset) =
            flipBytesIfLittleEndian(value, littleEndian);
    }

private:
    DataView(RefPtr<ArrayBuffer>&&, size_t byteOffset, size_t byteLength);
};

} // namespace JSC
