/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "GPULegacyTexture.h"

#if ENABLE(WEBMETAL)

#import "GPULegacyDevice.h"
#import "GPULegacyDrawable.h"
#import "GPULegacyTextureDescriptor.h"
#import "Logging.h"
#import <Metal/Metal.h>

namespace WebCore {

GPULegacyTexture::GPULegacyTexture(const GPULegacyDevice& device, const GPULegacyTextureDescriptor& descriptor)
{
    LOG(WebMetal, "GPULegacyTexture::GPULegacyTexture()");

    if (!descriptor.metal())
        return;

    m_metal = adoptNS([device.metal() newTextureWithDescriptor:descriptor.metal()]);
}

GPULegacyTexture::GPULegacyTexture(const GPULegacyDrawable& other)
    : m_metal { other.texture() }
{
    LOG(WebMetal, "GPULegacyTexture::GPULegacyTexture()");
}

unsigned GPULegacyTexture::width() const
{
    return m_metal.get().width;
}

unsigned GPULegacyTexture::height() const
{
    return m_metal.get().height;
}

MTLTexture *GPULegacyTexture::metal() const
{
    return m_metal.get();
}

} // namespace WebCore

#endif
