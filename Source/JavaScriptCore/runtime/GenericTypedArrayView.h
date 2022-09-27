/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#pragma once

#include "ArrayBuffer.h"
#include "ArrayBufferView.h"

namespace JSC {

template<typename Adaptor>
class GenericTypedArrayView final : public ArrayBufferView {
public:
    static Ref<GenericTypedArrayView> create(size_t length);
    static Ref<GenericTypedArrayView> create(const typename Adaptor::Type* array, size_t length);
    static Ref<GenericTypedArrayView> create(RefPtr<ArrayBuffer>&&, size_t byteOffset, size_t length);
    static RefPtr<GenericTypedArrayView> tryCreate(size_t length);
    static RefPtr<GenericTypedArrayView> tryCreate(const typename Adaptor::Type* array, size_t length);
    static RefPtr<GenericTypedArrayView> tryCreate(RefPtr<ArrayBuffer>&&, size_t byteOffset, size_t length);
    
    static Ref<GenericTypedArrayView> createUninitialized(size_t length);
    static RefPtr<GenericTypedArrayView> tryCreateUninitialized(size_t length);
    
    typename Adaptor::Type* data() const { return static_cast<typename Adaptor::Type*>(baseAddress()); }
    
    bool set(GenericTypedArrayView<Adaptor>* array, size_t offset)
    {
        return setImpl(array, offset * sizeof(typename Adaptor::Type));
    }
    
    bool setRange(const typename Adaptor::Type* data, size_t count, size_t offset)
    {
        return setRangeImpl(
            reinterpret_cast<const char*>(data),
            count * sizeof(typename Adaptor::Type),
            offset * sizeof(typename Adaptor::Type));
    }
    
    bool zeroRange(size_t offset, size_t count)
    {
        return zeroRangeImpl(offset * sizeof(typename Adaptor::Type), count * sizeof(typename Adaptor::Type));
    }
    
    void zeroFill() { zeroRange(0, length()); }
    
    size_t length() const
    {
        if (isDetached())
            return 0;
        return byteLength() / sizeof(typename Adaptor::Type);
    }

    typename Adaptor::Type item(size_t index) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(index < this->length());
        return data()[index];
    }
    
    void set(size_t index, double value) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(index < this->length());
        data()[index] = Adaptor::toNativeFromDouble(value);
    }

    void setNative(size_t index, typename Adaptor::Type value) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(index < this->length());
        data()[index] = value;
    }

    bool getRange(typename Adaptor::Type* data, size_t count, size_t offset)
    {
        return getRangeImpl(
            reinterpret_cast<char*>(data),
            count * sizeof(typename Adaptor::Type),
            offset * sizeof(typename Adaptor::Type));
    }

    bool checkInboundData(size_t offset, size_t count) const
    {
        return isSumSmallerThanOrEqual(offset, count, this->length());
    }

    JSArrayBufferView* wrapImpl(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject);

private:
    GenericTypedArrayView(RefPtr<ArrayBuffer>&&, size_t byteOffset, size_t length);
};

} // namespace JSC
