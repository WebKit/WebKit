/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#include <PAL/crypto/CryptoDigest.h>
#include <wtf/text/CString.h>

namespace TestWebKitAPI {

static CString toHex(WTF::Vector<uint8_t>&& hash)
{
    const char hex[] = "0123456789ABCDEF";

    WTF::Vector<char> buffer(hash.size() * 2);
    for (size_t i = 0; i < hash.size(); i++) {
        uint8_t hi = hash[i] >> 4;
        uint8_t lo = hash[i] & 0x0F;
        buffer[2 * i] = hex[hi];
        buffer[2 * i + 1] = hex[lo];
    }

    return CString(buffer.data(), buffer.size());
}

static void expect(PAL::CryptoDigest::Algorithm algorithm, const CString& input, int repeat, const CString& expected)
{
    auto cryptoDigest = PAL::CryptoDigest::create(algorithm);

    for (int i = 0; i < repeat; ++i)
        cryptoDigest->addBytes(input.data(), input.length());

    CString actual = toHex(cryptoDigest->computeHash());

    ASSERT_EQ(expected.length(), actual.length());
    ASSERT_STREQ(expected.data(), actual.data());
}

static void expectSHA1(const CString& input, int repeat, const CString& expected)
{
    expect(PAL::CryptoDigest::Algorithm::SHA_1, input, repeat, expected);
}

static void expectSHA224(const CString& input, int repeat, const CString& expected)
{
    expect(PAL::CryptoDigest::Algorithm::SHA_224, input, repeat, expected);
}

static void expectSHA256(const CString& input, int repeat, const CString& expected)
{
    expect(PAL::CryptoDigest::Algorithm::SHA_256, input, repeat, expected);
}

static void expectSHA384(const CString& input, int repeat, const CString& expected)
{
    expect(PAL::CryptoDigest::Algorithm::SHA_384, input, repeat, expected);
}

static void expectSHA512(const CString& input, int repeat, const CString& expected)
{
    expect(PAL::CryptoDigest::Algorithm::SHA_512, input, repeat, expected);
}

TEST(CryptoDigest, SHA1Computation)
{
    // Examples taken from sample code in RFC 3174.
    expectSHA1("abc", 1, "A9993E364706816ABA3E25717850C26C9CD0D89D");
    expectSHA1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1, "84983E441C3BD26EBAAE4AA1F95129E5E54670F1");
    expectSHA1("a", 1000000, "34AA973CD4C4DAA4F61EEB2BDBAD27316534016F");
    expectSHA1("0123456701234567012345670123456701234567012345670123456701234567", 10, "DEA356A2CDDD90C7A7ECEDC5EBB563934F460452");
}

TEST(CryptoDigest, SHA224Computation)
{
    // Examples taken from sample code in RFC 3874.
    expectSHA224("abc", 1, "23097D223405D8228642A477BDA255B32AADBCE4BDA0B3F7E36C9DA7");
    expectSHA224("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1, "75388B16512776CC5DBA5DA1FD890150B0C6455CB4F58B1952522525");
    expectSHA224("a", 1000000, "20794655980C91D8BBB4C1EA97618A4BF03F42581948B2EE4EE7AD67");
}

TEST(CryptoDigest, SHA256Computation)
{
    // Examples taken from sample code in FIPS-180.
    expectSHA256("abc", 1, "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
    expectSHA256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1, "248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1");
    expectSHA256("a", 1000000, "CDC76E5C9914FB9281A1C7E284D73E67F1809A48A497200E046D39CCC7112CD0");
}

TEST(CryptoDigest, SHA384Computation)
{
    // Examples taken from sample code in FIPS-180.
    expectSHA384("abc", 1, "CB00753F45A35E8BB5A03D699AC65007272C32AB0EDED1631A8B605A43FF5BED8086072BA1E7CC2358BAECA134C825A7");
    expectSHA384("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 1, "09330C33F71147E83D192FC782CD1B4753111B173B3B05D22FA08086E3B0F712FCC7C71A557E2DB966C3E9FA91746039");
    expectSHA384("a", 1000000, "9D0E1809716474CB086E834E310A4A1CED149E9C00F248527972CEC5704C2A5B07B8B3DC38ECC4EBAE97DDD87F3D8985");
}

TEST(CryptoDigest, SHA512Computation)
{
    // Examples taken from sample code in FIPS-180.
    expectSHA512("abc", 1, "DDAF35A193617ABACC417349AE20413112E6FA4E89A97EA20A9EEEE64B55D39A2192992A274FC1A836BA3C23A3FEEBBD454D4423643CE80E2A9AC94FA54CA49F");
    expectSHA512("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 1, "8E959B75DAE313DA8CF4F72814FC143F8F7779C6EB9F7FA17299AEADB6889018501D289E4900F7E4331B99DEC4B5433AC7D329EEB6DD26545E96E55B874BE909");
    expectSHA512("a", 1000000, "E718483D0CE769644E2E42C7BC15B4638E1F98B13B2044285632A803AFA973EBDE0FF244877EA60A4CB0432CE577C31BEB009C5C2C49AA2E4EADB217AD8CC09B");
}

}
