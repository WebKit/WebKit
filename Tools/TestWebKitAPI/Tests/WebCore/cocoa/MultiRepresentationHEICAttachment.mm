/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"

#if ENABLE(MULTI_REPRESENTATION_HEIC)

#import "Helpers/Test.h"
#import <CoreGraphics/CGImage.h>
#import <WebCore/NativeImage.h>
#import <WebCore/ShareableBitmap.h>
#import <WebCore/TextAttachmentForSerialization.h>
#import <limits>
#import <wtf/StdLibExtras.h>
#import <wtf/text/StringBuilder.h>

namespace TestWebKitAPI {

static WebCore::MultiRepresentationHEICAttachmentData representativeAttachmentData(size_t imageCount = 1)
{
    WebCore::MultiRepresentationHEICAttachmentData data;
    data.identifier = "com.apple.emoji.test"_s;
    data.description = "Test Emoji"_s;
    data.credit = "Apple Image Playground"_s;
    data.digitalSourceType = "http://cv.iptc.org/newscodes/digitalsourcetype/trainedAlgorithmicMedia"_s;

    for (size_t i = 0; i < imageCount; ++i)
        data.images.append(WebCore::MultiRepresentationHEICAttachmentSingleImage { nullptr, WebCore::FloatSize { 160, 160 } });

    return data;
}

static String stringOfLength(unsigned length)
{
    StringBuilder builder;
    for (unsigned i = 0; i < length; ++i)
        builder.append('a');
    return builder.toString();
}

TEST(MultiRepresentationHEICAttachment, AcceptsRepresentativeData)
{
    EXPECT_TRUE(representativeAttachmentData().isValid());
    EXPECT_TRUE(representativeAttachmentData(0).isValid());
}

TEST(MultiRepresentationHEICAttachment, AcceptsUnboundedMetadataStrings)
{
    auto data = representativeAttachmentData();
    data.identifier = stringOfLength(1024 * 1024);
    data.description = stringOfLength(1024 * 1024);
    data.credit = stringOfLength(1024 * 1024);
    data.digitalSourceType = stringOfLength(1024 * 1024);
    EXPECT_TRUE(data.isValid());
}

TEST(MultiRepresentationHEICAttachment, AcceptsFiniteImageSize)
{
    EXPECT_TRUE(WebCore::isValidMultiRepresentationHEICImageSize(WebCore::FloatSize { 0, 0 }));
    EXPECT_TRUE(WebCore::isValidMultiRepresentationHEICImageSize(WebCore::FloatSize { -4, 12 }));
}

TEST(MultiRepresentationHEICAttachment, RejectsNonFiniteImageSize)
{
    for (float value : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() }) {
        EXPECT_FALSE(WebCore::isValidMultiRepresentationHEICImageSize(WebCore::FloatSize { value, 160 }));
        EXPECT_FALSE(WebCore::isValidMultiRepresentationHEICImageSize(WebCore::FloatSize { 160, value }));

        auto data = representativeAttachmentData();
        data.images[0].size = WebCore::FloatSize { value, 160 };
        EXPECT_FALSE(data.isValid());
    }
}

TEST(MultiRepresentationHEICAttachment, RejectsNonFiniteImageSizeInAnyImage)
{
    auto data = representativeAttachmentData(4);
    data.images[3].size = WebCore::FloatSize { 160, std::numeric_limits<float>::quiet_NaN() };
    EXPECT_FALSE(data.isValid());
}

TEST(MultiRepresentationHEICAttachment, AcceptsUnboundedImageCount)
{
    EXPECT_TRUE(representativeAttachmentData(4096).isValid());
}

static uint8_t firstByteOf(CGImageRef image)
{
    RetainPtr data = adoptCF(CGDataProviderCopyData(CGImageGetDataProvider(image)));
    EXPECT_NOT_NULL(data.get());
    return *CFDataGetBytePtr(data.get());
}

TEST(MultiRepresentationHEICAttachment, StrikeSnapshotDoesNotAliasSenderMemory)
{
    WebCore::ShareableBitmapConfiguration configuration = { WebCore::IntSize(100, 100) };
    RefPtr source = WebCore::ShareableBitmap::create(configuration);
    ASSERT_TRUE(source);
    memsetSpan(source->mutableSpan(), 0xAA);

    RefPtr aliasingNativeImage = source->createNativeImage();
    ASSERT_TRUE(aliasingNativeImage);

    RefPtr snapshot = WebCore::ShareableBitmap::createFromImagePixels(*aliasingNativeImage);
    ASSERT_TRUE(snapshot);
    RetainPtr snapshotImage = snapshot->createPlatformImage(WebCore::DontCopyBackingStore);
    ASSERT_TRUE(snapshotImage);

    memsetSpan(source->mutableSpan(), 0x55);

    EXPECT_EQ(firstByteOf(aliasingNativeImage->platformImage().get()), 0x55);
    EXPECT_EQ(firstByteOf(snapshotImage.get()), 0xAA);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MULTI_REPRESENTATION_HEIC)
