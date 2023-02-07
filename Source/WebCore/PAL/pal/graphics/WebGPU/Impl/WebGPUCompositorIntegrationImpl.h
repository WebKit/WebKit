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

#include "WebGPUCompositorIntegration.h"

#include "WebGPUPresentationContextImpl.h"
#include <WebGPU/WebGPU.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#include <wtf/MachSendRight.h>
#include <wtf/RetainPtr.h>
#include <wtf/spi/cocoa/IOSurfaceSPI.h>
#endif

namespace PAL::WebGPU {

class ConvertToBackingContext;

class CompositorIntegrationImpl final : public CompositorIntegration {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<CompositorIntegrationImpl> create(ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new CompositorIntegrationImpl(convertToBackingContext));
    }

    virtual ~CompositorIntegrationImpl();

    void setPresentationContext(PresentationContextImpl& presentationContext)
    {
        ASSERT(!m_presentationContext);
        m_presentationContext = &presentationContext;
    }

    void registerCallbacks(WTF::Function<void(CFArrayRef)>&& renderBuffersWereRecreatedCallback, WTF::Function<void(CompletionHandler<void()>&&)>&& onSubmittedWorkScheduledCallback)
    {
        ASSERT(!m_renderBuffersWereRecreatedCallback);
        m_renderBuffersWereRecreatedCallback = WTFMove(renderBuffersWereRecreatedCallback);
        ASSERT(!m_onSubmittedWorkScheduledCallback);
        m_onSubmittedWorkScheduledCallback = WTFMove(onSubmittedWorkScheduledCallback);
    }

private:
    friend class DowncastConvertToBackingContext;

    explicit CompositorIntegrationImpl(ConvertToBackingContext&);

    CompositorIntegrationImpl(const CompositorIntegrationImpl&) = delete;
    CompositorIntegrationImpl(CompositorIntegrationImpl&&) = delete;
    CompositorIntegrationImpl& operator=(const CompositorIntegrationImpl&) = delete;
    CompositorIntegrationImpl& operator=(CompositorIntegrationImpl&&) = delete;

    void prepareForDisplay(CompletionHandler<void()>&&) override;

#if PLATFORM(COCOA)
    Vector<MachSendRight> recreateRenderBuffers(int width, int height) override;

    Vector<RetainPtr<IOSurfaceRef>> m_renderBuffers;
    WTF::Function<void(CFArrayRef)> m_renderBuffersWereRecreatedCallback;
#endif

    WTF::Function<void(CompletionHandler<void()>&&)> m_onSubmittedWorkScheduledCallback;

    RefPtr<PresentationContextImpl> m_presentationContext;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
};

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
