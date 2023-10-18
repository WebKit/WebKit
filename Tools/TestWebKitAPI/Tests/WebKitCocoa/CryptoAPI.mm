#import "config.h"

#if ENABLE(WEB_CRYPTO)

#import "PlatformUtilities.h"
#import <pal/spi/cocoa/CoreCryptoSPI.h>

#include <WebCore/CryptoKeyOKP.h>

static void testCase()
{
    fprintf(stderr, "==== TEST CASE ====\n");

    std::array<uint8_t, 32> publicKey {
        216, 225, 137, 99, 216, 9, 212, 135, 217, 84, 154, 204, 174, 198, 116, 46,
        126, 235, 162, 77, 138, 13, 59, 20, 183, 227, 202, 234, 6, 137, 61, 204 };
    std::array<uint8_t, 32> privateKey {
        243, 200, 244, 196, 141, 248, 120, 20, 110, 140, 211, 191, 109, 244, 229, 14,
        56, 155, 167, 7, 78, 21, 194, 53, 45, 205, 93, 48, 141, 76, 168, 31 };

    std::array<uint8_t, 64> data {
        43, 126, 208, 188, 119, 149, 105, 74, 180, 172, 211, 89, 3, 254, 140, 215,
        216, 15, 106, 28, 134, 136, 166, 195, 65, 68, 9, 69, 117, 20, 161, 69,
        120, 85, 187, 178, 25, 227, 10, 27, 238, 168, 254, 134, 144, 130, 217, 159,
        200, 40, 47, 144, 80, 208, 36, 229, 158, 175, 7, 48, 186, 157, 183, 10 };

    std::array<uint8_t, 64> expectedSignature {
        61, 144, 222, 94, 87, 67, 223, 194, 130, 37, 191, 173, 179, 65, 177, 22,
        203, 248, 163, 241, 206, 237, 191, 74, 220, 53, 14, 245, 211, 71, 24, 67,
        164, 24, 97, 77, 203, 110, 97, 72, 98, 97, 76, 247, 175, 20, 150, 249,
        52, 11, 60, 132, 78, 164, 220, 234, 177, 211, 209, 85, 235, 126, 204, 0 };

    fprintf(stderr, "  publicKey: ");
    for (auto b : publicKey)
        fprintf(stderr, "%02x", b);
    fprintf(stderr, "\n");

    fprintf(stderr, "  privateKey: ");
    for (auto b : privateKey)
        fprintf(stderr, "%02x", b);
    fprintf(stderr, "\n");

    fprintf(stderr, "  data: ");
    for (auto b : data)
        fprintf(stderr, "%02x", b);
    fprintf(stderr, "\n");

    fprintf(stderr, "  expected signature: ");
    for (auto b : expectedSignature)
        fprintf(stderr, "%02x", b);
    fprintf(stderr, "\n");

    {
        const struct ccdigest_info* di = ccsha512_di();

        ccec25519pubkey generatedPK;
        memset(generatedPK, 0, sizeof(ccec25519pubkey));
        cced25519_make_pub(di, generatedPK, privateKey.data());

        fprintf(stderr, "\n  generated public key: ");
        for (size_t i = 0; i < 32; ++i)
            fprintf(stderr, "%02x", generatedPK[i]);
        bool generatedPublicKeyMatches = !memcmp(publicKey.data(), generatedPK, 32);
        EXPECT_TRUE(generatedPublicKeyMatches);
        if (!generatedPublicKeyMatches)
            fprintf(stderr, "\n  TEST: generated public key doesn NOT match the specified one");
    }

    for (size_t i = 0; i < 8; ++i) {
        const struct ccdigest_info* di = ccsha512_di();

        ccec25519signature generatedSignature;
        memset(generatedSignature, 0, sizeof(ccec25519signature));
        cced25519_sign(di, generatedSignature, data.size(), data.data(), publicKey.data(), privateKey.data());
        fprintf(stderr, "\n  generated signature: ");
        for (size_t i = 0; i < 64; ++i)
            fprintf(stderr, "%02x", generatedSignature[i]);
        bool generatedSignatureMatches = !memcmp(expectedSignature.data(), generatedSignature, 64);
        EXPECT_TRUE(generatedSignatureMatches);
        if (!generatedSignatureMatches)
            fprintf(stderr, "\n  TEST: iteration %zu: generated signature does NOT match expected signature", i + 1);
    }

    fprintf(stderr, "\n==== TEST CASE ====\n\n\n");
}


TEST(CryptoAPI, Ed25519Signing)
{
    testCase();
}


#endif
