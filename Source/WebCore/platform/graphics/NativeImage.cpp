/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "NativeImage.h"

#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "RenderingMode.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NativeImage);

#if !USE(CG) && !USE(SKIA)
RefPtr<NativeImage> NativeImage::create(PlatformImagePtr&& platformImage)
{
    if (!platformImage)
        return nullptr;
    return adoptRef(*new NativeImage(WTF::move(platformImage)));
}

RefPtr<NativeImage> NativeImage::createTransient(PlatformImagePtr&& image)
{
    return create(WTF::move(image));
}
#endif

#if !USE(SKIA)
NativeImage::NativeImage(PlatformImagePtr&& platformImage)
    : m_platformImage(WTF::move(platformImage))
{
    computeHeadroom();
}
#endif

NativeImage::~NativeImage()
{
    for (CheckedRef observer : m_observers)
        observer->willDestroyNativeImage(*this);
}

const PlatformImagePtr& NativeImage::platformImage() const
{
    return m_platformImage;
}

bool NativeImage::hasHDRContent() const
{
    return colorSpace().usesITUR_2100TF();
}

void NativeImage::replacePlatformImage(PlatformImagePtr&& platformImage) const
{
    ASSERT(platformImage);
    m_platformImage = WTF::move(platformImage);
    computeHeadroom();
}

#if !USE(CG)
void NativeImage::computeHeadroom() const
{
}
#endif

} // namespace WebCore
