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

#import "config.h"
#import "GPUTexture.h"

#if ENABLE(WEBGPU)

#import "Logging.h"

#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

Ref<GPUTexture> GPUTexture::create(PlatformTextureSmartPtr&& texture)
{
    return adoptRef(*new GPUTexture(WTFMove(texture)));
}

GPUTexture::GPUTexture(PlatformTextureSmartPtr&& texture)
    : m_platformTexture(WTFMove(texture))
{
}

RefPtr<GPUTexture> GPUTexture::createDefaultTextureView()
{
    RetainPtr<MTLTexture> texture;

    texture = adoptNS([m_platformTexture newTextureViewWithPixelFormat:m_platformTexture.get().pixelFormat]);

    if (!texture) {
        LOG(WebGPU, "GPUTexture::createDefaultTextureView(): Unable to create MTLTexture view!");
        return nullptr;
    }

    return GPUTexture::create(WTFMove(texture));
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
