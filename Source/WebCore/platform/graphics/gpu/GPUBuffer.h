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

#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

OBJC_PROTOCOL(MTLBuffer);

namespace JSC {
class ArrayBuffer;
class ArrayBufferView;
}

namespace WebCore {

class GPUDevice;

class GPUBuffer {
public:
    WEBCORE_EXPORT GPUBuffer(const GPUDevice&, const JSC::ArrayBufferView&);
    WEBCORE_EXPORT ~GPUBuffer();

    WEBCORE_EXPORT unsigned length() const;
    JSC::ArrayBuffer* contents() const { return m_contents.get(); }

#if USE(METAL)
    MTLBuffer *metal() const { return m_metal.get(); }
#endif

private:
#if USE(METAL)
    RetainPtr<MTLBuffer> m_metal;
#endif
    RefPtr<JSC::ArrayBuffer> m_contents;
};
    
} // namespace WebCore

#endif
