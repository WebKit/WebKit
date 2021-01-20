/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2012 peavo@outlook.com All rights reserved.
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
#include <WebCore/BitmapImage.h>
#include <WebCore/BitmapInfo.h>
#include <wtf/win/GDIObject.h>

using namespace WebCore;

namespace TestWebKitAPI {

// Test that there is no crash when BitmapImage::getHBITMAPOfSize() is called
// for an image with empty frames (BitmapImage::frameAtIndex(i) return null), WebKit Bug 102689.

TEST(WebCore, BitmapImageEmptyFrameTest)
{
    IntSize sz(16, 16);

    BitmapInfo bitmapInfo = BitmapInfo::create(sz);

    auto bmp = adoptGDIObject(CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, 0, 0, 0));

    RefPtr<Image> bitmapImageTest = BitmapImage::create(bmp.get());

    if (!bitmapImageTest)
        return;

    // Destroying decoded data will cause frameAtIndex(i) to return null.
    bitmapImageTest->destroyDecodedData();

    int bits[256];
    auto bitmap = adoptGDIObject(CreateBitmap(sz.width(), sz.height(), 1, 32, bits));

    bitmapImageTest->getHBITMAPOfSize(bitmap.get(), &sz);
}

} // namespace TestWebKitAPI
