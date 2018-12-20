// Copyright 2017 The Chromium Authors. All rights reserved.
// Copyright (C) 2018 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "CBORWriter.h"

#if ENABLE(WEB_AUTHN)

#include "CBORBinary.h"

namespace cbor {

CBORWriter::~CBORWriter()
{
}

// static
Optional<Vector<uint8_t>> CBORWriter::write(const CBORValue& node, size_t maxNestingLevel)
{
    Vector<uint8_t> cbor;
    CBORWriter writer(&cbor);
    if (writer.encodeCBOR(node, static_cast<int>(maxNestingLevel)))
        return cbor;
    return WTF::nullopt;
}

CBORWriter::CBORWriter(Vector<uint8_t>* cbor)
    : m_encodedCBOR(cbor)
{
}

bool CBORWriter::encodeCBOR(const CBORValue& node, int maxNestingLevel)
{
    if (maxNestingLevel < 0)
        return false;

    switch (node.type()) {
    case CBORValue::Type::None: {
        startItem(CBORValue::Type::ByteString, 0);
        return true;
    }
    // Represents unsigned integers.
    case CBORValue::Type::Unsigned: {
        int64_t value = node.getUnsigned();
        startItem(CBORValue::Type::Unsigned, static_cast<uint64_t>(value));
        return true;
    }
    // Represents negative integers.
    case CBORValue::Type::Negative: {
        int64_t value = node.getNegative();
        startItem(CBORValue::Type::Negative, static_cast<uint64_t>(-(value + 1)));
        return true;
    }
    // Represents a byte string.
    case CBORValue::Type::ByteString: {
        const CBORValue::BinaryValue& bytes = node.getByteString();
        startItem(CBORValue::Type::ByteString, static_cast<uint64_t>(bytes.size()));
        // Add the bytes.
        m_encodedCBOR->appendVector(bytes);
        return true;
    }
    case CBORValue::Type::String: {
        // WTFString uses UTF16 but RFC7049 requires UTF8
        auto utf8String = node.getString().utf8();
        startItem(CBORValue::Type::String, static_cast<uint64_t>(utf8String.length()));
        // Add the characters.
        m_encodedCBOR->append(utf8String.data(), utf8String.length());
        return true;
    }
    // Represents an array.
    case CBORValue::Type::Array: {
        const CBORValue::ArrayValue& array = node.getArray();
        startItem(CBORValue::Type::Array, array.size());
        for (const auto& value : array) {
            if (!encodeCBOR(value, maxNestingLevel - 1))
                return false;
        }
        return true;
    }
    // Represents a map.
    case CBORValue::Type::Map: {
        const CBORValue::MapValue& map = node.getMap();
        startItem(CBORValue::Type::Map, map.size());

        for (const auto& value : map) {
            if (!encodeCBOR(value.first, maxNestingLevel - 1))
                return false;
            if (!encodeCBOR(value.second, maxNestingLevel - 1))
                return false;
        }
        return true;
    }
    // Represents a simple value.
    case CBORValue::Type::SimpleValue: {
        const CBORValue::SimpleValue simpleValue = node.getSimpleValue();
        startItem(CBORValue::Type::SimpleValue, static_cast<uint64_t>(simpleValue));
        return true;
    }
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void CBORWriter::startItem(CBORValue::Type type, uint64_t size)
{
    m_encodedCBOR->append(static_cast<uint8_t>(static_cast<unsigned>(type) << constants::kMajorTypeBitShift));
    setUint(size);
}

void CBORWriter::setAdditionalInformation(uint8_t additionalInformation)
{
    ASSERT(!m_encodedCBOR->isEmpty());
    ASSERT((additionalInformation & constants::kAdditionalInformationMask) == additionalInformation);
    m_encodedCBOR->last() |= (additionalInformation & constants::kAdditionalInformationMask);
}

void CBORWriter::setUint(uint64_t value)
{
    size_t count = getNumUintBytes(value);
    int shift = -1;
    // Values under 24 are encoded directly in the initial byte.
    // Otherwise, the last 5 bits of the initial byte contains the length
    // of unsigned integer, which is encoded in following bytes.
    switch (count) {
    case 0:
        setAdditionalInformation(static_cast<uint8_t>(value));
        break;
    case 1:
        setAdditionalInformation(constants::kAdditionalInformation1Byte);
        shift = 0;
        break;
    case 2:
        setAdditionalInformation(constants::kAdditionalInformation2Bytes);
        shift = 1;
        break;
    case 4:
        setAdditionalInformation(constants::kAdditionalInformation4Bytes);
        shift = 3;
        break;
    case 8:
        setAdditionalInformation(constants::kAdditionalInformation8Bytes);
        shift = 7;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    for (; shift >= 0; shift--)
        m_encodedCBOR->append(0xFF & (value >> (shift * 8)));
}

size_t CBORWriter::getNumUintBytes(uint64_t value)
{
    if (value < 24)
        return 0;
    if (value <= 0xFF)
        return 1;
    if (value <= 0xFFFF)
        return 2;
    if (value <= 0xFFFFFFFF)
        return 4;
    return 8;
}

} // namespace cbor

#endif // ENABLE(WEB_AUTHN)
