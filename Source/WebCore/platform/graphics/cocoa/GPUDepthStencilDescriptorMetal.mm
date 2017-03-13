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
#import "GPUDepthStencilDescriptor.h"

#if ENABLE(WEBGPU)

#import "GPUEnums.h"
#import "Logging.h"

#import <Metal/Metal.h>

namespace WebCore {

GPUDepthStencilDescriptor::GPUDepthStencilDescriptor()
{
    LOG(WebGPU, "GPUDepthStencilDescriptor::GPUDepthStencilDescriptor()");

    m_depthStencilDescriptor = adoptNS((MTLDepthStencilDescriptor *)[MTLDepthStencilDescriptor new]);
}

bool GPUDepthStencilDescriptor::depthWriteEnabled() const
{
    if (!m_depthStencilDescriptor)
        return false;

    return [m_depthStencilDescriptor isDepthWriteEnabled];
}

void GPUDepthStencilDescriptor::setDepthWriteEnabled(bool newDepthWriteEnabled)
{
    ASSERT(m_depthStencilDescriptor);
    [m_depthStencilDescriptor setDepthWriteEnabled:newDepthWriteEnabled];
}

GPUCompareFunction GPUDepthStencilDescriptor::depthCompareFunction() const
{
    if (!m_depthStencilDescriptor)
        return GPUCompareFunction::Never;

    return static_cast<GPUCompareFunction>([m_depthStencilDescriptor depthCompareFunction]);
}

void GPUDepthStencilDescriptor::setDepthCompareFunction(GPUCompareFunction newFunction)
{
    ASSERT(m_depthStencilDescriptor);
    [m_depthStencilDescriptor setDepthCompareFunction:static_cast<MTLCompareFunction>(newFunction)];
}

MTLDepthStencilDescriptor *GPUDepthStencilDescriptor::platformDepthStencilDescriptor()
{
    return m_depthStencilDescriptor.get();
}

} // namespace WebCore

#endif
