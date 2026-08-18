/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "Helpers/Test.h"
#include <JavaScriptCore/DataView.h>
#include <WebCore/ISOFairPlayStreamingPsshBox.h>
#include <WebCore/ISOOriginalFormatBox.h>
#include <WebCore/ISOProtectionSchemeInfoBox.h>
#include <WebCore/ISOProtectionSystemSpecificHeaderBox.h>
#include <WebCore/ISOSchemeInformationBox.h>
#include <WebCore/ISOSchemeTypeBox.h>
#include <WebCore/ISOTrackEncryptionBox.h>
#include <WebCore/ISOVTTCue.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/text/Base64.h>

using namespace WebCore;

namespace TestWebKitAPI {

// Reads `box` out of `bytes`, then serializes it back out and returns the result. A
// correctly implemented box writes byte-for-byte what it read, so callers can assert
// equality against the original input. Returns an empty Vector if either step fails.
//
// `box` must not have been read into already: some boxes refuse a second read rather than
// overwrite sub-boxes they have already parsed.
static Vector<uint8_t> readThenSerialize(ISOBox& box, const Vector<uint8_t>& bytes)
{
    if (!box.read(bytes.span()))
        return { };

    auto serialized = box.serialize();
    return { serialized->span() };
}

// Computes `box`'s size from its own fields, serializes it, and returns the bytes. Unlike
// readThenSerialize(), nothing here is inherited from parsed input: updateSize() has to derive
// the size from the box's partialSize() overrides alone, and serialize() allocates from that.
static Vector<uint8_t> updateSizeThenSerialize(ISOBox& box)
{
    box.updateSize();
    auto serialized = box.serialize();
    return { serialized->span() };
}

static Vector<uint8_t> decodeBase64(ASCIILiteral literal)
{
    auto decoded = base64Decode(StringView { literal });
    if (!decoded)
        return { };
    return WTF::move(*decoded);
}

static constexpr auto base64EncodedSinfWithKeyID3 = "AAAAYXNpbmYAAAAMZnJtYW1wNGEAAAAUc2NobQAAAABjYmNzAAEAAAAAADlzY2hpAAAAMXRlbmMBAAAAAAABAAAAAAAAAAAAAAAAAAAAAAMQ1fvWuC7ZPk75iuQJMe4ztw=="_s;

TEST(ISOBox, ISOProtectionSchemeInfoBox)
{
    auto sinfArray = base64Decode(StringView { base64EncodedSinfWithKeyID3 });
    ASSERT_TRUE(sinfArray);
    ASSERT_EQ(97UL, sinfArray->size());

    auto view = sinfArray->span();

    ISOProtectionSchemeInfoBox sinfBox;
    ASSERT_TRUE(sinfBox.read(view));
    ASSERT_EQ(FourCC('mp4a'), sinfBox.originalFormatBox().dataFormat());

    auto* schemeTypeBox = sinfBox.schemeTypeBox();
    ASSERT_NOT_NULL(schemeTypeBox);
    ASSERT_EQ(FourCC('cbcs'), schemeTypeBox->schemeType());
    ASSERT_EQ(0x10000U, schemeTypeBox->schemeVersion());

    auto* schemeInformationBox = sinfBox.schemeInformationBox();
    ASSERT_NOT_NULL(schemeInformationBox);

    auto* trackEncryptionBox = downcast<ISOTrackEncryptionBox>(schemeInformationBox->schemeSpecificData());
    ASSERT_NOT_NULL(trackEncryptionBox);
    ASSERT_FALSE(trackEncryptionBox->defaultCryptByteBlock().value());
    ASSERT_FALSE(trackEncryptionBox->defaultSkipByteBlock().value());
    ASSERT_EQ(1, trackEncryptionBox->defaultIsProtected());
    ASSERT_EQ(0, trackEncryptionBox->defaultPerSampleIVSize());

    Vector<uint8_t> defaultKeyID = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3};
    ASSERT_EQ(defaultKeyID, trackEncryptionBox->defaultKID());

    Vector<uint8_t> defaultIV = {0xD5, 0xFB, 0xD6, 0xB8, 0x2E, 0xD9, 0x3E, 0x4E, 0xF9, 0x8A, 0xE4, 0x09, 0x31, 0xEE, 0x33, 0xB7};
    ASSERT_EQ(defaultIV, trackEncryptionBox->defaultConstantIV());
}

static constexpr auto base64EncodedPsshWithAssetId = "AAAAsHBzc2gAAAAAlM6G+wf/T0OtuJPS+paMogAAAJAAAACQZnBzZAAAABBmcHNpAAAAAGNlbmMAAAA8ZnBzawAAABxma3JpAAAAAAAAAAAAAAAAAAAAAAAAAAEAAAAYZmthaQAAAAAAAAAAAAAAAAAAAPEAAAA8ZnBzawAAABxma3JpAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAAYZmthaQAAAAAAAAAAAAAAAAAAAPI="_s;

