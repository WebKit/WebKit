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

#if ENABLE(SERVICE_WORKER)

#include "Test.h"
#include <WebCore/PushMessageCrypto.h>
#include <wtf/text/Base64.h>

namespace TestWebKitAPI {
using namespace WebCore::PushCrypto;

static Vector<uint8_t> mustBase64URLDecode(const String& encoded)
{
    return base64URLDecode(encoded).value();
}

static ClientKeys makeAES128GCMClientKeys(void)
{
    // Example values from RFC8291 Section 5.
    auto publicKey = mustBase64URLDecode("BCVxsr7N_eNgVRqvHtD0zTZsEc6-VV-JvLexhqUzORcxaOzi6-AYWXvTBHm4bjyPjs7Vd8pZGH6SRpkNtoIAiw4"_s);
    auto privateKey = mustBase64URLDecode("q1dXpw3UpT5VOmu_cf_v6ih07Aems3njxI-JWgLcM94"_s);
    auto secret = mustBase64URLDecode("BTBZMqHH6r4Tts7J_aSIgg"_s);

    return ClientKeys {
        P256DHKeyPair { WTFMove(publicKey), WTFMove(privateKey) },
        WTFMove(secret)
    };
}

TEST(PushMessageCrypto, AES128GCMPayloadWithMinimalPadding)
{
    // Example payload from RFC8291 Section 5.
    auto clientKeys = makeAES128GCMClientKeys();
    auto payload = mustBase64URLDecode("DGv6ra1nlYgDCS1FRnbzlwAAEABBBP4z9KsN6nGRTbVYI_c7VJSPQTBtkgcy27mlmlMoZIIgDll6e3vCYLocInmYWAmS6TlzAC8wEqKK6PBru3jl7A_yl95bQpu6cVPTpK4Mqgkf1CXztLVBSt2Ks3oZwbuwXPXLWyouBWLVWGNWQexSgSxsj_Qulcy4a-fN"_s);

    auto result = decryptAES128GCMPayload(clientKeys, payload);
    ASSERT_TRUE(result.has_value());

    auto actual = String::adopt(WTFMove(*result));
    ASSERT_EQ("When I grow up, I want to be a watermelon"_s, actual);
}

TEST(PushMessageCrypto, AES128GCMPayloadWithPadding)
{
    auto clientKeys = makeAES128GCMClientKeys();
    auto payload = mustBase64URLDecode("DGv6ra1nlYgDCS1FRnbzlwAAEABBBP4z9KsN6nGRTbVYI_c7VJSPQTBtkgcy27mlmlMoZIIgDll6e3vCYLocInmYWAmS6TlzAC8wEqKK6PBru3jl7A_DkNRXA6CYFiG805FVNqPbr9v9EW9lfwpny0c"_s);

    auto result = decryptAES128GCMPayload(clientKeys, payload);
    ASSERT_TRUE(result.has_value());

    auto actual = String::adopt(WTFMove(*result));
    ASSERT_EQ("foobar"_s, actual);
}

TEST(PushMessageCrypto, AES128GCMPayloadWithIncorrectPadding)
{
    auto clientKeys = makeAES128GCMClientKeys();
    auto payload = mustBase64URLDecode("DGv6ra1nlYgDCS1FRnbzlwAAEABBBP4z9KsN6nGRTbVYI_c7VJSPQTBtkgcy27mlmlMoZIIgDll6e3vCYLocInmYWAmS6TlzAC8wEqKK6PBru3jl7A_DkNRXA6CbFiG80zK6Bf1IRIL9ovjHp7iLjCw"_s);

    auto result = decryptAES128GCMPayload(clientKeys, payload);
    ASSERT_FALSE(result.has_value());
}

TEST(PushMessageCrypto, AES128GCMPayloadWithEmptyPlaintext)
{
    auto clientKeys = makeAES128GCMClientKeys();
    auto payload = mustBase64URLDecode("DGv6ra1nlYgDCS1FRnbzlwAAEABBBP4z9KsN6nGRTbVYI_c7VJSPQTBtkgcy27mlmlMoZIIgDll6e3vCYLocInmYWAmS6TlzAC8wEqKK6PBru3jl7A-nWA9JFLVeQ32ERULH8YEUcA"_s);

    auto result = decryptAES128GCMPayload(clientKeys, payload);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->isEmpty());
}

