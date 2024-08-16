/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)

#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageBufferCGBackend.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class BifurcatedGraphicsContext;
class DynamicContentScalingDisplayList;
}

namespace WebKit {

class DynamicContentScalingImageBufferBackend;

// Ideally this would be a generic "BifurcatedImageBuffer", but it is
// currently insufficiently general (e.g. needs to support bifurcating
// the context flush, etc.).

class DynamicContentScalingBifurcatedImageBuffer : public WebCore::ImageBuffer {
public:
    DynamicContentScalingBifurcatedImageBuffer(WebCore::ImageBufferParameters, const WebCore::ImageBufferBackend::Info&, const WebCore::ImageBufferCreationContext&, std::unique_ptr<WebCore::ImageBufferBackend>&& = nullptr, WebCore::RenderingResourceIdentifier = WebCore::RenderingResourceIdentifier::generate());

    WebCore::GraphicsContext& context() const final;

protected:
    std::optional<WebCore::DynamicContentScalingDisplayList> dynamicContentScalingDisplayList() final;

    void releaseGraphicsContext() final;

    mutable std::unique_ptr<WebCore::BifurcatedGraphicsContext> m_context;
    std::unique_ptr<DynamicContentScalingImageBufferBackend> m_dynamicContentScalingBackend;
};

}

#endif // ENABLE(RE_DYNAMIC_CONTENT_SCALING)
