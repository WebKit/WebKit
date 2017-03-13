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
#import "GPUDrawable.h"

#if ENABLE(WEBGPU)

#import "GPUDevice.h"
#import "Logging.h"

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

namespace WebCore {

GPUDrawable::GPUDrawable(GPUDevice* device)
{
    LOG(WebGPU, "GPUDrawable::GPUDrawable()");

    if (!device || !device->platformDevice())
        return;

    m_drawable = (MTLDrawable *)[static_cast<CAMetalLayer*>(device->platformLayer()) nextDrawable];
}

void GPUDrawable::release()
{
    LOG(WebGPU, "GPUDrawable::release()");
    m_drawable = nullptr;
}
    
MTLDrawable *GPUDrawable::platformDrawable()
{
    return m_drawable.get();
}

MTLTexture *GPUDrawable::platformTexture()
{
    if (!m_drawable)
        return nullptr;
    return (MTLTexture *)[(id<CAMetalDrawable>)m_drawable.get() texture];
}

} // namespace WebCore

#endif
