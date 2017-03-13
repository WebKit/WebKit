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

#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

#if PLATFORM(COCOA)
OBJC_CLASS MTLTextureDescriptor;
#endif

namespace WebCore {

class GPUTextureDescriptor : public RefCounted<GPUTextureDescriptor> {
public:
    static RefPtr<GPUTextureDescriptor> create(unsigned long pixelFormat, unsigned long width, unsigned long height, bool mipmapped);
    WEBCORE_EXPORT ~GPUTextureDescriptor();

    WEBCORE_EXPORT unsigned long width() const;
    WEBCORE_EXPORT void setWidth(unsigned long);

    WEBCORE_EXPORT unsigned long height() const;
    WEBCORE_EXPORT void setHeight(unsigned long);

    WEBCORE_EXPORT unsigned long sampleCount() const;
    WEBCORE_EXPORT void setSampleCount(unsigned long);

    WEBCORE_EXPORT unsigned long textureType() const;
    WEBCORE_EXPORT void setTextureType(unsigned long);

    WEBCORE_EXPORT unsigned long storageMode() const;
    WEBCORE_EXPORT void setStorageMode(unsigned long);

    WEBCORE_EXPORT unsigned long usage() const;
    WEBCORE_EXPORT void setUsage(unsigned long);

#if PLATFORM(COCOA)
    WEBCORE_EXPORT MTLTextureDescriptor *platformTextureDescriptor();
#endif

private:
    GPUTextureDescriptor(unsigned long pixelFormat, unsigned long width, unsigned long height, bool mipmapped);

#if PLATFORM(COCOA)
    RetainPtr<MTLTextureDescriptor> m_textureDescriptor;
#endif
};
    
} // namespace WebCore
#endif
