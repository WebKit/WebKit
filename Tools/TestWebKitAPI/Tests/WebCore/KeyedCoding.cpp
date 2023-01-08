/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#include <WebCore/KeyedCoding.h>
#include <WebCore/SharedBuffer.h>
#include <cstdint>
#include <cstdlib>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

static bool checkDecodedBytes(const uint8_t* original, size_t originalSize, const uint8_t* decoded, size_t decodedSize)
{
    if (originalSize != decodedSize)
        return false;

    return !std::memcmp(original, decoded, originalSize);
}

TEST(KeyedCoding, SetAndGetBytes)
{
    const uint8_t data[] = { 0x00, 0x01, 0x02, 0x03, 0xde, 0xad, 0xbe, 0xef };
    const size_t dataLength = std::size(data);
    const size_t dataLengthOne = 1;
    const size_t dataLengthZero = 0;

    auto encoder = WebCore::KeyedEncoder::encoder();
    encoder->encodeBytes("data"_s, data, dataLength);
    encoder->encodeBytes("data-size0"_s, data, dataLengthZero);
    encoder->encodeBytes("data-size1"_s, data, dataLengthOne);
    auto encodedBuffer = encoder->finishEncoding();

    auto decoder = WebCore::KeyedDecoder::decoder(reinterpret_cast<const uint8_t*>(encodedBuffer->data()), encodedBuffer->size());

    bool success;
    const uint8_t* buffer;
    size_t size;

    success = decoder->decodeBytes("data"_s, buffer, size);
    EXPECT_TRUE(success);
    EXPECT_EQ(dataLength, size);
    EXPECT_TRUE(checkDecodedBytes(data, dataLength, buffer, size));

    success = decoder->decodeBytes("data-size0"_s, buffer, size);
    EXPECT_TRUE(success);
    EXPECT_EQ(dataLengthZero, size);

    success = decoder->decodeBytes("data-size1"_s, buffer, size);
    EXPECT_TRUE(success);
    EXPECT_EQ(dataLengthOne, size);
    EXPECT_TRUE(checkDecodedBytes(data, dataLengthOne, buffer, size));
}

template <typename EncodeFunctionType, typename DecodeValueType, typename EncodeValueType>
bool testSimpleValue(EncodeFunctionType encode, bool (WebCore::KeyedDecoder::* decode)(const String&, DecodeValueType&), EncodeValueType value)
{
    auto encoder = WebCore::KeyedEncoder::encoder();
    (encoder.get()->*encode)("key"_s, value);

    auto encodedBuffer = encoder->finishEncoding();
    auto decoder = WebCore::KeyedDecoder::decoder(reinterpret_cast<const uint8_t*>(encodedBuffer->data()), encodedBuffer->size());

    DecodeValueType decodedValue;
    bool success = (decoder.get()->*decode)("key"_s, decodedValue);

    return success && decodedValue == value;
}

TEST(KeyedCoding, SetAndGetBool)
{
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeBool, &WebCore::KeyedDecoder::decodeBool, true));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeBool, &WebCore::KeyedDecoder::decodeBool, false));
}

TEST(KeyedCoding, SetAndGetInt32)
{
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeInt32, &WebCore::KeyedDecoder::decodeInt32, 0));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeInt32, &WebCore::KeyedDecoder::decodeInt32, 1));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeInt32, &WebCore::KeyedDecoder::decodeInt32, -1));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeInt32, &WebCore::KeyedDecoder::decodeInt32, INT_MIN));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeInt32, &WebCore::KeyedDecoder::decodeInt32, INT_MAX));
}

TEST(KeyedCoding, SetAndGetUInt32)
{
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeUInt32, &WebCore::KeyedDecoder::decodeUInt32, uint32_t(0)));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeUInt32, &WebCore::KeyedDecoder::decodeUInt32, uint32_t(1)));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeUInt32, &WebCore::KeyedDecoder::decodeUInt32, UINT32_MAX));
}

