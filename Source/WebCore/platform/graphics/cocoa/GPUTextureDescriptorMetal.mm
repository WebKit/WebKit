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
#import "GPUTextureDescriptor.h"

#if ENABLE(WEBGPU)

#import "Logging.h"

#import <Metal/Metal.h>

namespace WebCore {

GPUTextureDescriptor::GPUTextureDescriptor(unsigned long pixelFormat, unsigned long width, unsigned long height, bool mipmapped)
{
    LOG(WebGPU, "GPUTextureDescriptor::GPUTextureDescriptor()");

    m_textureDescriptor = (MTLTextureDescriptor *)[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:static_cast<MTLPixelFormat>(pixelFormat) width:width height:height mipmapped:mipmapped];
}

unsigned long GPUTextureDescriptor::width() const
{
    if (!m_textureDescriptor)
        return 0;
    return [m_textureDescriptor width];
}

void GPUTextureDescriptor::setWidth(unsigned long newWidth)
{
    ASSERT(m_textureDescriptor);
    [m_textureDescriptor setWidth:newWidth];
}

unsigned long GPUTextureDescriptor::height() const
{
    if (!m_textureDescriptor)
        return 0;
    return [m_textureDescriptor height];
}

void GPUTextureDescriptor::setHeight(unsigned long newHeight)
{
    ASSERT(m_textureDescriptor);
    [m_textureDescriptor setHeight:newHeight];
}

unsigned long GPUTextureDescriptor::sampleCount() const
{
    if (!m_textureDescriptor)
        return 0;
    return [m_textureDescriptor sampleCount];
}

void GPUTextureDescriptor::setSampleCount(unsigned long newSampleCount)
{
    ASSERT(m_textureDescriptor);
    [m_textureDescriptor setSampleCount:newSampleCount];
}

unsigned long GPUTextureDescriptor::textureType() const
{
    if (!m_textureDescriptor)
        return 0;
    return [m_textureDescriptor textureType];
}

void GPUTextureDescriptor::setTextureType(unsigned long newTextureType)
{
    ASSERT(m_textureDescriptor);
    [m_textureDescriptor setTextureType:static_cast<MTLTextureType>(newTextureType)];
}

unsigned long GPUTextureDescriptor::storageMode() const
{
    if (!m_textureDescriptor)
        return 0;
    return [m_textureDescriptor storageMode];
}

void GPUTextureDescriptor::setStorageMode(unsigned long newStorageMode)
{
    ASSERT(m_textureDescriptor);
    [m_textureDescriptor setStorageMode:static_cast<MTLStorageMode>(newStorageMode)];
}

unsigned long GPUTextureDescriptor::usage() const
{
    if (!m_textureDescriptor)
        return 0;
    return [m_textureDescriptor usage];
}

void GPUTextureDescriptor::setUsage(unsigned long newUsage)
{
    ASSERT(m_textureDescriptor);
    [m_textureDescriptor setUsage:newUsage];
}

MTLTextureDescriptor *GPUTextureDescriptor::platformTextureDescriptor()
{
    return m_textureDescriptor.get();
}

} // namespace WebCore

#endif
