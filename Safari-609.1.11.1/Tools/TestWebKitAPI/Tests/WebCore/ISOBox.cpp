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

#include "Test.h"
#include <JavaScriptCore/DataView.h>
#include <WebCore/ISOFairPlayStreamingPsshBox.h>
#include <WebCore/ISOProtectionSchemeInfoBox.h>
#include <WebCore/ISOSchemeInformationBox.h>
#include <WebCore/ISOSchemeTypeBox.h>
#include <WebCore/ISOTrackEncryptionBox.h>
#include <wtf/text/Base64.h>

using namespace WebCore;

namespace TestWebKitAPI {

static const char* base64EncodedSinfWithKeyID3 = "AAAAYXNpbmYAAAAMZnJtYW1wNGEAAAAUc2NobQAAAABjYmNzAAEAAAAAADlzY2hpAAAAMXRlbmMBAAAAAAABAAAAAAAAAAAAAAAAAAAAAAMQ1fvWuC7ZPk75iuQJMe4ztw==";

TEST(ISOBox, ISOProtectionSchemeInfoBox)
{
    Vector<uint8_t> sinfArray;
    ASSERT_TRUE(base64Decode(StringView(base64EncodedSinfWithKeyID3), sinfArray));
    ASSERT_EQ(97UL, sinfArray.size());

    auto view = JSC::DataView::create(ArrayBuffer::create(sinfArray.data(), sinfArray.size()), 0, sinfArray.size());

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

static const char* base64EncodedPsshWithAssetId = "AAAAsHBzc2gAAAAAlM6G+wf/T0OtuJPS+paMogAAAJAAAACQZnBzZAAAABBmcHNpAAAAAGNlbmMAAAA8ZnBzawAAABxma3JpAAAAAAAAAAAAAAAAAAAAAAAAAAEAAAAYZmthaQAAAAAAAAAAAAAAAAAAAPEAAAA8ZnBzawAAABxma3JpAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAAYZmthaQAAAAAAAAAAAAAAAAAAAPI=";

TEST(ISOBox, ISOFairPlayStreamingPsshBox)
{
    Vector<uint8_t> psshArray;
    ASSERT_TRUE(base64Decode(StringView(base64EncodedPsshWithAssetId), psshArray));
    ASSERT_EQ(176UL, psshArray.size());

    auto view = JSC::DataView::create(ArrayBuffer::create(psshArray.data(), psshArray.size()), 0, psshArray.size());

    ISOFairPlayStreamingPsshBox psshBox;

    ASSERT_TRUE(psshBox.read(view));

    auto infoBox = psshBox.initDataBox().info();
    ASSERT_EQ(FourCC('cenc'), infoBox.scheme());

    auto requests = psshBox.initDataBox().requests();
    ASSERT_EQ(2UL, requests.size());

    Vector<uint8_t, 16> expectedFirstKeyID = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    ASSERT_EQ(requests[0].requestInfo().keyID(), expectedFirstKeyID);

    Vector<uint8_t> expectedFirstAssetID = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF1 };
    ASSERT_TRUE(requests[0].assetID());
    ASSERT_EQ(requests[0].assetID().value().data(), expectedFirstAssetID);

    Vector<uint8_t, 16> expectedSecondKeyID = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    ASSERT_EQ(requests[1].requestInfo().keyID(), expectedSecondKeyID);

    Vector<uint8_t> expectedSecondAssetID = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF2 };
    ASSERT_TRUE(requests[1].assetID());
    ASSERT_EQ(requests[1].assetID().value().data(), expectedSecondAssetID);
}

}