TEST(KeyedCoding, SetAndGetInt64)
{
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeInt64, &WebCore::KeyedDecoder::decodeInt64, int64_t(0)));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeInt64, &WebCore::KeyedDecoder::decodeInt64, int64_t(-2)));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeInt64, &WebCore::KeyedDecoder::decodeInt64, int64_t(2)));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeInt64, &WebCore::KeyedDecoder::decodeInt64, INT64_MIN));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeInt64, &WebCore::KeyedDecoder::decodeInt64, INT64_MAX));
}

TEST(KeyedCoding, SetAndGetUInt64)
{
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeUInt64, &WebCore::KeyedDecoder::decodeUInt64, uint64_t(0)));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeUInt64, &WebCore::KeyedDecoder::decodeUInt64, uint64_t(1)));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeUInt64, &WebCore::KeyedDecoder::decodeUInt64, UINT64_MAX));
}

TEST(KeyedCoding, SetAndGetFloat)
{
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeFloat, &WebCore::KeyedDecoder::decodeFloat, float(0)));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeFloat, &WebCore::KeyedDecoder::decodeFloat, float(1.5)));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeFloat, &WebCore::KeyedDecoder::decodeFloat, float(-1.5)));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeFloat, &WebCore::KeyedDecoder::decodeFloat, std::numeric_limits<float>::lowest()));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeFloat, &WebCore::KeyedDecoder::decodeFloat, std::numeric_limits<float>::min()));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeFloat, &WebCore::KeyedDecoder::decodeFloat, std::numeric_limits<float>::max()));
}

TEST(KeyedCoding, SetAndGetDouble)
{
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeDouble, &WebCore::KeyedDecoder::decodeDouble, double(0)));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeDouble, &WebCore::KeyedDecoder::decodeDouble, double(1.25)));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeDouble, &WebCore::KeyedDecoder::decodeDouble, double(-1.25)));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeDouble, &WebCore::KeyedDecoder::decodeDouble, std::numeric_limits<double>::lowest()));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeDouble, &WebCore::KeyedDecoder::decodeDouble, std::numeric_limits<double>::min()));
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeDouble, &WebCore::KeyedDecoder::decodeDouble, std::numeric_limits<double>::max()));
}

TEST(KeyedCoding, SetAndGetString)
{
    EXPECT_TRUE(testSimpleValue(&WebCore::KeyedEncoder::encodeString, &WebCore::KeyedDecoder::decodeString, String("WebKit"_s)));
}

TEST(KeyedCoding, GetNonExistingRecord)
{
    auto encoder = WebCore::KeyedEncoder::encoder();
    encoder->encodeBool("bool-true"_s, true);

    auto encodedBuffer = encoder->finishEncoding();
    auto decoder = WebCore::KeyedDecoder::decoder(reinterpret_cast<const uint8_t*>(encodedBuffer->data()), encodedBuffer->size());

    bool success, boolValue;
    success = decoder->decodeBool("bool-true"_s, boolValue);
    EXPECT_TRUE(success);
    EXPECT_EQ(true, boolValue);

    success = decoder->decodeBool("no-exist-key"_s, boolValue);
    EXPECT_FALSE(success);

    int32_t int32Value;
    success = decoder->decodeInt32("no-exist-key"_s, int32Value);
    EXPECT_FALSE(success);

    uint32_t uint32Value;
    success = decoder->decodeUInt32("no-exist-key"_s, uint32Value);
    EXPECT_FALSE(success);

    int64_t int64Value;
    success = decoder->decodeInt64("no-exist-key"_s, int64Value);
    EXPECT_FALSE(success);

    uint64_t uint64Value;
    success = decoder->decodeUInt64("no-exist-key"_s, uint64Value);
    EXPECT_FALSE(success);

    float floatValue;
    success = decoder->decodeFloat("no-exist-key"_s, floatValue);
    EXPECT_FALSE(success);

    double doubleValue;
    success = decoder->decodeDouble("no-exist-key"_s, doubleValue);
    EXPECT_FALSE(success);

    const uint8_t* buffer;
    size_t size;
    success = decoder->decodeBytes("no-exist-key"_s, buffer, size);
    EXPECT_FALSE(success);
}

