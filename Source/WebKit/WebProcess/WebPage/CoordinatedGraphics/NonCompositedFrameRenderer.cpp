/*
 * Copyright (C) 2025 Igalia S.L.
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

#include "config.h"
#include "NonCompositedFrameRenderer.h"

#if USE(COORDINATED_GRAPHICS)
#include <WebCore/GraphicsContextSkia.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(NonCompositedFrameRenderer);

std::unique_ptr<NonCompositedFrameRenderer> NonCompositedFrameRenderer::create(WebPage& webPage)
{
    auto instance = makeUnique<NonCompositedFrameRenderer>(webPage);
    return instance->initialize() ? WTF::move(instance) : nullptr;
}

NonCompositedFrameRenderer::NonCompositedFrameRenderer(WebPage& webPage)
    : m_webPage(webPage)
    , m_surface(AcceleratedSurface::create(m_webPage, [this] {
        m_canRenderNextFrame = true;
        if (m_shouldRenderFollowupFrame) {
            m_shouldRenderFollowupFrame = false;
            display();
        }
    }, AcceleratedSurface::RenderingPurpose::NonComposited))
{
#if ENABLE(DAMAGE_TRACKING)
    resetFrameDamage();
#endif
}

bool NonCompositedFrameRenderer::initialize()
{
    if (m_surface->usesGL()) {
        static_assert(sizeof(GLNativeWindowType) <= sizeof(uint64_t), "GLNativeWindowType must not be longer than 64 bits.");
        m_context = GLContext::create(PlatformDisplay::sharedDisplay(), m_surface->window());
        if (!m_context || !m_context->makeContextCurrent())
            return false;
    }

    m_surface->didCreateCompositingRunLoop(RunLoop::mainSingleton());
    return true;
}

NonCompositedFrameRenderer::~NonCompositedFrameRenderer()
{
    m_surface->willDestroyGLContext();
    m_surface->willDestroyCompositingRunLoop();
}

void NonCompositedFrameRenderer::setNeedsDisplayInRect(const IntRect& rect)
{
#if ENABLE(DAMAGE_TRACKING)
    if (m_frameDamage)
        m_frameDamage->add(rect);
#else
    UNUSED_PARAM(rect);
#endif
}

#if ENABLE(DAMAGE_TRACKING)
void NonCompositedFrameRenderer::resetFrameDamage()
{
    Ref webPage = m_webPage.get();
    if (webPage->corePage()->settings().propagateDamagingInformation())
        m_frameDamage = std::make_optional<Damage>(webPage->bounds(), webPage->corePage()->settings().unifyDamagedRegions() ? Damage::Mode::BoundingBox : Damage::Mode::Rectangles);
}
#endif

void NonCompositedFrameRenderer::display()
{
    if (!m_canRenderNextFrame) {
        m_shouldRenderFollowupFrame = true;
        return;
    }

    Ref webPage = m_webPage.get();
    webPage->updateRendering();
    webPage->finalizeRenderingUpdate({ });
    webPage->flushPendingEditorStateUpdate();

    IntSize scaledSize = webPage->size();
    scaledSize.scale(webPage->deviceScaleFactor());

    if (m_context)
        m_context->makeContextCurrent();
    m_surface->willRenderFrame(scaledSize);

    auto* canvas = m_surface->canvas();
    if (!canvas)
        return;

    if (m_context)
        PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent();

    canvas->save();
    GraphicsContextSkia graphicsContext(*canvas, m_surface->usesGL() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated, RenderingPurpose::Unspecified);
    graphicsContext.applyDeviceScaleFactor(webPage->deviceScaleFactor());

#if ENABLE(DAMAGE_TRACKING)
    if (m_frameDamage) {
        {
            Locker locker { m_frameDamageHistoryForTestingLock };
            if (m_frameDamageHistoryForTesting)
                m_frameDamageHistoryForTesting->append(m_frameDamage->regionForTesting());
        }
        m_surface->setFrameDamage(WTF::move(*m_frameDamage));
        resetFrameDamage();
    }
#endif

    auto rectToRepaint = webPage->bounds();
#if ENABLE(DAMAGE_TRACKING)
    if (webPage->corePage()->settings().useDamagingInformationForCompositing()) {
        if (auto& renderTargetDamage = m_surface->renderTargetDamage())
            rectToRepaint = renderTargetDamage->bounds();
    }
#endif
    webPage->drawRect(graphicsContext, rectToRepaint);
    canvas->restore();

    if (m_context)
        m_context->makeContextCurrent();

    m_canRenderNextFrame = false;
    m_surface->didRenderFrame();

    webPage->didUpdateRendering();
}

#if ENABLE(DAMAGE_TRACKING)
void NonCompositedFrameRenderer::resetDamageHistoryForTesting()
{
    Locker locker { m_frameDamageHistoryForTestingLock };
    m_frameDamageHistoryForTesting = std::make_optional<Vector<WebCore::Region>>();
}

void NonCompositedFrameRenderer::foreachRegionInDamageHistoryForTesting(Function<void(const Region&)>&& callback)
{
    Locker locker { m_frameDamageHistoryForTestingLock };
    if (m_frameDamageHistoryForTesting) {
        for (const auto& region : *m_frameDamageHistoryForTesting)
            callback(region);
    }
}
#endif

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
