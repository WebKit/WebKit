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
#include "CBORReader.h"

#if ENABLE(WEB_AUTHN)

#include "CBORBinary.h"
#include <limits>
#include <utility>

namespace cbor {

namespace {

CBORValue::Type getMajorType(uint8_t initialDataByte)
{
    return static_cast<CBORValue::Type>((initialDataByte & constants::kMajorTypeMask) >> constants::kMajorTypeBitShift);
}

uint8_t getAdditionalInfo(uint8_t initialDataByte)
{
    return initialDataByte & constants::kAdditionalInformationMask;
}

// Error messages that correspond to each of the error codes.
const char kNoError[] = "Successfully deserialized to a CBOR value.";
const char kUnsupportedMajorType[] = "Unsupported major type.";
const char kUnknownAdditionalInfo[] = "Unknown additional info format in the first byte.";
const char kIncompleteCBORData[] = "Prematurely terminated CBOR data byte array.";
const char kIncorrectMapKeyType[] = "Map keys other than utf-8 encoded strings are not allowed.";
const char kTooMuchNesting[] = "Too much nesting.";
const char kInvalidUTF8[] = "String encoding other than utf8 are not allowed.";
const char kExtraneousData[] = "Trailing data bytes are not allowed.";
const char kDuplicateKey[] = "Duplicate map keys are not allowed.";
const char kMapKeyOutOfOrder[] = "Map keys must be sorted by byte length and then by byte-wise lexical order.";
const char kNonMinimalCBOREncoding[] = "Unsigned integers must be encoded with minimum number of bytes.";
const char kUnsupportedSimpleValue[] = "Unsupported or unassigned simple value.";
const char kUnsupportedFloatingPointValue[] = "Floating point numbers are not supported.";
const char kOutOfRangeIntegerValue[] = "Integer values must be between INT64_MIN and INT64_MAX.";

} // namespace

CBORReader::CBORReader(Bytes::const_iterator it, Bytes::const_iterator end)
    : m_it(it)
    , m_end(end)
    , m_errorCode(DecoderError::CBORNoError)
{
}

CBORReader::~CBORReader()
{
}

// static
Optional<CBORValue> CBORReader::read(const Bytes& data, DecoderError* errorCodeOut, int maxNestingLevel)
{
    CBORReader reader(data.begin(), data.end());
    Optional<CBORValue> decodedCbor = reader.decodeCBOR(maxNestingLevel);

    if (decodedCbor)
        reader.checkExtraneousData();
    if (errorCodeOut)
        *errorCodeOut = reader.getErrorCode();

    if (reader.getErrorCode() != DecoderError::CBORNoError)
        return WTF::nullopt;
    return decodedCbor;
}

Optional<CBORValue> CBORReader::decodeCBOR(int maxNestingLevel)
{
    if (maxNestingLevel < 0 || maxNestingLevel > kCBORMaxDepth) {
        m_errorCode = DecoderError::TooMuchNesting;
        return WTF::nullopt;
    }

    if (!canConsume(1)) {
        m_errorCode = DecoderError::IncompleteCBORData;
        return WTF::nullopt;
    }

    const uint8_t initialByte = *m_it++;
    const auto major_type = getMajorType(initialByte);
    const uint8_t additionalInfo = getAdditionalInfo(initialByte);

    uint64_t value;
    if (!readVariadicLengthInteger(additionalInfo, &value))
        return WTF::nullopt;

    switch (major_type) {
    case CBORValue::Type::Unsigned:
        return decodeValueToUnsigned(value);
    case CBORValue::Type::Negative:
        return decodeValueToNegative(value);
    case CBORValue::Type::ByteString:
        return readBytes(value);
    case CBORValue::Type::String:
        return readString(value);
    case CBORValue::Type::Array:
        return readCBORArray(value, maxNestingLevel);
    case CBORValue::Type::Map:
        return readCBORMap(value, maxNestingLevel);
    case CBORValue::Type::SimpleValue:
        return readSimpleValue(additionalInfo, value);
    case CBORValue::Type::None:
        break;
    }

    m_errorCode = DecoderError::UnsupportedMajorType;
    return WTF::nullopt;
}

bool CBORReader::readVariadicLengthInteger(uint8_t additionalInfo, uint64_t* value)
{
    uint8_t additionalBytes = 0;
    if (additionalInfo < 24) {
        *value = additionalInfo;
        return true;
    }

    if (additionalInfo == 24)
        additionalBytes = 1;
    else if (additionalInfo == 25)
        additionalBytes = 2;
    else if (additionalInfo == 26)
        additionalBytes = 4;
    else if (additionalInfo == 27)
        additionalBytes = 8;
    else {
        m_errorCode = DecoderError::UnknownAdditionalInfo;
        return false;
    }

    if (!canConsume(additionalBytes)) {
        m_errorCode = DecoderError::IncompleteCBORData;
        return false;
    }

    uint64_t intData = 0;
    for (uint8_t i = 0; i < additionalBytes; ++i) {
        intData <<= 8;
        intData |= *m_it++;
    }

    *value = intData;
    return checkMinimalEncoding(additionalBytes, intData);
}

Optional<CBORValue> CBORReader::decodeValueToNegative(uint64_t value)
{
    if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        m_errorCode = DecoderError::OutOfRangeIntegerValue;
        return WTF::nullopt;
    }
    return CBORValue(-static_cast<int64_t>(value) - 1);
}