TEST(ISOBox, ISOFairPlayStreamingPsshBox)
{
    auto psshArray = base64Decode(StringView(base64EncodedPsshWithAssetId));
    ASSERT_TRUE(psshArray);
    ASSERT_EQ(176UL, psshArray->size());

    auto view = psshArray->span();

    ISOFairPlayStreamingPsshBox psshBox;

    ASSERT_TRUE(psshBox.read(view));

    auto infoBox = psshBox.initDataBox().info();
    ASSERT_EQ(FourCC('cenc'), infoBox.scheme());

    auto requests = psshBox.initDataBox().requests();
    ASSERT_EQ(2UL, requests.size());

    Vector<uint8_t, 16> expectedFirstKeyID = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    ASSERT_EQ(requests[0].requestInfo().keyID(), expectedFirstKeyID);

    Vector<uint8_t> expectedFirstAssetID = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF1 };
    ASSERT_TRUE(requests[0].assetID());
    ASSERT_EQ(requests[0].assetID().value().data(), expectedFirstAssetID);

    Vector<uint8_t, 16> expectedSecondKeyID = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 };
    ASSERT_EQ(requests[1].requestInfo().keyID(), expectedSecondKeyID);

    Vector<uint8_t> expectedSecondAssetID = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF2 };
    ASSERT_TRUE(requests[1].assetID());
    ASSERT_EQ(requests[1].assetID().value().data(), expectedSecondAssetID);
}

// MARK: - Round-trip tests
//
// Serializing a box that was just read must reproduce the original bytes exactly.

TEST(ISOBox, ProtectionSchemeInfoBox_Roundtrip)
{
    auto sinfArray = decodeBase64(base64EncodedSinfWithKeyID3);
    ASSERT_EQ(97UL, sinfArray.size());

    ISOProtectionSchemeInfoBox sinfBox;
    EXPECT_EQ(sinfArray, readThenSerialize(sinfBox, sinfArray));
}

TEST(ISOBox, FairPlayStreamingPsshBox_Roundtrip)
{
    auto psshArray = decodeBase64(base64EncodedPsshWithAssetId);
    ASSERT_EQ(176UL, psshArray.size());

    ISOFairPlayStreamingPsshBox psshBox;
    EXPECT_EQ(psshArray, readThenSerialize(psshBox, psshArray));
}

