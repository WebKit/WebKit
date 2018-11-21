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
#import "GPULegacyTextureDescriptor.h"

#if ENABLE(WEBMETAL)

#import "Logging.h"
#import <Metal/Metal.h>

namespace WebCore {

GPULegacyTextureDescriptor::GPULegacyTextureDescriptor(unsigned pixelFormat, unsigned width, unsigned height, bool mipmapped)
{
    LOG(WebMetal, "GPULegacyTextureDescriptor::GPULegacyTextureDescriptor()");

    m_metal = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:static_cast<MTLPixelFormat>(pixelFormat) width:width height:height mipmapped:mipmapped];
}

unsigned GPULegacyTextureDescriptor::width() const
{
    return [m_metal width];
}

void GPULegacyTextureDescriptor::setWidth(unsigned newWidth) const
{
    ASSERT(m_metal);
    [m_metal setWidth:newWidth];
}

unsigned GPULegacyTextureDescriptor::height() const
{
    return [m_metal height];
}

void GPULegacyTextureDescriptor::setHeight(unsigned newHeight) const
{
    ASSERT(m_metal);
    [m_metal setHeight:newHeight];
}

unsigned GPULegacyTextureDescriptor::sampleCount() const
{
    return [m_metal sampleCount];
}

void GPULegacyTextureDescriptor::setSampleCount(unsigned newSampleCount) const
{
    ASSERT(m_metal);
    [m_metal setSampleCount:newSampleCount];
}

unsigned GPULegacyTextureDescriptor::textureType() const
{
    return [m_metal textureType];
}

void GPULegacyTextureDescriptor::setTextureType(unsigned newTextureType) const
{
    ASSERT(m_metal);
    [m_metal setTextureType:static_cast<MTLTextureType>(newTextureType)];
}

unsigned GPULegacyTextureDescriptor::storageMode() const
{
    return [m_metal storageMode];
}

void GPULegacyTextureDescriptor::setStorageMode(unsigned newStorageMode) const
{
    [m_metal setStorageMode:static_cast<MTLStorageMode>(newStorageMode)];
}

unsigned GPULegacyTextureDescriptor::usage() const
{
    return [m_metal usage];
}

void GPULegacyTextureDescriptor::setUsage(unsigned newUsage) const
{
    ASSERT(m_metal);
    [m_metal setUsage:newUsage];
}

MTLTextureDescriptor *GPULegacyTextureDescriptor::metal() const
{
    return m_metal.get();
}

} // namespace WebCore

#endif
