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

#if ENABLE(WEBMETAL)

#include <wtf/RetainPtr.h>

OBJC_CLASS MTLTextureDescriptor;

namespace WebCore {

class GPUTextureDescriptor {
public:
    GPUTextureDescriptor(unsigned pixelFormat, unsigned width, unsigned height, bool mipmapped);
    ~GPUTextureDescriptor();

    unsigned width() const;
    void setWidth(unsigned) const;

    unsigned height() const;
    void setHeight(unsigned) const;

    unsigned sampleCount() const;
    void setSampleCount(unsigned) const;

    unsigned textureType() const;
    void setTextureType(unsigned) const;

    unsigned storageMode() const;
    void setStorageMode(unsigned) const;

    unsigned usage() const;
    void setUsage(unsigned) const;

#if USE(METAL)
    MTLTextureDescriptor *metal() const;
#endif

#if USE(METAL)
private:
    RetainPtr<MTLTextureDescriptor> m_metal;
#endif
};
    
} // namespace WebCore
#endif
