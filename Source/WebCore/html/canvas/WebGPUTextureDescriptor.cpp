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

#include "config.h"
#include "WebGPUTextureDescriptor.h"

#if ENABLE(WEBGPU)

#include "GPUTextureDescriptor.h"
#include "WebGPURenderingContext.h"

namespace WebCore {

Ref<WebGPUTextureDescriptor> WebGPUTextureDescriptor::create(unsigned long pixelFormat, unsigned long width, unsigned long height, bool mipmapped)
{
    return adoptRef(*new WebGPUTextureDescriptor(pixelFormat, width, height, mipmapped));
}

WebGPUTextureDescriptor::WebGPUTextureDescriptor(unsigned long pixelFormat, unsigned long width, unsigned long height, bool mipmapped)
    : WebGPUObject()
{
    m_textureDescriptor = GPUTextureDescriptor::create(pixelFormat, width, height, mipmapped);
}

WebGPUTextureDescriptor::~WebGPUTextureDescriptor()
{
}

unsigned long WebGPUTextureDescriptor::width() const
{
    if (!m_textureDescriptor)
        return 0;

    return m_textureDescriptor->width();
}

void WebGPUTextureDescriptor::setWidth(unsigned long width)
{
    if (!m_textureDescriptor)
        return;

    m_textureDescriptor->setWidth(width);
}

unsigned long WebGPUTextureDescriptor::height() const
{
    if (!m_textureDescriptor)
        return 0;

    return m_textureDescriptor->height();
}

void WebGPUTextureDescriptor::setHeight(unsigned long height)
{
    if (!m_textureDescriptor)
        return;
    
    m_textureDescriptor->setHeight(height);
}

unsigned long WebGPUTextureDescriptor::sampleCount() const
{
    if (!m_textureDescriptor)
        return 0;

    return m_textureDescriptor->sampleCount();
}

void WebGPUTextureDescriptor::setSampleCount(unsigned long sampleCount)
{
    if (!m_textureDescriptor)
        return;
    
    m_textureDescriptor->setSampleCount(sampleCount);
}

unsigned long WebGPUTextureDescriptor::textureType() const
{
    if (!m_textureDescriptor)
        return 0;

    return m_textureDescriptor->textureType();
}

void WebGPUTextureDescriptor::setTextureType(unsigned long textureType)
{
    if (!m_textureDescriptor)
        return;
    
    m_textureDescriptor->setTextureType(textureType);
}

unsigned long WebGPUTextureDescriptor::storageMode() const
{
    if (!m_textureDescriptor)
        return 0;

    return m_textureDescriptor->storageMode();
}

void WebGPUTextureDescriptor::setStorageMode(unsigned long storageMode)
{
    if (!m_textureDescriptor)
        return;
    
    m_textureDescriptor->setStorageMode(storageMode);
}

unsigned long WebGPUTextureDescriptor::usage() const
{
    if (!m_textureDescriptor)
        return 0;

    return m_textureDescriptor->usage();
}

void WebGPUTextureDescriptor::setUsage(unsigned long usage)
{
    if (!m_textureDescriptor)
        return;

    m_textureDescriptor->setUsage(usage);
}
    
} // namespace WebCore

#endif
