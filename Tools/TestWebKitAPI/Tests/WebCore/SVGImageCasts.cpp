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

#include <WebCore/ImageObserver.h>
#include <WebCore/SVGImage.h>
#include <WebCore/SVGImageForContainer.h>
#include <wtf/URL.h>

namespace TestWebKitAPI {

using namespace WebCore;

class TestImageObserver : public ImageObserver {
public:
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

    bool canDestroyDecodedData(const Image&) final
    {
        return true;
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
    TestImageObserver imageObserver;
    auto svgImage = SVGImage::create(imageObserver);
    Image& svgImageBase = svgImage.get();
    EXPECT_TRUE(is<SVGImage>(svgImageBase));
    EXPECT_FALSE(is<SVGImageForContainer>(svgImageBase));
    auto svgImageForContainer = SVGImageForContainer::create(svgImage.ptr(), { }, 0, URL());
    Image& svgImageForContainerBase = svgImageForContainer.get();
    EXPECT_FALSE(is<SVGImage>(svgImageForContainerBase));
    EXPECT_TRUE(is<SVGImageForContainer>(svgImageForContainerBase));
}

}
