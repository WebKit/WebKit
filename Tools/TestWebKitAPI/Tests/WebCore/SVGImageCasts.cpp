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

#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageObserver.h>
#include <WebCore/PixelBuffer.h>
#include <WebCore/PlatformStrategies.h>
#include <WebCore/SVGImage.h>
#include <WebCore/SVGImageForContainer.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/URL.h>

namespace TestWebKitAPI {

using namespace WebCore;

class TestImageObserver : public ImageObserver {
public:
    static Ref<TestImageObserver> create()
    {
        return adoptRef(*new TestImageObserver);
    }

private:
    TestImageObserver() = default;

    URL sourceUrl() const final
    {
        return URL();
    }

    String mimeType() const final
    {
        return String();
    }

    long long expectedContentLength() const final
    {
        return 0;
    }

    void decodedSizeChanged(const Image&, long long) final
    {
    }

    void didDraw(const Image&) final
    {
    }

    void imageFrameAvailable(const Image&, ImageAnimatingState, const IntRect* changeRect = nullptr, DecodingStatus = DecodingStatus::Invalid) final
    {
    }

    void changedInRect(const Image&, const IntRect* changeRect = nullptr) final
    {
    }

    void scheduleRenderingUpdate(const Image&) final
    {
    }
};

TEST(SVGImageCasts, SVGImageForContainerIsNotSVGImage)
{
    auto imageObserver = TestImageObserver::create();
    auto svgImage = SVGImage::create(imageObserver.ptr());
    Image& svgImageBase = svgImage.get();
    EXPECT_TRUE(is<SVGImage>(svgImageBase));
    EXPECT_FALSE(is<SVGImageForContainer>(svgImageBase));
    auto svgImageForContainer = SVGImageForContainer::create(svgImage.ptr(), { }, 0, URL());
    Image& svgImageForContainerBase = svgImageForContainer.get();
    EXPECT_FALSE(is<SVGImage>(svgImageForContainerBase));
    EXPECT_TRUE(is<SVGImageForContainer>(svgImageForContainerBase));
}

TEST(SVGImageCasts, SVGImageDataToNativeImage)
{
    auto size = IntSize { 300, 150 };
    auto rect = IntRect { IntPoint::zero(), size };

    auto svgImage = SVGImage::create();
    const auto simpleText = "<svg xmlns='http://www.w3.org/2000/svg'><rect width='200' height='100' x='50' y='25' rx='20' ry='20' fill='rgb(0, 255, 0)' /></svg>"_span;
    auto buffer = FragmentedSharedBuffer::create(simpleText);

    PlatformStrategies platformStrategies;
    setPlatformStrategies(&platformStrategies);
    svgImage->setData(WTFMove(buffer), true);
    EXPECT_EQ(svgImage->size(), size);

    auto nativeImage = svgImage->nativeImage();
    ASSERT_NE(nativeImage, nullptr);
    EXPECT_EQ(nativeImage->size(), size);

    auto imageBuffer = ImageBuffer::create(size, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    ASSERT_NE(imageBuffer, nullptr);

    imageBuffer->context().drawNativeImage(*nativeImage, rect, rect);

    auto format = PixelBufferFormat { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, DestinationColorSpace::SRGB() };
    auto pixelBuffer = imageBuffer->getPixelBuffer(format, { rect.center(), { 1, 1 } });
    ASSERT_NE(pixelBuffer, nullptr);

    auto pixel = Color { SRGBA<uint8_t> { pixelBuffer->item(0), pixelBuffer->item(1), pixelBuffer->item(2), pixelBuffer->item(3) } };
    EXPECT_EQ(pixel, Color::green);
}

}
