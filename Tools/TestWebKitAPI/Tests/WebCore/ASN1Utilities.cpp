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
#include "ASN1Utilities.h"

#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

namespace ASN1 {

struct Object {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    virtual size_t encodedLengthBytes() const = 0;
    virtual void serializeTo(Vector<uint8_t>&) const = 0;
    virtual ~Object() = default;
protected:
    size_t sizeSerializedSize(size_t size) const
    {
        if (size <= 0x7F)
            return 1;
        ASSERT(size <= std::numeric_limits<uint16_t>::max());
        return 3;
    }
    void serializeSize(Vector<uint8_t>& vector, size_t size) const
    {
        ASSERT(size <= std::numeric_limits<uint16_t>::max());
        if (size > 0x7F) {
            vector.append(0x82);
            vector.append(static_cast<uint8_t>(size >> 8));
        }
        vector.append(static_cast<uint8_t>(size));
    }
};

struct ObjectIdentifier : Object {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    enum class Type : uint8_t {
        RsaEncryption,
        Rsapss,
        Sha384,
        Pkcs1MGF
    };
    ObjectIdentifier(Type type)
        : type(type) { }
private:
    const Type type;
    size_t encodedLengthBytes() const final { return 11; }
    void serializeTo(Vector<uint8_t>& vector) const final
    {
        vector.append(0x6);
        auto oidBytes = bytes();
        vector.append(oidBytes.size());
        vector.append(std::span { oidBytes });
    }
    std::array<uint8_t, 9> bytes() const
    {
        switch (type) {
        case Type::RsaEncryption:
            return { 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01 };
        case Type::Rsapss:
            return { 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0A };
        case Type::Sha384:
            return { 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02 };
        case Type::Pkcs1MGF:
            return { 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x08 };
        }
    }
};

struct Sequence : Object {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    template<typename... Types>
    static UniqueRef<Sequence> create(Types... types)
    {
        return makeUniqueRef<Sequence>(Vector<UniqueRef<Object>>::from(std::forward<Types>(types)...));
    }
    Sequence(Vector<UniqueRef<Object>>&& elements)
        : elements(WTFMove(elements)) { }
private:
    const Vector<UniqueRef<Object>> elements;
    size_t encodedLengthBytes() const final
    {
        size_t elementBytes = elementEncodedLengthBytes();
        return 1 + sizeSerializedSize(elementBytes) + elementBytes;
    }
    void serializeTo(Vector<uint8_t>& vector) const final
    {
        vector.append(0x30);
        serializeSize(vector, elementEncodedLengthBytes());
        for (const auto& object : elements)
            object->serializeTo(vector);
    }
    size_t elementEncodedLengthBytes() const
    {
        size_t total = 0;
        for (const auto& element : elements)
            total += element->encodedLengthBytes();
        return total;
    }
};

struct Null : Object {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
private:
    size_t encodedLengthBytes() const final { return 2; }
    void serializeTo(Vector<uint8_t>& vector) const final
    {
        vector.append(0x05);
        vector.append(0x00);
    }
};

struct IndexWrapper : Object {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    IndexWrapper(uint8_t index, UniqueRef<Object>&& wrapped)
        : index(index), wrapped(WTFMove(wrapped)) { }
private:
    const uint8_t index;
    UniqueRef<Object> wrapped;
    size_t encodedLengthBytes() const final
    {
        size_t wrappedBytes = wrapped->encodedLengthBytes();
        return 1 + sizeSerializedSize(wrappedBytes) + wrappedBytes;
    }
    void serializeTo(Vector<uint8_t>& vector) const final
    {
        switch (index) {
        case 0:
            vector.append(0xa0);
            break;
        case 1:
            vector.append(0xa1);
            break;
        case 2:
            vector.append(0xa2);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        serializeSize(vector, wrapped->encodedLengthBytes());
        wrapped->serializeTo(vector);
    }
};

struct Integer : Object {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    Integer(uint8_t value)
        : value(value) { }
private:
    const uint8_t value;
    size_t encodedLengthBytes() const final
    {
        return 3;
    }
    void serializeTo(Vector<uint8_t>& vector) const final
    {
        vector.append(0x02);
        vector.append(0x01);
        vector.append(value);
    }
};

struct BitString : Object {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    BitString(Vector<uint8_t>&& bytes)
        : bytes(WTFMove(bytes)) { }
private:
    const Vector<uint8_t> bytes;
    size_t encodedLengthBytes() const final
    {
        size_t byteSize = bytes.size();
        return 2 + sizeSerializedSize(byteSize) + byteSize;
    }
    void serializeTo(Vector<uint8_t>& vector) const final
    {
        vector.append(0x03);
        serializeSize(vector, bytes.size() + 1);
        vector.append(0x00);
        vector.appendVector(bytes);
    }
};

} // namespace ASN1

namespace TestWebKitAPI {

Vector<uint8_t> wrapPublicKeyWithRSAPSSOID(Vector<uint8_t>&& publicKey)
{
#if HAVE(RSA_PSS_OID)
    auto sequence = ASN1::Sequence::create(
        ASN1::Sequence::create(
            makeUniqueRef<ASN1::ObjectIdentifier>(ASN1::ObjectIdentifier::Type::Rsapss),
            ASN1::Sequence::create(
                makeUniqueRef<ASN1::IndexWrapper>(0, ASN1::Sequence::create(
                    makeUniqueRef<ASN1::ObjectIdentifier>(ASN1::ObjectIdentifier::Type::Sha384)
                )),
                makeUniqueRef<ASN1::IndexWrapper>(1, ASN1::Sequence::create(
                    makeUniqueRef<ASN1::ObjectIdentifier>(ASN1::ObjectIdentifier::Type::Pkcs1MGF),
                    ASN1::Sequence::create(
                        makeUniqueRef<ASN1::ObjectIdentifier>(ASN1::ObjectIdentifier::Type::Sha384)
                    )
                )),
                makeUniqueRef<ASN1::IndexWrapper>(2, makeUniqueRef<ASN1::Integer>(48))
            )
        ),
        makeUniqueRef<ASN1::BitString>(WTFMove(publicKey))
    );
#else
    auto sequence = ASN1::Sequence::create(
        ASN1::Sequence::create(
            makeUniqueRef<ASN1::ObjectIdentifier>(ASN1::ObjectIdentifier::Type::RsaEncryption),
            makeUniqueRef<ASN1::Null>()
        ),
        makeUniqueRef<ASN1::BitString>(WTFMove(publicKey))
    );
#endif

    Vector<uint8_t> wrapped;
    static_cast<ASN1::Object&>(sequence.get()).serializeTo(wrapped);
    return wrapped;
}

}
