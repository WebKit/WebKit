/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUPtr.h"
#include "WebGPUXRSubImage.h"

#include <WebGPU/WebGPU.h>

namespace WebCore::WebGPU {

class ConvertToBackingContext;

class XRSubImageImpl final : public XRSubImage {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<XRSubImageImpl> create(WebGPUPtr<WGPUXRSubImage>&& backing, ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new XRSubImageImpl(WTFMove(backing), convertToBackingContext));
    }

    virtual ~XRSubImageImpl();

private:
    friend class DowncastConvertToBackingContext;

    explicit XRSubImageImpl(WebGPUPtr<WGPUXRSubImage>&&, ConvertToBackingContext&);

    XRSubImageImpl(const XRSubImageImpl&) = delete;
    XRSubImageImpl(XRSubImageImpl&&) = delete;
    XRSubImageImpl& operator=(const XRSubImageImpl&) = delete;
    XRSubImageImpl& operator=(XRSubImageImpl&&) = delete;

    WGPUXRSubImage backing() const { return m_backing.get(); }

    WebGPUPtr<WGPUXRSubImage> m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
};

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
