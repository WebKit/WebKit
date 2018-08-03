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

#pragma once

#if ENABLE(WEBGPU)

#include "GPURenderPassDescriptor.h"
#include "WebGPURenderPassColorAttachmentDescriptor.h"
#include "WebGPURenderPassDepthAttachmentDescriptor.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class WebGPURenderPassDescriptor : public RefCounted<WebGPURenderPassDescriptor> {
public:
    ~WebGPURenderPassDescriptor();
    static Ref<WebGPURenderPassDescriptor> create();

    WebGPURenderPassDepthAttachmentDescriptor& depthAttachment();
    const Vector<RefPtr<WebGPURenderPassColorAttachmentDescriptor>>& colorAttachments();

    const GPURenderPassDescriptor& descriptor() const { return m_descriptor; }

private:
    WebGPURenderPassDescriptor();

    Vector<RefPtr<WebGPURenderPassColorAttachmentDescriptor>> m_colorAttachments;
    RefPtr<WebGPURenderPassDepthAttachmentDescriptor> m_depthAttachment;

    GPURenderPassDescriptor m_descriptor;
};
    
} // namespace WebCore

#endif