Optional<CBORValue> CBORReader::decodeValueToUnsigned(uint64_t value)
{
    if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        m_errorCode = DecoderError::OutOfRangeIntegerValue;
        return WTF::nullopt;
    }
    return CBORValue(static_cast<int64_t>(value));
}

Optional<CBORValue> CBORReader::readSimpleValue(uint8_t additionalInfo, uint64_t value)
{
    // Floating point numbers are not supported.
    if (additionalInfo > 24 && additionalInfo < 28) {
        m_errorCode = DecoderError::UnsupportedFloatingPointValue;
        return WTF::nullopt;
    }

    ASSERT(value <= 255u);
    CBORValue::SimpleValue possiblyUnsupportedSimpleValue = static_cast<CBORValue::SimpleValue>(static_cast<int>(value));
    switch (possiblyUnsupportedSimpleValue) {
    case CBORValue::SimpleValue::FalseValue:
    case CBORValue::SimpleValue::TrueValue:
    case CBORValue::SimpleValue::NullValue:
    case CBORValue::SimpleValue::Undefined:
        return CBORValue(possiblyUnsupportedSimpleValue);
    }

    m_errorCode = DecoderError::UnsupportedSimpleValue;
    return WTF::nullopt;
}

Optional<CBORValue> CBORReader::readString(uint64_t numBytes)
{
    if (!canConsume(numBytes)) {
        m_errorCode = DecoderError::IncompleteCBORData;
        return WTF::nullopt;
    }

    ASSERT(numBytes <= std::numeric_limits<size_t>::max());
    String cborString = String::fromUTF8(m_it, static_cast<size_t>(numBytes));
    m_it += numBytes;

    // Invalid UTF8 bytes produce an empty WTFString.
    // Not to confuse it with an actual empty WTFString.
    if (!numBytes || hasValidUTF8Format(cborString))
        return CBORValue(WTFMove(cborString));
    return WTF::nullopt;
}

Optional<CBORValue> CBORReader::readBytes(uint64_t numBytes)
{
    if (!canConsume(numBytes)) {
        m_errorCode = DecoderError::IncompleteCBORData;
        return WTF::nullopt;
    }

    Bytes cborByteString;
    ASSERT(numBytes <= std::numeric_limits<size_t>::max());
    cborByteString.append(m_it, static_cast<size_t>(numBytes));
    m_it += numBytes;

    return CBORValue(WTFMove(cborByteString));
}

Optional<CBORValue> CBORReader::readCBORArray(uint64_t length, int maxNestingLevel)
{
    CBORValue::ArrayValue cborArray;
    while (length-- > 0) {
        Optional<CBORValue> cborElement = decodeCBOR(maxNestingLevel - 1);
        if (!cborElement)
            return WTF::nullopt;
        cborArray.append(WTFMove(cborElement.value()));
    }
    return CBORValue(WTFMove(cborArray));
}