struct KeyedCodingTestObject {
    static void encode(WebCore::KeyedEncoder& encoder, const KeyedCodingTestObject& object)
    {
        encoder.encodeString("name"_s, object.m_name);
        encoder.encodeInt32("age"_s, object.m_age);
    }

    static bool decode(WebCore::KeyedDecoder& decoder, KeyedCodingTestObject& object)
    {
        if (!decoder.decodeString("name"_s, object.m_name))
            return false;
        if (!decoder.decodeInt32("age"_s, object.m_age))
            return false;
        return true;
    }

    KeyedCodingTestObject() { };
    KeyedCodingTestObject(String name, int age)
        : m_name(name)
        , m_age(age)
    {
    }

    bool equals(const KeyedCodingTestObject& other) const
    {
        return (m_name == other.m_name) && (m_age == other.m_age);
    }

    String m_name;
    int32_t m_age;
};

static bool operator==(const KeyedCodingTestObject& lObject, const KeyedCodingTestObject& rObject)
{
    return lObject.equals(rObject);
}

TEST(KeyedCoding, SetAndGetObject)
{
    const KeyedCodingTestObject user0("Noah"_s, 28);
    const KeyedCodingTestObject user1("Emma"_s, 31);

    auto encoder = WebCore::KeyedEncoder::encoder();
    encoder->encodeObject("user0"_s, user0, KeyedCodingTestObject::encode);
    encoder->encodeObject("user1"_s, user1, KeyedCodingTestObject::encode);
    auto encodedBuffer = encoder->finishEncoding();

    auto decoder = WebCore::KeyedDecoder::decoder(reinterpret_cast<const uint8_t*>(encodedBuffer->data()), encodedBuffer->size());
    bool success;
    KeyedCodingTestObject decodedUser0, decodedUser1;

    success = decoder->decodeObject("user0"_s, decodedUser0, KeyedCodingTestObject::decode);
    EXPECT_TRUE(success);
    EXPECT_EQ(decodedUser0, user0);

    success = decoder->decodeObject("user1"_s, decodedUser1, KeyedCodingTestObject::decode);
    EXPECT_TRUE(success);
    EXPECT_EQ(decodedUser1, user1);
}

TEST(KeyedCoding, SetAndGetObjects)
{
    Vector<KeyedCodingTestObject> users;
    for (int i = 0; i < 10; i++)
        users.append(KeyedCodingTestObject("username"_s, i));

    auto encoder = WebCore::KeyedEncoder::encoder();
    encoder->encodeObjects("users"_s, users.begin(), users.end(), KeyedCodingTestObject::encode);
    auto encodedBuffer = encoder->finishEncoding();

    auto decoder = WebCore::KeyedDecoder::decoder(reinterpret_cast<const uint8_t*>(encodedBuffer->data()), encodedBuffer->size());
    Vector<KeyedCodingTestObject> decodedUsers;

    bool success = decoder->decodeObjects("users"_s, decodedUsers, KeyedCodingTestObject::decode);
    EXPECT_TRUE(success);
    EXPECT_EQ(users, decodedUsers);
}

TEST(KeyedCoding, SetAndGetWithEmptyKey)
{
    auto encoder = WebCore::KeyedEncoder::encoder();
    encoder->encodeBool(emptyString(), false);

    auto encodedBuffer = encoder->finishEncoding();
    auto decoder = WebCore::KeyedDecoder::decoder(reinterpret_cast<const uint8_t*>(encodedBuffer->data()), encodedBuffer->size());

    bool success, boolValue;
    success = decoder->decodeBool(emptyString(), boolValue);

    EXPECT_TRUE(success);
    EXPECT_EQ(false, boolValue);
}

static Vector<uint8_t> generateRandomData()
{
    Vector<uint8_t> data;
    for (auto i = 0; i < 256; ++i)
        data.append(std::rand() / (RAND_MAX + 1.0) * std::numeric_limits<uint8_t>::max());
    return data;
}

TEST(KeyedCoding, DecodeRandomData)
{
    std::srand(0);
    for (auto i = 0; i < 10; ++i) {
        auto data = generateRandomData();
        // Don't crash.
        WebCore::KeyedDecoder::decoder(data.data(), data.size());
    }
}

}
