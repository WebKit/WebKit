/*
 * Copyright (C) 2025, 2026 Igalia S.L.
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

#if USE(COORDINATED_GRAPHICS)
#include "AcceleratedSurface.h"
#include "FrameRenderer.h"

namespace WebCore {
class GLContext;
}

namespace WebKit {
class WebPage;

class NonCompositedFrameRenderer final : public FrameRenderer {
    WTF_MAKE_TZONE_ALLOCATED(NonCompositedFrameRenderer);
public:
    static std::unique_ptr<NonCompositedFrameRenderer> create(WebPage&);

    explicit NonCompositedFrameRenderer(WebPage&);
    ~NonCompositedFrameRenderer();

private:
    bool initialize();

#if ENABLE(DAMAGE_TRACKING)
    void resetFrameDamage();
#endif

    void addDirtyRect(const WebCore::IntRect&);
    void finishRenderingUpdate();
    void frameComplete();

    // FrameRenderer
    uint64_t surfaceID() const override { return m_surface->surfaceID(); }
    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    void updateRenderingWithForcedRepaint() override;
    void scheduleRenderingUpdate() override;
    bool canUpdateRendering() const override;
    void updateRendering() override;
    void sizeDidChange() override;
    bool ensureDrawing() override;
    void fillGLInformation(RenderProcessInfo&&, CompletionHandler<void(RenderProcessInfo&&)>&&) override;
#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM) && (USE(GBM) || OS(ANDROID))
    void preferredBufferFormatsDidChange() override;
#endif
#if ENABLE(DAMAGE_TRACKING)
    void resetDamageHistoryForTesting() override;
    void foreachRegionInDamageHistoryForTesting(Function<void(const WebCore::Region&)>&&) const override;
#endif
#if PLATFORM(GTK)
    void adjustTransientZoom(double, WebCore::FloatPoint, WebCore::FloatPoint) override;
    void commitTransientZoom(double, WebCore::FloatPoint, WebCore::FloatPoint) override;
#endif

    const WeakRef<WebPage> m_webPage;
    Ref<AcceleratedSurface> m_surface;
    std::unique_ptr<WebCore::GLContext> m_context;
    bool m_pendingNotifyFrame { false };
    bool m_isWaitingForFrameComplete { false };
    bool m_shouldRenderFollowupFrame { false };
#if ENABLE(DAMAGE_TRACKING)
    std::optional<WebCore::Damage> m_frameDamage;
    std::optional<Vector<WebCore::Region>> m_frameDamageHistoryForTesting;
#endif
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