// Byte equality alone would also be satisfied by a writer that echoed the bytes it parsed,
// which is what ISOFairPlayStreamingPsshBox used to do via the base class's m_data. Now that
// it serializes through m_initDataBox, re-parse the output and check the sub-box tree came
// back, so a regression to byte-echoing is caught.
TEST(ISOBox, FairPlayStreamingPsshBox_RoundtripReconstructsSubBoxes)
{
    auto psshArray = decodeBase64(base64EncodedPsshWithAssetId);
    ASSERT_EQ(176UL, psshArray.size());

    ISOFairPlayStreamingPsshBox original;
    auto serialized = readThenSerialize(original, psshArray);
    ASSERT_EQ(psshArray, serialized);

    ISOFairPlayStreamingPsshBox reparsed;
    auto reparsedView = serialized.span();
    ASSERT_TRUE(reparsed.read(reparsedView));

    EXPECT_EQ(ISOFairPlayStreamingPsshBox::fairPlaySystemID(), reparsed.systemID());
    EXPECT_EQ(FourCC('cenc'), reparsed.initDataBox().info().scheme());

    auto& requests = reparsed.initDataBox().requests();
    ASSERT_EQ(2UL, requests.size());

    Vector<uint8_t, 16> expectedFirstKeyID { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    EXPECT_EQ(expectedFirstKeyID, requests[0].requestInfo().keyID());
    Vector<uint8_t> expectedFirstAssetID { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF1 };
    ASSERT_TRUE(requests[0].assetID());
    EXPECT_EQ(expectedFirstAssetID, requests[0].assetID()->data());

    Vector<uint8_t, 16> expectedSecondKeyID { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 };
    EXPECT_EQ(expectedSecondKeyID, requests[1].requestInfo().keyID());
    Vector<uint8_t> expectedSecondAssetID { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF2 };
    ASSERT_TRUE(requests[1].assetID());
    EXPECT_EQ(expectedSecondAssetID, requests[1].assetID()->data());
}

// serialize() allocates from m_size, so the size a box computes for itself has to agree with
// the size it parsed. Recomputing it must be a no-op for a well-formed box, and the box must
// still serialize identically afterwards.
TEST(ISOBox, FairPlayStreamingPsshBox_UpdateSizeIsStable)
{
    auto psshArray = decodeBase64(base64EncodedPsshWithAssetId);
    ASSERT_EQ(176UL, psshArray.size());

    ISOFairPlayStreamingPsshBox psshBox;
    auto view = psshArray.span();
    ASSERT_TRUE(psshBox.read(view));
    ASSERT_EQ(176U, psshBox.size());

    psshBox.updateSize();
    EXPECT_EQ(176U, psshBox.size());

    auto serialized = psshBox.serialize();
    EXPECT_EQ(psshArray, Vector<uint8_t> { serialized->span() });
}

// MARK: - Individual box types

TEST(ISOBox, OriginalFormatBox)
{
    Vector<uint8_t> bytes { 0x00, 0x00, 0x00, 0x0C, 'f', 'r', 'm', 'a', 'm', 'p', '4', 'a' };

    auto view = bytes.span();
    ISOOriginalFormatBox frma;
    ASSERT_TRUE(frma.read(view));
    EXPECT_EQ(FourCC('mp4a'), frma.dataFormat());
    EXPECT_EQ(12U, frma.size());

    EXPECT_EQ(bytes, readThenSerialize(frma, bytes));
}

TEST(ISOBox, SchemeTypeBox)
{
    Vector<uint8_t> bytes {
        0x00, 0x00, 0x00, 0x14, 's', 'c', 'h', 'm',
        0x00, 0x00, 0x00, 0x00,
        'c', 'b', 'c', 's',
        0x00, 0x01, 0x00, 0x00,
    };

    auto view = bytes.span();
    ISOSchemeTypeBox schm;
    ASSERT_TRUE(schm.read(view));
    EXPECT_EQ(FourCC('cbcs'), schm.schemeType());
    EXPECT_EQ(0x10000U, schm.schemeVersion());

    EXPECT_EQ(bytes, readThenSerialize(schm, bytes));
}

// The version 1 'tenc' payload carries a crypt/skip byte pattern, which the version 0
// payload does not. The sinf fixture above only exercises a constant-IV box, so cover
// the pattern-encryption path explicitly.
TEST(ISOBox, TrackEncryptionBox_V1PatternEncryption)
{
    Vector<uint8_t> bytes {
        0x00, 0x00, 0x00, 0x20, 't', 'e', 'n', 'c',
        0x01, 0x00, 0x00, 0x00, // version 1, flags 0
        0x00, // reserved
        0x19, // cryptByteBlock = 1, skipByteBlock = 9
        0x01, // defaultIsProtected
        0x08, // defaultPerSampleIVSize (non-zero, so no constant IV)
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    };

    auto view = bytes.span();
    ISOTrackEncryptionBox tenc;
    ASSERT_TRUE(tenc.read(view));

    ASSERT_TRUE(tenc.defaultCryptByteBlock());
    EXPECT_EQ(1, *tenc.defaultCryptByteBlock());
    ASSERT_TRUE(tenc.defaultSkipByteBlock());
    EXPECT_EQ(9, *tenc.defaultSkipByteBlock());
    EXPECT_EQ(1, tenc.defaultIsProtected());
    EXPECT_EQ(8, tenc.defaultPerSampleIVSize());

    Vector<uint8_t> expectedKID { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 };
    EXPECT_EQ(expectedKID, tenc.defaultKID());
    EXPECT_TRUE(tenc.defaultConstantIV().isEmpty());

    EXPECT_EQ(bytes, readThenSerialize(tenc, bytes));
}

// A version 1 'pssh' carries an explicit key ID list ahead of its data payload; a
// version 0 'pssh' does not.
static constexpr auto base64EncodedPsshV1WithKeyIDs = "AAAASHBzc2gBAAAAEHfv7MCyTQKs4zweUuL7SwAAAAIAAAAAAAAAAAAAAAAAAACqAAAAAAAAAAAAAAAAAAAAuwAAAAREQVRB"_s;

TEST(ISOBox, ProtectionSystemSpecificHeaderBox_V1KeyIDs)
{
    auto psshArray = decodeBase64(base64EncodedPsshV1WithKeyIDs);
    ASSERT_EQ(72UL, psshArray.size());

    auto view = psshArray.span();
    ISOProtectionSystemSpecificHeaderBox pssh;
    ASSERT_TRUE(pssh.read(view));

    EXPECT_EQ(ISOProtectionSystemSpecificHeaderBox::commonSystemID(), pssh.systemID());

    auto& keyIDs = pssh.keyIDs();
    ASSERT_EQ(2UL, keyIDs.size());
    EXPECT_EQ(0xAA, keyIDs[0][15]);
    EXPECT_EQ(0xBB, keyIDs[1][15]);

    Vector<uint8_t> expectedData { 'D', 'A', 'T', 'A' };
    EXPECT_EQ(expectedData, pssh.data());

    EXPECT_EQ(psshArray, readThenSerialize(pssh, psshArray));
}

// 'vttc' and its string sub-boxes had no coverage at all.
static constexpr auto base64EncodedWebVTTCue = "AAAAPnZ0dGMAAAANaWRlbmN1ZS0xAAAAFHN0dGdhbGlnbjpjZW50ZXIAAAAVcGF5bEhlbGxvLCB3b3JsZCE="_s;

TEST(ISOBox, WebVTTCue)
{
    auto cueArray = decodeBase64(base64EncodedWebVTTCue);
    ASSERT_EQ(62UL, cueArray.size());

    auto view = cueArray.span();
    ISOWebVTTCue cue;
    ASSERT_TRUE(cue.read(view));

    EXPECT_STREQ("cue-1", cue.id().utf8().data());
    EXPECT_STREQ("align:center", cue.settings().utf8().data());
    EXPECT_STREQ("Hello, world!", cue.cueText().utf8().data());
    EXPECT_TRUE(cue.sourceID().isEmpty());
    EXPECT_TRUE(cue.originalStartTime().isEmpty());
}

// 'fkvl' carries 32-bit key versions. Storing them in a byte vector silently truncated any
// value above 255, and made partialSize() report one byte per version where parse consumes
// four.
TEST(ISOBox, FairPlayStreamingKeyRequestBox_VersionList)
{
    auto bytes = decodeBase64("AAAAOGZwc2sAAAAcZmtyaQAAAAABAgMEBQYHCAkKCwwNDg8QAAAAFGZrdmwAAAABAAASNN6tvu8="_s);
    ASSERT_EQ(56UL, bytes.size());

    auto view = bytes.span();
    ISOFairPlayStreamingKeyRequestBox fpsk;
    ASSERT_TRUE(fpsk.read(view));

    Vector<uint8_t> expectedKeyID { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 };
    EXPECT_EQ(expectedKeyID, fpsk.requestInfo().keyID());

    ASSERT_TRUE(fpsk.versionList());
    Vector<uint32_t> expectedVersions { 1U, 0x1234U, 0xDEADBEEFU };
    EXPECT_EQ(expectedVersions, fpsk.versionList()->versions());

    // The versions are 32-bit, so the writer has to emit four bytes each; writing them as a
    // byte sequence would narrow each one and leave the rest of the reserved space zero-filled.
    // Note this needs a fresh box: parse() rejects a second read once an optional sub-box is
    // already present.
    ISOFairPlayStreamingKeyRequestBox roundtripped;
    EXPECT_EQ(bytes, readThenSerialize(roundtripped, bytes));
}

// A version 0 'tenc' whose defaultPerSampleIVSize is zero carries a trailing constant IV,
// which the sinf fixture above does exercise but only as part of a larger tree. Cover the
// standalone read/write of that trailing field.
//
// Note the writer also refuses a constant IV longer than 127 bytes, since the length is
// encoded as a single signed byte; that guard is not reachable through the public API today
// because there is no setter for the IV, so it is not covered here.
TEST(ISOBox, TrackEncryptionBox_V0ConstantIV)
{
    Vector<uint8_t> bytes {
        0x00, 0x00, 0x00, 0x23, 't', 'e', 'n', 'c',
        0x00, 0x00, 0x00, 0x00, // version 0, flags 0
        0x00, // reserved
        0x00, // reserved
        0x01, // defaultIsProtected
        0x00, // defaultPerSampleIVSize == 0, so a constant IV follows
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x02, 0xAA, 0xBB, // constant IV: length byte + 2 bytes
    };

    auto view = bytes.span();
    ISOTrackEncryptionBox tenc;
    ASSERT_TRUE(tenc.read(view));

    EXPECT_EQ(1, tenc.defaultIsProtected());
    EXPECT_EQ(0, tenc.defaultPerSampleIVSize());
    EXPECT_FALSE(tenc.defaultCryptByteBlock());
    EXPECT_FALSE(tenc.defaultSkipByteBlock());

    Vector<uint8_t> expectedIV { 0xAA, 0xBB };
    EXPECT_EQ(expectedIV, tenc.defaultConstantIV());

    EXPECT_EQ(bytes, readThenSerialize(tenc, bytes));
}

// MARK: - Construct-from-scratch round-trips
//
// A parse-then-serialize test inherits m_size from its input, so it never exercises a box's
// own size computation. Building a box programmatically does: updateSize() has to derive the
// size from partialSize() alone, and serialize() then allocates from it. This is the shape
// CDMPrivateFairPlayStreaming uses to synthesize a PSSH from a set of key IDs.

TEST(ISOBox, FairPlayStreamingPsshBox_ConstructedRoundtrip)
{
    ISOFairPlayStreamingKeyRequestInfoBox::KeyID firstKeyID { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    ISOFairPlayStreamingKeyRequestInfoBox::KeyID secondKeyID { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 };

    ISOFairPlayStreamingPsshBox pssh;
    // A default-constructed box is already a well-formed empty 'pssh' carrying the FairPlay
    // system ID, so callers only supply the init data.
    EXPECT_EQ(FourCC('pssh'), pssh.boxType());
    EXPECT_EQ(ISOFairPlayStreamingPsshBox::fairPlaySystemID(), pssh.systemID());

    pssh.initDataBox().info().setScheme(FourCC('cbcs'));

    Vector<ISOFairPlayStreamingKeyRequestBox> requests;
    for (auto& keyID : { firstKeyID, secondKeyID }) {
        ISOFairPlayStreamingKeyRequestBox request;
        request.requestInfo().setKeyID(keyID);
        requests.append(WTF::move(request));
    }
    pssh.initDataBox().setRequests(WTF::move(requests));

    pssh.updateSize();

    // 'pssh' version/flags + systemID + DataSize (32) wrapping an 'fpsd' of 96: its own header
    // (8) plus an 'fpsi' of 16 and two 'fpsk' of 36, each an 8-byte header over a 28-byte
    // 'fkri'. Every one of those contributions comes from a partialSize() override.
    EXPECT_EQ(128U, pssh.size());

    auto serialized = pssh.serialize();
    auto bytes = Vector<uint8_t> { serialized->span() };
    ASSERT_EQ(128UL, bytes.size());

    ISOFairPlayStreamingPsshBox reparsed;
    auto view = bytes.span();
    ASSERT_TRUE(reparsed.read(view));

    EXPECT_EQ(FourCC('pssh'), reparsed.boxType());
    EXPECT_EQ(ISOFairPlayStreamingPsshBox::fairPlaySystemID(), reparsed.systemID());
    EXPECT_EQ(FourCC('cbcs'), reparsed.initDataBox().info().scheme());

    auto& reparsedRequests = reparsed.initDataBox().requests();
    ASSERT_EQ(2UL, reparsedRequests.size());
    EXPECT_EQ(firstKeyID, reparsedRequests[0].requestInfo().keyID());
    EXPECT_EQ(secondKeyID, reparsedRequests[1].requestInfo().keyID());

    // Nothing set these, so they must not appear in the output.
    EXPECT_FALSE(reparsedRequests[0].assetID());
    EXPECT_FALSE(reparsedRequests[0].versionList());
    EXPECT_FALSE(reparsedRequests[1].assetID());
    EXPECT_FALSE(reparsedRequests[1].versionList());
}

// The leaf payload boxes expose non-const accessors so a caller can fill them in. Each one is
// checked standalone as well as through the tree below, because a size mismatch anywhere in the
// tree fails the whole serialization: testing the leaves alone says which box got it wrong.

TEST(ISOBox, FairPlayStreamingKeyAssetIdBox_ConstructedRoundtrip)
{
    Vector<uint8_t> assetIDData { 0xF1, 0xF2, 0xF3, 0xF4, 0xF5 };

    ISOFairPlayStreamingKeyAssetIdBox fkai;
    EXPECT_EQ(FourCC('fkai'), fkai.boxType());
    fkai.data() = assetIDData;

    auto bytes = updateSizeThenSerialize(fkai);
    Vector<uint8_t> expected {
        0x00, 0x00, 0x00, 0x0D, 'f', 'k', 'a', 'i',
        0xF1, 0xF2, 0xF3, 0xF4, 0xF5,
    };
    EXPECT_EQ(expected, bytes);

    ISOFairPlayStreamingKeyAssetIdBox reparsed;
    ASSERT_TRUE(reparsed.read(bytes.span()));
    EXPECT_EQ(assetIDData, reparsed.data());
}

TEST(ISOBox, FairPlayStreamingKeyContextBox_ConstructedRoundtrip)
{
    Vector<uint8_t> contextData { 0xC0, 0xC1, 0xC2, 0xC3, 0xC4 };

    ISOFairPlayStreamingKeyContextBox fkcx;
    EXPECT_EQ(FourCC('fkcx'), fkcx.boxType());
    fkcx.data() = contextData;

    auto bytes = updateSizeThenSerialize(fkcx);
    Vector<uint8_t> expected {
        0x00, 0x00, 0x00, 0x0D, 'f', 'k', 'c', 'x',
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4,
    };
    EXPECT_EQ(expected, bytes);

    ISOFairPlayStreamingKeyContextBox reparsed;
    ASSERT_TRUE(reparsed.read(bytes.span()));
    EXPECT_EQ(contextData, reparsed.data());
}

// The version values are 32-bit, so four bytes have to be reserved and written per entry. A
// byte-wide size or write would put the box's declared size and its payload out of step.
TEST(ISOBox, FairPlayStreamingKeyVersionListBox_ConstructedRoundtrip)
{
    Vector<uint32_t> versions { 1U, 0x1234U, 0xDEADBEEFU };

    ISOFairPlayStreamingKeyVersionListBox fkvl;
    EXPECT_EQ(FourCC('fkvl'), fkvl.boxType());
    fkvl.versions() = versions;

    auto bytes = updateSizeThenSerialize(fkvl);
    Vector<uint8_t> expected {
        0x00, 0x00, 0x00, 0x14, 'f', 'k', 'v', 'l',
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x12, 0x34,
        0xDE, 0xAD, 0xBE, 0xEF,
    };
    EXPECT_EQ(expected, bytes);

    ISOFairPlayStreamingKeyVersionListBox reparsed;
    ASSERT_TRUE(reparsed.read(bytes.span()));
    EXPECT_EQ(versions, reparsed.versions());
}

// An empty payload is legal and must not be confused with a truncated one: 'fkai' and 'fkcx'
// both special-case a box whose declared size covers only its header.
TEST(ISOBox, FairPlayStreamingKeyAssetIdBox_ConstructedEmpty)
{
    ISOFairPlayStreamingKeyAssetIdBox fkai;
    auto bytes = updateSizeThenSerialize(fkai);

    Vector<uint8_t> expected { 0x00, 0x00, 0x00, 0x08, 'f', 'k', 'a', 'i' };
    EXPECT_EQ(expected, bytes);

    ISOFairPlayStreamingKeyAssetIdBox reparsed;
    ASSERT_TRUE(reparsed.read(bytes.span()));
    EXPECT_TRUE(reparsed.data().isEmpty());
}

// 'fkri' carries a fixed-width key ID, and parse() requires those bytes to be present, so a
// default-constructed box starts out with a zero-filled one. Without that, a request built from
// scratch and never given a key ID would serialize to bytes it could not read back.
TEST(ISOBox, FairPlayStreamingKeyRequestInfoBox_DefaultKeyIDIsZeroFilled)
{
    ISOFairPlayStreamingKeyRequestInfoBox fkri;
    EXPECT_EQ(FourCC('fkri'), fkri.boxType());

    Vector<uint8_t, 16> expectedKeyID { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    EXPECT_EQ(expectedKeyID, fkri.keyID());

    auto bytes = updateSizeThenSerialize(fkri);
    Vector<uint8_t> expected {
        0x00, 0x00, 0x00, 0x1C, 'f', 'k', 'r', 'i',
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    EXPECT_EQ(expected, bytes);

    ISOFairPlayStreamingKeyRequestInfoBox reparsed;
    ASSERT_TRUE(reparsed.read(bytes.span()));
    EXPECT_EQ(expectedKeyID, reparsed.keyID());
}

// All three optional sub-boxes at once, with the exact bytes spelled out: 'fpsk' writes them in
// a fixed order (asset ID, context, version list) that a size-only assertion would not pin down.
TEST(ISOBox, FairPlayStreamingKeyRequestBox_ConstructedWithAllSubBoxes)
{
    ISOFairPlayStreamingKeyRequestInfoBox::KeyID keyID { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    Vector<uint8_t> assetIDData { 0xA1, 0xA2, 0xA3, 0xA4 };
    Vector<uint8_t> contextData { 0xC0, 0xC1, 0xC2, 0xC3, 0xC4 };
    Vector<uint32_t> versions { 1U, 0xDEADBEEFU };

    ISOFairPlayStreamingKeyRequestBox fpsk;
    fpsk.requestInfo().setKeyID(keyID);

    ISOFairPlayStreamingKeyAssetIdBox assetID;
    assetID.data() = assetIDData;
    fpsk.setAssetID(WTF::move(assetID));

    ISOFairPlayStreamingKeyContextBox context;
    context.data() = contextData;
    fpsk.setContent(WTF::move(context));

    ISOFairPlayStreamingKeyVersionListBox versionList;
    versionList.versions() = versions;
    fpsk.setVersionList(WTF::move(versionList));

    auto bytes = updateSizeThenSerialize(fpsk);

    // 8-byte 'fpsk' header over a 28-byte 'fkri', a 12-byte 'fkai', a 13-byte 'fkcx' and a
    // 16-byte 'fkvl'.
    Vector<uint8_t> expected {
        0x00, 0x00, 0x00, 0x4D, 'f', 'p', 's', 'k',
        0x00, 0x00, 0x00, 0x1C, 'f', 'k', 'r', 'i',
        0x00, 0x00, 0x00, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x00, 0x00, 0x00, 0x0C, 'f', 'k', 'a', 'i',
        0xA1, 0xA2, 0xA3, 0xA4,
        0x00, 0x00, 0x00, 0x0D, 'f', 'k', 'c', 'x',
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4,
        0x00, 0x00, 0x00, 0x10, 'f', 'k', 'v', 'l',
        0x00, 0x00, 0x00, 0x01, 0xDE, 0xAD, 0xBE, 0xEF,
    };
    EXPECT_EQ(expected, bytes);
    EXPECT_EQ(77U, fpsk.size());

    ISOFairPlayStreamingKeyRequestBox reparsed;
    ASSERT_TRUE(reparsed.read(bytes.span()));

    EXPECT_EQ(keyID, reparsed.requestInfo().keyID());
    ASSERT_TRUE(reparsed.assetID());
    EXPECT_EQ(assetIDData, reparsed.assetID()->data());
    ASSERT_TRUE(reparsed.content());
    EXPECT_EQ(contextData, reparsed.content()->data());
    ASSERT_TRUE(reparsed.versionList());
    EXPECT_EQ(versions, reparsed.versionList()->versions());
}

// The same request nested in a 'pssh', so every enclosing partialSize() has to account for the
// optional sub-boxes rather than just the required 'fkri'.
TEST(ISOBox, FairPlayStreamingPsshBox_ConstructedWithAllSubBoxes)
{
    ISOFairPlayStreamingKeyRequestInfoBox::KeyID keyID { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    Vector<uint8_t> assetIDData { 0xA1, 0xA2, 0xA3, 0xA4 };
    Vector<uint8_t> contextData { 0xC0, 0xC1, 0xC2, 0xC3, 0xC4 };
    Vector<uint32_t> versions { 1U, 0xDEADBEEFU };

    ISOFairPlayStreamingKeyRequestBox request;
    request.requestInfo().setKeyID(keyID);

    ISOFairPlayStreamingKeyAssetIdBox assetID;
    assetID.data() = assetIDData;
    request.setAssetID(WTF::move(assetID));

    ISOFairPlayStreamingKeyContextBox context;
    context.data() = contextData;
    request.setContent(WTF::move(context));

    ISOFairPlayStreamingKeyVersionListBox versionList;
    versionList.versions() = versions;
    request.setVersionList(WTF::move(versionList));

    ISOFairPlayStreamingPsshBox pssh;
    pssh.initDataBox().info().setScheme(FourCC('cbcs'));

    Vector<ISOFairPlayStreamingKeyRequestBox> requests;
    requests.append(WTF::move(request));
    pssh.initDataBox().setRequests(WTF::move(requests));

    auto bytes = updateSizeThenSerialize(pssh);

    // 'pssh' header (32) over an 'fpsd' of 101: its own header (8) plus a 16-byte 'fpsi' and the
    // 77-byte 'fpsk' checked above.
    EXPECT_EQ(133U, pssh.size());
    ASSERT_EQ(133UL, bytes.size());

    ISOFairPlayStreamingPsshBox reparsed;
    ASSERT_TRUE(reparsed.read(bytes.span()));

    EXPECT_EQ(ISOFairPlayStreamingPsshBox::fairPlaySystemID(), reparsed.systemID());
    EXPECT_EQ(FourCC('cbcs'), reparsed.initDataBox().info().scheme());

    auto& reparsedRequests = reparsed.initDataBox().requests();
    ASSERT_EQ(1UL, reparsedRequests.size());
    EXPECT_EQ(keyID, reparsedRequests[0].requestInfo().keyID());
    ASSERT_TRUE(reparsedRequests[0].assetID());
    EXPECT_EQ(assetIDData, reparsedRequests[0].assetID()->data());
    ASSERT_TRUE(reparsedRequests[0].content());
    EXPECT_EQ(contextData, reparsedRequests[0].content()->data());
    ASSERT_TRUE(reparsedRequests[0].versionList());
    EXPECT_EQ(versions, reparsedRequests[0].versionList()->versions());
}

// Clearing an optional sub-box has to shrink the enclosing boxes, not just leave a hole: this is
// the reverse of the resize test below, and catches a partialSize() that only ever grows.
TEST(ISOBox, FairPlayStreamingKeyRequestBox_ClearedOptionalsShrinkBox)
{
    ISOFairPlayStreamingKeyRequestBox fpsk;

    ISOFairPlayStreamingKeyContextBox context;
    context.data() = Vector<uint8_t> { 0xC0, 0xC1, 0xC2, 0xC3, 0xC4 };
    fpsk.setContent(WTF::move(context));

    fpsk.updateSize();
    EXPECT_EQ(49U, fpsk.size());

    fpsk.setContent(std::nullopt);
    auto bytes = updateSizeThenSerialize(fpsk);

    // Back to an 8-byte header over the required 28-byte 'fkri' alone.
    EXPECT_EQ(36U, fpsk.size());
    ASSERT_EQ(36UL, bytes.size());

    ISOFairPlayStreamingKeyRequestBox reparsed;
    ASSERT_TRUE(reparsed.read(bytes.span()));
    EXPECT_FALSE(reparsed.content());
    EXPECT_FALSE(reparsed.assetID());
    EXPECT_FALSE(reparsed.versionList());
}

// Builds the same box the base64 fixture above holds, from scratch, and requires the bytes to
// match it exactly. The fixture is known-good input for the parser, so this pins the writer and
// every partialSize() in the tree ('fpsi', 'fkri', 'fkai', 'fpsk', 'fpsd', 'pssh') against it
// without hand-asserting an expected size at each level.
TEST(ISOBox, FairPlayStreamingPsshBox_ConstructedMatchesFixture)
{
    auto expected = decodeBase64(base64EncodedPsshWithAssetId);
    ASSERT_EQ(176UL, expected.size());

    auto makeRequest = [](uint8_t keyIDTail, uint8_t assetIDTail) {
        ISOFairPlayStreamingKeyRequestBox request;
        request.requestInfo().setKeyID(ISOFairPlayStreamingKeyRequestInfoBox::KeyID { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, keyIDTail });

        ISOFairPlayStreamingKeyAssetIdBox assetID;
        assetID.data() = Vector<uint8_t> { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, assetIDTail };
        request.setAssetID(WTF::move(assetID));

        return request;
    };

    ISOFairPlayStreamingPsshBox pssh;
    pssh.initDataBox().info().setScheme(FourCC('cenc'));

    Vector<ISOFairPlayStreamingKeyRequestBox> requests;
    requests.append(makeRequest(0x01, 0xF1));
    requests.append(makeRequest(0x02, 0xF2));
    pssh.initDataBox().setRequests(WTF::move(requests));

    EXPECT_EQ(expected, updateSizeThenSerialize(pssh));
    EXPECT_EQ(176U, pssh.size());
}

// Swapping a parsed box's 16-byte asset ID for a 20-byte one has to grow every enclosing box by
// the same four bytes. That only happens if each partialSize() measures the box it contains
// rather than reusing the size it parsed, so this catches a stale size that the fixture tests --
// where the computed and parsed sizes coincide -- cannot distinguish.
TEST(ISOBox, FairPlayStreamingPsshBox_ResizedAssetIDUpdatesEnclosingSizes)
{
    auto psshArray = decodeBase64(base64EncodedPsshWithAssetId);
    ASSERT_EQ(176UL, psshArray.size());

    ISOFairPlayStreamingPsshBox pssh;
    ASSERT_TRUE(pssh.read(psshArray.span()));
    ASSERT_EQ(176U, pssh.size());

    auto requests = pssh.initDataBox().requests();
    ASSERT_EQ(2UL, requests.size());

    ISOFairPlayStreamingKeyAssetIdBox widerAssetID;
    widerAssetID.data() = Vector<uint8_t> { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF9 };
    requests[0].setAssetID(WTF::move(widerAssetID));
    pssh.initDataBox().setRequests(WTF::move(requests));

    auto bytes = updateSizeThenSerialize(pssh);
    EXPECT_EQ(180U, pssh.size());
    ASSERT_EQ(180UL, bytes.size());

    ISOFairPlayStreamingPsshBox reparsed;
    ASSERT_TRUE(reparsed.read(bytes.span()));

    auto& reparsedRequests = reparsed.initDataBox().requests();
    ASSERT_EQ(2UL, reparsedRequests.size());

    Vector<uint8_t> expectedFirstAssetID { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF9 };
    ASSERT_TRUE(reparsedRequests[0].assetID());
    EXPECT_EQ(expectedFirstAssetID, reparsedRequests[0].assetID()->data());

    // The request that was not touched has to come back byte-for-byte, so the extra four bytes
    // went into the first asset ID rather than shifting the boxes that follow it.
    Vector<uint8_t> expectedSecondAssetID { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF2 };
    ASSERT_TRUE(reparsedRequests[1].assetID());
    EXPECT_EQ(expectedSecondAssetID, reparsedRequests[1].assetID()->data());

    Vector<uint8_t, 16> expectedSecondKeyID { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 };
    EXPECT_EQ(expectedSecondKeyID, reparsedRequests[1].requestInfo().keyID());
}

// MARK: - Malformed input
//
// Every bound in ISOProtectionSystemSpecificHeaderBox::parse is derived from the box's
// declared size, which ISOBox::parse only clamps down to the bytes left in the view — never
// up to the size of the box's own header. These cover the resulting arithmetic edges.

// A box declaring a size (8) smaller than its own header must be rejected. Version 0 with a
// zero dataSize, so no other bound can reject it incidentally: without the size invariant
// this parses "successfully" while having consumed far more bytes than it claims to occupy.
TEST(ISOBox, ProtectionSystemSpecificHeaderBox_RejectsSizeSmallerThanHeader)
{
    auto bytes = decodeBase64("AAAACHBzc2gAAAAAEHfv7MCyTQKs4zweUuL7SwAAAADMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzA=="_s);
    ASSERT_FALSE(bytes.isEmpty());

    auto view = bytes.span();
    ISOProtectionSystemSpecificHeaderBox pssh;
    EXPECT_FALSE(pssh.read(view));
}

// keyIDCount == 2^28 makes `keyIDCount * sizeof(KeyID)` wrap to exactly 0 when computed in
// 32-bit arithmetic, defeating the bound on the key ID list.
TEST(ISOBox, ProtectionSystemSpecificHeaderBox_RejectsOverflowingKeyIDCount)
{
    auto bytes = decodeBase64("AAAAYHBzc2gBAAAAEHfv7MCyTQKs4zweUuL7SxAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"_s);
    ASSERT_FALSE(bytes.isEmpty());

    auto view = bytes.span();
    ISOProtectionSystemSpecificHeaderBox pssh;
    EXPECT_FALSE(pssh.read(view));
    EXPECT_TRUE(pssh.keyIDs().isEmpty());
}

// A dataSize that reaches past the box's own end but stays inside the view must be rejected;
// bounding only by the view would let the box absorb the sibling bytes that follow it.
TEST(ISOBox, ProtectionSystemSpecificHeaderBox_RejectsDataSizePastBoxEnd)
{
    auto bytes = decodeBase64("AAAAKHBzc2gAAAAAEHfv7MCyTQKs4zweUuL7SwAAAECqqqqqqqqqqru7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7u7s="_s);
    ASSERT_FALSE(bytes.isEmpty());

    auto view = bytes.span();
    ISOProtectionSystemSpecificHeaderBox pssh;
    EXPECT_FALSE(pssh.read(view));
    EXPECT_TRUE(pssh.data().isEmpty());
}

}