Optional<CBORValue> CBORReader::readCBORMap(uint64_t length, int maxNestingLevel)
{
    CBORValue::MapValue cborMap;
    while (length-- > 0) {
        Optional<CBORValue> key = decodeCBOR(maxNestingLevel - 1);
        Optional<CBORValue> value = decodeCBOR(maxNestingLevel - 1);
        if (!key || !value)
            return WTF::nullopt;

        // Only CBOR maps with integer or string type keys are allowed.
        if (!key->isString() && !key->isInteger()) {
            m_errorCode = DecoderError::IncorrectMapKeyType;
            return WTF::nullopt;
        }
        if (!checkDuplicateKey(key.value(), cborMap) || !checkOutOfOrderKey(key.value(), cborMap))
            return WTF::nullopt;

        cborMap.emplace(std::make_pair(WTFMove(key.value()), WTFMove(value.value())));
    }
    return CBORValue(WTFMove(cborMap));
}

bool CBORReader::canConsume(uint64_t bytes)
{
    if (static_cast<uint64_t>(std::distance(m_it, m_end)) >= bytes)
        return true;
    m_errorCode = DecoderError::IncompleteCBORData;
    return false;
}

bool CBORReader::checkMinimalEncoding(uint8_t additionalBytes, uint64_t uintData)
{
    if ((additionalBytes == 1 && uintData < 24) || uintData <= (1ULL << 8 * (additionalBytes >> 1)) - 1) {
        m_errorCode = DecoderError::NonMinimalCBOREncoding;
        return false;
    }
    return true;
}

void CBORReader::checkExtraneousData()
{
    if (m_it != m_end)
        m_errorCode = DecoderError::ExtraneousData;
}

bool CBORReader::checkDuplicateKey(const CBORValue& newKey, const CBORValue::MapValue& map)
{
    if (map.find(newKey) != map.end()) {
        m_errorCode = DecoderError::DuplicateKey;
        return false;
    }
    return true;
}

bool CBORReader::hasValidUTF8Format(const String& stringData)
{
    // Invalid UTF8 bytes produce an empty WTFString.
    if (stringData.isEmpty()) {
        m_errorCode = DecoderError::InvalidUTF8;
        return false;
    }
    return true;
}

bool CBORReader::checkOutOfOrderKey(const CBORValue& newKey, const CBORValue::MapValue& map)
{
    auto comparator = map.key_comp();
    if (!map.empty() && comparator(newKey, map.rbegin()->first)) {
        m_errorCode = DecoderError::OutOfOrderKey;
        return false;
    }
    return true;
}

CBORReader::DecoderError CBORReader::getErrorCode()
{
    return m_errorCode;
}

// static
const char* CBORReader::errorCodeToString(DecoderError error)
{
    switch (error) {
    case DecoderError::CBORNoError:
        return kNoError;
    case DecoderError::UnsupportedMajorType:
        return kUnsupportedMajorType;
    case DecoderError::UnknownAdditionalInfo:
        return kUnknownAdditionalInfo;
    case DecoderError::IncompleteCBORData:
        return kIncompleteCBORData;
    case DecoderError::IncorrectMapKeyType:
        return kIncorrectMapKeyType;
    case DecoderError::TooMuchNesting:
        return kTooMuchNesting;
    case DecoderError::InvalidUTF8:
        return kInvalidUTF8;
    case DecoderError::ExtraneousData:
        return kExtraneousData;
    case DecoderError::DuplicateKey:
        return kDuplicateKey;
    case DecoderError::OutOfOrderKey:
        return kMapKeyOutOfOrder;
    case DecoderError::NonMinimalCBOREncoding:
        return kNonMinimalCBOREncoding;
    case DecoderError::UnsupportedSimpleValue:
        return kUnsupportedSimpleValue;
    case DecoderError::UnsupportedFloatingPointValue:
        return kUnsupportedFloatingPointValue;
    case DecoderError::OutOfRangeIntegerValue:
        return kOutOfRangeIntegerValue;
    default:
        ASSERT_NOT_REACHED();
        return "Unknown error code.";
    }
}

} // namespace cbor

#endif // ENABLE(WEB_AUTHN)
