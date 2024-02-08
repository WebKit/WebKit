/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "WebGPUExternalTexture.h"
#include "WebGPUExternalTextureDescriptor.h"
#include "WebGPUPredefinedColorSpace.h"
#include "WebGPUPtr.h"
#include <WebGPU/WebGPU.h>
#include <WebGPU/WebGPUExt.h>

namespace WebCore::WebGPU {

class ConvertToBackingContext;
struct ExternalTextureDescriptor;

class ExternalTextureImpl final : public ExternalTexture {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ExternalTextureImpl> create(WebGPUPtr<WGPUExternalTexture>&& externalTexture, const ExternalTextureDescriptor& descriptor, ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new ExternalTextureImpl(WTFMove(externalTexture), descriptor, convertToBackingContext));
    }

    virtual ~ExternalTextureImpl();

    WGPUExternalTexture backing() const { return m_backing.get(); };

private:
    friend class DowncastConvertToBackingContext;

    ExternalTextureImpl(WebGPUPtr<WGPUExternalTexture>&&, const ExternalTextureDescriptor&, ConvertToBackingContext&);

    ExternalTextureImpl(const ExternalTextureImpl&) = delete;
    ExternalTextureImpl(ExternalTextureImpl&&) = delete;
    ExternalTextureImpl& operator=(const ExternalTextureImpl&) = delete;
    ExternalTextureImpl& operator=(ExternalTextureImpl&&) = delete;

    void setLabelInternal(const String&) final;
    void destroy() final;
    void undestroy() final;

    Ref<ConvertToBackingContext> m_convertToBackingContext;

    WebGPUPtr<WGPUExternalTexture> m_backing;
    PredefinedColorSpace m_colorSpace;
};

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
