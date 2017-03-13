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

#include <runtime/ArrayBufferView.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

#if PLATFORM(COCOA)
OBJC_CLASS MTLBuffer;
#endif

namespace WebCore {

class GPUDevice;

class GPUBuffer : public RefCounted<GPUBuffer> {
public:
    static RefPtr<GPUBuffer> create(GPUDevice*, ArrayBufferView*);
    WEBCORE_EXPORT ~GPUBuffer();

    WEBCORE_EXPORT unsigned long length() const;
    WEBCORE_EXPORT RefPtr<ArrayBuffer> contents();

#if PLATFORM(COCOA)
    WEBCORE_EXPORT MTLBuffer *platformBuffer();
#endif

private:
    GPUBuffer(GPUDevice*, ArrayBufferView*);
    
#if PLATFORM(COCOA)
    RetainPtr<MTLBuffer> m_buffer;
#endif
    RefPtr<ArrayBuffer> m_contents;
};
    
} // namespace WebCore
#endif
