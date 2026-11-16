/*
 * Copyright (C) 2024 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

#include <wtf/text/Base64.h>

namespace TestWebKitAPI {

#define EXPECT_ENCODE(expected, input) EXPECT_EQ(expected, base64EncodeToString(input, options))
#define EXPECT_DECODE(expected, input) EXPECT_EQ(expected, base64DecodeToString(input, options))
// base64DecodeToString() returns a null String when the input cannot be decoded, and a null
// String does not compare equal to the empty String, so check emptiness rather than equality.
#define EXPECT_DECODE_EMPTY(input) EXPECT_TRUE(base64DecodeToString(input, options).isEmpty())

TEST(Base64, Encode)
{
    static constexpr OptionSet<Base64EncodeOption> options;

    EXPECT_ENCODE(""_s, byteCast<uint8_t>(""_span));
    EXPECT_ENCODE("Zg=="_s, byteCast<uint8_t>("f"_span));
    EXPECT_ENCODE("Zm8="_s, byteCast<uint8_t>("fo"_span));
    EXPECT_ENCODE("Zm9v"_s, byteCast<uint8_t>("foo"_span));
    EXPECT_ENCODE("Zm9vYg=="_s, byteCast<uint8_t>("foob"_span));
    EXPECT_ENCODE("Zm9vYmE="_s, byteCast<uint8_t>("fooba"_span));
    EXPECT_ENCODE("Zm9vYmFy"_s, byteCast<uint8_t>("foobar"_span));

    EXPECT_ENCODE("AA=="_s, Vector<uint8_t>({ 0 }));
    EXPECT_ENCODE("AQ=="_s, Vector<uint8_t>({ 1 }));
    EXPECT_ENCODE("gA=="_s, Vector<uint8_t>({ 128 }));
    EXPECT_ENCODE("/g=="_s, Vector<uint8_t>({ 254 }));
    EXPECT_ENCODE("/w=="_s, Vector<uint8_t>({ 255 }));
    EXPECT_ENCODE("AAE="_s, Vector<uint8_t>({ 0, 1 }));
    EXPECT_ENCODE("/v8="_s, Vector<uint8_t>({ 254, 255 }));
    EXPECT_ENCODE("AAGA/v8="_s, Vector<uint8_t>({ 0, 1, 128, 254, 255 }));
}

TEST(Base64, EncodeOmitPadding)
{
    static constexpr OptionSet<Base64EncodeOption> options = { Base64EncodeOption::OmitPadding };

    EXPECT_ENCODE(""_s, byteCast<uint8_t>(""_span));
    EXPECT_ENCODE("Zg"_s, byteCast<uint8_t>("f"_span));
    EXPECT_ENCODE("Zm8"_s, byteCast<uint8_t>("fo"_span));
    EXPECT_ENCODE("Zm9v"_s, byteCast<uint8_t>("foo"_span));
    EXPECT_ENCODE("Zm9vYg"_s, byteCast<uint8_t>("foob"_span));
    EXPECT_ENCODE("Zm9vYmE"_s, byteCast<uint8_t>("fooba"_span));
    EXPECT_ENCODE("Zm9vYmFy"_s, byteCast<uint8_t>("foobar"_span));

    EXPECT_ENCODE("AA"_s, Vector<uint8_t>({ 0 }));
    EXPECT_ENCODE("AQ"_s, Vector<uint8_t>({ 1 }));
    EXPECT_ENCODE("gA"_s, Vector<uint8_t>({ 128 }));
    EXPECT_ENCODE("/g"_s, Vector<uint8_t>({ 254 }));
    EXPECT_ENCODE("/w"_s, Vector<uint8_t>({ 255 }));
    EXPECT_ENCODE("AAE"_s, Vector<uint8_t>({ 0, 1 }));
    EXPECT_ENCODE("/v8"_s, Vector<uint8_t>({ 254, 255 }));
    EXPECT_ENCODE("AAGA/v8"_s, Vector<uint8_t>({ 0, 1, 128, 254, 255 }));
}

TEST(Base64, EncodeURL)
{
    static constexpr OptionSet<Base64EncodeOption> options = { Base64EncodeOption::URL };

    EXPECT_ENCODE(""_s, byteCast<uint8_t>(""_span));
    EXPECT_ENCODE("Zg=="_s, byteCast<uint8_t>("f"_span));
    EXPECT_ENCODE("Zm8="_s, byteCast<uint8_t>("fo"_span));
    EXPECT_ENCODE("Zm9v"_s, byteCast<uint8_t>("foo"_span));
    EXPECT_ENCODE("Zm9vYg=="_s, byteCast<uint8_t>("foob"_span));
    EXPECT_ENCODE("Zm9vYmE="_s, byteCast<uint8_t>("fooba"_span));
    EXPECT_ENCODE("Zm9vYmFy"_s, byteCast<uint8_t>("foobar"_span));

    EXPECT_ENCODE("AA=="_s, Vector<uint8_t>({ 0 }));
    EXPECT_ENCODE("AQ=="_s, Vector<uint8_t>({ 1 }));
    EXPECT_ENCODE("gA=="_s, Vector<uint8_t>({ 128 }));
    EXPECT_ENCODE("_g=="_s, Vector<uint8_t>({ 254 }));
    EXPECT_ENCODE("_w=="_s, Vector<uint8_t>({ 255 }));
    EXPECT_ENCODE("AAE="_s, Vector<uint8_t>({ 0, 1 }));
    EXPECT_ENCODE("_v8="_s, Vector<uint8_t>({ 254, 255 }));
    EXPECT_ENCODE("AAGA_v8="_s, Vector<uint8_t>({ 0, 1, 128, 254, 255 }));
}

TEST(Base64, EncodeURLOmitPadding)
{
    static constexpr OptionSet<Base64EncodeOption> options = { Base64EncodeOption::URL, Base64EncodeOption::OmitPadding };

    EXPECT_ENCODE(""_s, byteCast<uint8_t>(""_span));
    EXPECT_ENCODE("Zg"_s, byteCast<uint8_t>("f"_span));
    EXPECT_ENCODE("Zm8"_s, byteCast<uint8_t>("fo"_span));
    EXPECT_ENCODE("Zm9v"_s, byteCast<uint8_t>("foo"_span));
    EXPECT_ENCODE("Zm9vYg"_s, byteCast<uint8_t>("foob"_span));
    EXPECT_ENCODE("Zm9vYmE"_s, byteCast<uint8_t>("fooba"_span));
    EXPECT_ENCODE("Zm9vYmFy"_s, byteCast<uint8_t>("foobar"_span));

    EXPECT_ENCODE("AA"_s, Vector<uint8_t>({ 0 }));
    EXPECT_ENCODE("AQ"_s, Vector<uint8_t>({ 1 }));
    EXPECT_ENCODE("gA"_s, Vector<uint8_t>({ 128 }));
    EXPECT_ENCODE("_g"_s, Vector<uint8_t>({ 254 }));
    EXPECT_ENCODE("_w"_s, Vector<uint8_t>({ 255 }));
    EXPECT_ENCODE("AAE"_s, Vector<uint8_t>({ 0, 1 }));
    EXPECT_ENCODE("_v8"_s, Vector<uint8_t>({ 254, 255 }));
    EXPECT_ENCODE("AAGA_v8"_s, Vector<uint8_t>({ 0, 1, 128, 254, 255 }));
}

TEST(Base64, Decode)
{
    static constexpr OptionSet<Base64DecodeOption> options;

    EXPECT_DECODE_EMPTY("==="_s);
    EXPECT_DECODE("f"_s, "Zg==="_s);
    EXPECT_DECODE("fo"_s, "Zm8==="_s);
    EXPECT_DECODE("foo"_s, "Zm9v==="_s);
    EXPECT_DECODE("foob"_s, "Zm9vYg==="_s);
    EXPECT_DECODE("fooba"_s, "Zm9vYmE==="_s);
    EXPECT_DECODE("foobar"_s, "Zm9vYmFy==="_s);

    EXPECT_TRUE(Vector<uint8_t>({ 0 }) == base64Decode("AA==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 1 }) == base64Decode("AQ==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 128 }) == base64Decode("gA==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254 }) == base64Decode("/g==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 255 }) == base64Decode("/w==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1 }) == base64Decode("AAE==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254, 255 }) == base64Decode("/v8==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1, 128, 254, 255 }) == base64Decode("AAGA/v8==="_s, options));
}

TEST(Base64, DecodeValidatePadding)
{
    static constexpr OptionSet<Base64DecodeOption> options = { Base64DecodeOption::ValidatePadding };

    EXPECT_DECODE_EMPTY(""_s);
    EXPECT_DECODE("f"_s, "Zg=="_s);
    EXPECT_DECODE("fo"_s, "Zm8="_s);
    EXPECT_DECODE("foo"_s, "Zm9v"_s);
    EXPECT_DECODE("foob"_s, "Zm9vYg=="_s);
    EXPECT_DECODE("fooba"_s, "Zm9vYmE="_s);
    EXPECT_DECODE("foobar"_s, "Zm9vYmFy"_s);

    EXPECT_TRUE(Vector<uint8_t>({ 0 }) == base64Decode("AA=="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 1 }) == base64Decode("AQ=="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 128 }) == base64Decode("gA=="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254 }) == base64Decode("/g=="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 255 }) == base64Decode("/w=="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1 }) == base64Decode("AAE="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254, 255 }) == base64Decode("/v8="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1, 128, 254, 255 }) == base64Decode("AAGA/v8="_s, options));
}

TEST(Base64, DecodeIgnoreWhitespace)
{
    static constexpr OptionSet<Base64DecodeOption> options = { Base64DecodeOption::IgnoreWhitespace };

    EXPECT_DECODE_EMPTY(" = = = "_s);
    EXPECT_DECODE("f"_s, " Z g = = = "_s);
    EXPECT_DECODE("fo"_s, " Z m 8 = = = "_s);
    EXPECT_DECODE("foo"_s, " Z m 9 v = = = "_s);
    EXPECT_DECODE("foob"_s, " Z m 9 v Y g = = = "_s);
    EXPECT_DECODE("fooba"_s, " Z m 9 v Y m E = = = "_s);
    EXPECT_DECODE("foobar"_s, " Z m 9 v Y m F y = = = "_s);

    EXPECT_TRUE(Vector<uint8_t>({ 0 }) == base64Decode(" A A = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 1 }) == base64Decode(" A Q = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 128 }) == base64Decode(" g A = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254 }) == base64Decode(" / g = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 255 }) == base64Decode(" / w = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1 }) == base64Decode(" A A E = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254, 255 }) == base64Decode(" / v 8 = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1, 128, 254, 255 }) == base64Decode(" A A G A / v 8 = = = "_s, options));
}

TEST(Base64, DecodeValidatePaddingIgnoreWhitespace)
{
    static constexpr OptionSet<Base64DecodeOption> options = { Base64DecodeOption::ValidatePadding, Base64DecodeOption::IgnoreWhitespace };

    EXPECT_DECODE_EMPTY(" "_s);
    EXPECT_DECODE("f"_s, " Z g = = "_s);
    EXPECT_DECODE("fo"_s, " Z m 8 = "_s);
    EXPECT_DECODE("foo"_s, " Z m 9 v "_s);
    EXPECT_DECODE("foob"_s, " Z m 9 v Y g = = "_s);
    EXPECT_DECODE("fooba"_s, " Z m 9 v Y m E = "_s);
    EXPECT_DECODE("foobar"_s, " Z m 9 v Y m F y "_s);

    EXPECT_TRUE(Vector<uint8_t>({ 0 }) == base64Decode(" A A = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 1 }) == base64Decode(" A Q = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 128 }) == base64Decode(" g A = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254 }) == base64Decode(" / g = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 255 }) == base64Decode(" / w = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1 }) == base64Decode(" A A E = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254, 255 }) == base64Decode(" / v 8 = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1, 128, 254, 255 }) == base64Decode(" A A G A / v 8 = "_s, options));
}

TEST(Base64, DecodeURL)
{
    static constexpr OptionSet<Base64DecodeOption> options = { Base64DecodeOption::URL };

    EXPECT_DECODE_EMPTY("==="_s);
    EXPECT_DECODE("f"_s, "Zg==="_s);
    EXPECT_DECODE("fo"_s, "Zm8==="_s);
    EXPECT_DECODE("foo"_s, "Zm9v==="_s);
    EXPECT_DECODE("foob"_s, "Zm9vYg==="_s);
    EXPECT_DECODE("fooba"_s, "Zm9vYmE==="_s);
    EXPECT_DECODE("foobar"_s, "Zm9vYmFy==="_s);

    EXPECT_TRUE(Vector<uint8_t>({ 0 }) == base64Decode("AA==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 1 }) == base64Decode("AQ==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 128 }) == base64Decode("gA==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254 }) == base64Decode("_g==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 255 }) == base64Decode("_w==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1 }) == base64Decode("AAE==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254, 255 }) == base64Decode("_v8==="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1, 128, 254, 255 }) == base64Decode("AAGA_v8==="_s, options));
}

TEST(Base64, DecodeURLValidatePadding)
{
    static constexpr OptionSet<Base64DecodeOption> options = { Base64DecodeOption::URL, Base64DecodeOption::ValidatePadding };

    EXPECT_DECODE_EMPTY(""_s);
    EXPECT_DECODE("f"_s, "Zg=="_s);
    EXPECT_DECODE("fo"_s, "Zm8="_s);
    EXPECT_DECODE("foo"_s, "Zm9v"_s);
    EXPECT_DECODE("foob"_s, "Zm9vYg=="_s);
    EXPECT_DECODE("fooba"_s, "Zm9vYmE="_s);
    EXPECT_DECODE("foobar"_s, "Zm9vYmFy"_s);

    EXPECT_TRUE(Vector<uint8_t>({ 0 }) == base64Decode("AA=="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 1 }) == base64Decode("AQ=="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 128 }) == base64Decode("gA=="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254 }) == base64Decode("_g=="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 255 }) == base64Decode("_w=="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1 }) == base64Decode("AAE="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254, 255 }) == base64Decode("_v8="_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1, 128, 254, 255 }) == base64Decode("AAGA_v8="_s, options));
}

TEST(Base64, DecodeURLIgnoreWhitespace)
{
    static constexpr OptionSet<Base64DecodeOption> options = { Base64DecodeOption::URL, Base64DecodeOption::IgnoreWhitespace };

    EXPECT_DECODE_EMPTY(" = = = "_s);
    EXPECT_DECODE("f"_s, " Z g = = = "_s);
    EXPECT_DECODE("fo"_s, " Z m 8 = = = "_s);
    EXPECT_DECODE("foo"_s, " Z m 9 v = = = "_s);
    EXPECT_DECODE("foob"_s, " Z m 9 v Y g = = = "_s);
    EXPECT_DECODE("fooba"_s, " Z m 9 v Y m E = = = "_s);
    EXPECT_DECODE("foobar"_s, " Z m 9 v Y m F y = = = "_s);

    EXPECT_TRUE(Vector<uint8_t>({ 0 }) == base64Decode(" A A = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 1 }) == base64Decode(" A Q = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 128 }) == base64Decode(" g A = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254 }) == base64Decode(" _ g = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 255 }) == base64Decode(" _ w = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1 }) == base64Decode(" A A E = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254, 255 }) == base64Decode(" _ v 8 = = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1, 128, 254, 255 }) == base64Decode(" A A G A _ v 8 = = = "_s, options));
}

TEST(Base64, DecodeURLValidatePaddingIgnoreWhitespace)
{
    static constexpr OptionSet<Base64DecodeOption> options = { Base64DecodeOption::URL, Base64DecodeOption::ValidatePadding, Base64DecodeOption::IgnoreWhitespace };

    EXPECT_DECODE_EMPTY(" "_s);
    EXPECT_DECODE("f"_s, " Z g = = "_s);
    EXPECT_DECODE("fo"_s, " Z m 8 = "_s);
    EXPECT_DECODE("foo"_s, " Z m 9 v "_s);
    EXPECT_DECODE("foob"_s, " Z m 9 v Y g = = "_s);
    EXPECT_DECODE("fooba"_s, " Z m 9 v Y m E = "_s);
    EXPECT_DECODE("foobar"_s, " Z m 9 v Y m F y "_s);

    EXPECT_TRUE(Vector<uint8_t>({ 0 }) == base64Decode(" A A = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 1 }) == base64Decode(" A Q = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 128 }) == base64Decode(" g A = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254 }) == base64Decode(" _ g = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 255 }) == base64Decode(" _ w = = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1 }) == base64Decode(" A A E = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 254, 255 }) == base64Decode(" _ v 8 = "_s, options));
    EXPECT_TRUE(Vector<uint8_t>({ 0, 1, 128, 254, 255 }) == base64Decode(" A A G A _ v 8 = "_s, options));
}

TEST(Base64, EncodedSizeRejectsInputAboveUnsignedRange)
{
    static constexpr OptionSet<Base64EncodeOption> options;

    size_t hugeLengthTruncatingToSmallValue = static_cast<size_t>(std::numeric_limits<unsigned>::max()) + 100;
    EXPECT_EQ(calculateBase64EncodedSize(hugeLengthTruncatingToSmallValue, options), 0U);

    EXPECT_EQ(calculateBase64EncodedSize(static_cast<size_t>(WTF::maximumBase64EncoderInputBufferSize) + 1, options), 0U);

    EXPECT_EQ(calculateBase64EncodedSize(0, options), 0U);
    EXPECT_EQ(calculateBase64EncodedSize(1, options), 4U);
    EXPECT_EQ(calculateBase64EncodedSize(3, options), 4U);
    EXPECT_EQ(calculateBase64EncodedSize(6, options), 8U);
    EXPECT_NE(calculateBase64EncodedSize(WTF::maximumBase64EncoderInputBufferSize, options), 0U);
}

} // namespace TestWebKitAPI
