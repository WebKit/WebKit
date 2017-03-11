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

#include "WebGPUEnums.h"
#include "WebGPUObject.h"

#include <wtf/Vector.h>

namespace WebCore {

class GPUTextureDescriptor;

class WebGPUTextureDescriptor : public WebGPUObject {
public:
    virtual ~WebGPUTextureDescriptor();
    static Ref<WebGPUTextureDescriptor> create(unsigned long pixelFormat, unsigned long width, unsigned long height, bool mipmapped);

    unsigned long width() const;
    void setWidth(unsigned long);

    unsigned long height() const;
    void setHeight(unsigned long);

    unsigned long sampleCount() const;
    void setSampleCount(unsigned long);

    unsigned long textureType() const;
    void setTextureType(unsigned long);

    unsigned long storageMode() const;
    void setStorageMode(unsigned long);

    unsigned long usage() const;
    void setUsage(unsigned long);

    GPUTextureDescriptor* textureDescriptor() { return m_textureDescriptor.get(); }

private:
    WebGPUTextureDescriptor(unsigned long pixelFormat, unsigned long width, unsigned long height, bool mipmapped);

    RefPtr<GPUTextureDescriptor> m_textureDescriptor;
};

} // namespace WebCore

#endif