static ClientKeys makeAESGCMClientKeys(void)
{
    // Example values from draft-ietf-webpush-encryption-04 Section 5.
    auto publicKey = mustBase64URLDecode("BCEkBjzL8Z3C-oi2Q7oE5t2Np-p7osjGLg93qUP0wvqRT21EEWyf0cQDQcakQMqz4hQKYOQ3il2nNZct4HgAUQU"_s);
    auto privateKey = mustBase64URLDecode("9FWl15_QUQAWDaD3k3l50ZBZQJ4au27F1V4F0uLSD_M"_s);
    auto secret = mustBase64URLDecode("R29vIGdvbyBnJyBqb29iIQ"_s);

    return ClientKeys {
        P256DHKeyPair { WTFMove(publicKey), WTFMove(privateKey) },
        WTFMove(secret)
    };
}

TEST(PushMessageCrypto, AESGCMPayloadWithMinimalPadding)
{
    // Example values from draft-ietf-webpush-encryption-04 Section 5.
    auto clientKeys = makeAESGCMClientKeys();
    auto salt = mustBase64URLDecode("lngarbyKfMoi9Z75xYXmkg"_s);
    auto serverPublicKey = mustBase64URLDecode("BNoRDbb84JGm8g5Z5CFxurSqsXWJ11ItfXEWYVLE85Y7CYkDjXsIEc4aqxYaQ1G8BqkXCJ6DPpDrWtdWj_mugHU"_s);
    auto payload = mustBase64URLDecode("6nqAQUME8hNqw5J3kl8cpVVJylXKYqZOeseZG8UueKpA"_s);

    auto result = decryptAESGCMPayload(clientKeys, serverPublicKey, salt, payload);
    ASSERT_TRUE(result.has_value());

    auto actual = String::adopt(WTFMove(*result));
    ASSERT_EQ("I am the walrus"_s, actual);
}

TEST(PushMessageCrypto, AESGCMPayloadWithPadding)
{
    auto clientKeys = makeAESGCMClientKeys();
    auto salt = mustBase64URLDecode("lngarbyKfMoi9Z75xYXmkg"_s);
    auto serverPublicKey = mustBase64URLDecode("BNoRDbb84JGm8g5Z5CFxurSqsXWJ11ItfXEWYVLE85Y7CYkDjXsIEc4aqxYaQ1G8BqkXCJ6DPpDrWtdWj_mugHU"_s);
    auto payload = mustBase64URLDecode("6nnJYSIPvQhgx8ApDFDkWY6v5N9KAuur3Nv6"_s);

    auto result = decryptAESGCMPayload(clientKeys, serverPublicKey, salt, payload);
    ASSERT_TRUE(result.has_value());

    auto actual = String::adopt(WTFMove(*result));
    ASSERT_EQ("foobar"_s, actual);
}

TEST(PushMessageCrypto, AESGCMPayloadWithIncorrectPadding)
{
    auto clientKeys = makeAESGCMClientKeys();
    auto salt = mustBase64URLDecode("lngarbyKfMoi9Z75xYXmkg"_s);
    auto serverPublicKey = mustBase64URLDecode("BNoRDbb84JGm8g5Z5CFxurSqsXWJ11ItfXEWYVLE85Y7CYkDjXsIEc4aqxYaQ1G8BqkXCJ6DPpDrWtdWj_mugHU"_s);
    auto payload = mustBase64URLDecode("6n7JYSNptAhtxNNyA5YtGywKZGxwcbEsBPqauw"_s);

    auto result = decryptAESGCMPayload(clientKeys, serverPublicKey, salt, payload);
    ASSERT_FALSE(result.has_value());
}

TEST(PushMessageCrypto, AESGCMPayloadWithEmptyPlaintext)
{
    auto clientKeys = makeAESGCMClientKeys();
    auto salt = mustBase64URLDecode("lngarbyKfMoi9Z75xYXmkg"_s);
    auto serverPublicKey = mustBase64URLDecode("BNoRDbb84JGm8g5Z5CFxurSqsXWJ11ItfXEWYVLE85Y7CYkDjXsIEc4aqxYaQ1G8BqkXCJ6DPpDrWtdWj_mugHU"_s);
    auto payload = mustBase64URLDecode("6nogQ_ptbmKpIB8yi4_4H0LS"_s);

    auto result = decryptAESGCMPayload(clientKeys, serverPublicKey, salt, payload);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->isEmpty());
}

}

#endif
