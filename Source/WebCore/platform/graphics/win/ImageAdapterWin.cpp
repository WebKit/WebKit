/*
 * Copyright (C) 2008-2024 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageAdapter.h"

#if PLATFORM(WIN)

#include "BitmapImage.h"
#include "SharedBuffer.h"
#include "WebCoreBundleWin.h"
#include <windows.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

Ref<Image> ImageAdapter::loadPlatformResource(const char *name)
{
    auto path = webKitBundlePath(StringView::fromLatin1(name), "png"_s, "icons"_s);
    auto data = FileSystem::readEntireFile(path);
    auto img = BitmapImage::create();
    if (data)
        img->setData(FragmentedSharedBuffer::create(WTFMove(*data)), true);
    return img;
}

void ImageAdapter::invalidate()
{
}

bool ImageAdapter::getHBITMAP(HBITMAP bmp)
{
    return getHBITMAPOfSize(bmp, 0);
}

} // namespace WebCore

#endif // PLATFORM(WIN)
