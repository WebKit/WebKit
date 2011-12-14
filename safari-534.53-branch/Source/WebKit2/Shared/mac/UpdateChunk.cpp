/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "UpdateChunk.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "Attachment.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/FloatRect.h>
#include <mach/vm_map.h>
#include <wtf/RetainPtr.h>

using namespace WebCore;

namespace WebKit {

UpdateChunk::UpdateChunk()
    : m_data(0)
    , m_size(0)
{
}

UpdateChunk::UpdateChunk(const IntRect& rect)
    : m_rect(rect)
    , m_size(size())
{
    vm_allocate(mach_task_self(), reinterpret_cast<vm_address_t*>(&m_data), m_size, VM_FLAGS_ANYWHERE);
}

UpdateChunk::~UpdateChunk()
{
    if (m_data)
        vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(m_data), m_size);
}

RetainPtr<CGImageRef> UpdateChunk::createImage()
{
    RetainPtr<CGDataProviderRef> provider(AdoptCF, CGDataProviderCreateWithData(0, m_data, size(), 0));
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGImageRef> image(AdoptCF, CGImageCreate(m_rect.width(), m_rect.height(), 8, 32, m_rect.width() * 4, colorSpace.get(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host, provider.get(), 0, false, kCGRenderingIntentDefault));
    
    return image;
}

void UpdateChunk::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(m_rect);
    encoder->encode(CoreIPC::Attachment(m_data, size(), MACH_MSG_VIRTUAL_COPY, true));
    
    m_data = 0;
}

bool UpdateChunk::decode(CoreIPC::ArgumentDecoder* decoder, UpdateChunk& chunk)
{
    IntRect rect;
    if (!decoder->decode(rect))
        return false;
    chunk.m_rect = rect;
    
    CoreIPC::Attachment attachment;
    if (!decoder->decode(attachment))
        return false;

    chunk.m_size = attachment.size();
    chunk.m_data = reinterpret_cast<uint8_t*>(attachment.address());
    return true;
}

} // namespace WebKit
