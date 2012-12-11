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

#ifndef ArgumentEncoder_h
#define ArgumentEncoder_h

#include "ArgumentCoder.h"
#include "Attachment.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/TypeTraits.h>
#include <wtf/Vector.h>

namespace CoreIPC {

class ArgumentEncoder;
class DataReference;

class ArgumentEncoder {
public:
    static PassOwnPtr<ArgumentEncoder> create();
    virtual ~ArgumentEncoder();

    void encodeFixedLengthData(const uint8_t*, size_t, unsigned alignment);
    void encodeVariableLengthByteArray(const DataReference&);

    void encode(bool);
    void encode(uint16_t);
    void encode(uint32_t);
    void encode(uint64_t);
    void encode(int32_t);
    void encode(int64_t);
    void encode(float);
    void encode(double);

    template<typename T> void encodeEnum(T t)
    {
        COMPILE_ASSERT(sizeof(T) <= sizeof(uint64_t), enum_type_must_not_be_larger_than_64_bits);

        encode(static_cast<uint64_t>(t));
    }

    template<bool B, typename T = void>
    struct EnableIf { };

    template<typename T>
    struct EnableIf<true, T> { typedef T Type; };
    
    template<typename T> class UsesDeprecatedEncodeFunction {
        typedef char YesType;
        struct NoType {
            char padding[8];
        };

        static YesType checkEncode(void (*)(ArgumentEncoder*, const T&));
        static NoType checkEncode(...);

    public:
        static const bool value = sizeof(checkEncode(ArgumentCoder<T>::encode)) == sizeof(YesType);
    };

    // FIXME: This is the function that gets chosen if the argument coder takes the ArgumentEncoder as a pointer.
    // This is the deprecated form - get rid of it.
    template<typename T> void encode(const T& t, typename EnableIf<UsesDeprecatedEncodeFunction<T>::value>::Type* = 0)
    {
        ArgumentCoder<T>::encode(this, t);
    }

    template<typename T> void encode(const T& t, typename EnableIf<!UsesDeprecatedEncodeFunction<T>::value>::Type* = 0)
    {
        ArgumentCoder<T>::encode(*this, t);
    }

    template<typename T> ArgumentEncoder& operator<<(const T& t)
    {
        encode(t);
        return *this;
    }

    uint8_t* buffer() { return usesInlineBuffer() ? m_inlineBuffer : m_buffer; }
    size_t bufferSize() const { return m_bufferSize; }

    void addAttachment(const Attachment&);
    Vector<Attachment> releaseAttachments();

protected:
    ArgumentEncoder();

private:
    static const size_t inlineBufferSize = 4096;
    bool usesInlineBuffer() const { return m_bufferCapacity <= inlineBufferSize; }
    uint8_t* grow(unsigned alignment, size_t size);
    
    uint8_t* m_buffer;
    
    size_t m_bufferSize;
    size_t m_bufferCapacity;

    Vector<Attachment> m_attachments;

    uint8_t m_inlineBuffer[inlineBufferSize];
};

} // namespace CoreIPC

#endif // ArgumentEncoder_h
