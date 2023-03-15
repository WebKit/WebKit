/*
 * Copyright (C) 2020-2023 Apple Inc.  All rights reserved.
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

#include <wtf/NeverDestroyed.h>

namespace WebCore {

RefPtr<NativeImage> NativeImage::create(PlatformImagePtr&& platformImage, RenderingResourceIdentifier renderingResourceIdentifier)
{
    if (!platformImage)
        return nullptr;
    return adoptRef(*new NativeImage(WTFMove(platformImage), renderingResourceIdentifier));
}

NativeImage::NativeImage(PlatformImagePtr&& platformImage, RenderingResourceIdentifier renderingResourceIdentifier)
    : m_platformImage(WTFMove(platformImage))
    , m_renderingResourceIdentifier(renderingResourceIdentifier)
{
    ASSERT(m_platformImage);
}

NativeImage::~NativeImage()
{
    for (auto observer : m_observers)
        observer->releaseNativeImage(m_renderingResourceIdentifier);
}

void NativeImage::setPlatformImage(PlatformImagePtr&& platformImage)
{
    ASSERT(platformImage);
    m_platformImage = WTFMove(platformImage);
}

} // namespace WebCore
