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

#include "config.h"
#include "NonCompositedFrameRenderer.h"

#if USE(COORDINATED_GRAPHICS)
#include "DrawingArea.h"
#include "RenderProcessInfo.h"
#include "WebPage.h"
#include "WebPageInlines.h"
#include <WebCore/GLContext.h>
#include <WebCore/GraphicsContextSkia.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/Settings.h>
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>

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
        frameComplete();
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
    if (m_forcedRepaintAsyncCallback)
        m_forcedRepaintAsyncCallback();
    if (m_context)
        m_context->makeContextCurrent();
    m_surface->willDestroyGLContext();
    m_context = nullptr;
    m_surface->willDestroyCompositingRunLoop();
}

void NonCompositedFrameRenderer::addDirtyRect(const IntRect& rect)
{
#if ENABLE(DAMAGE_TRACKING)
    if (m_frameDamage) {
        auto scaledRect = rect;
        scaledRect.scale(m_webPage->deviceScaleFactor());
        m_frameDamage->add(scaledRect);
    }
#else
    UNUSED_PARAM(rect);
#endif
}

void NonCompositedFrameRenderer::setNeedsDisplay()
{
    auto dirtyRect = m_webPage->bounds();
    if (dirtyRect.isEmpty())
        return;

    addDirtyRect(dirtyRect);
    scheduleRenderingUpdate();
}

void NonCompositedFrameRenderer::setNeedsDisplayInRect(const IntRect& rect)
{
    auto dirtyRect = rect;
    dirtyRect.intersect(m_webPage->bounds());
    if (dirtyRect.isEmpty())
        return;

    addDirtyRect(dirtyRect);
    scheduleRenderingUpdate();
}

#if ENABLE(DAMAGE_TRACKING)
void NonCompositedFrameRenderer::resetFrameDamage()
{
    auto scaledRect = m_webPage->bounds();
    scaledRect.scale(m_webPage->deviceScaleFactor());
    if (!m_context) {
        // For CPU rendering use the damage unconditionally to reduce the amount of pixels to upload to the GPU for the UI process.
        m_frameDamage = std::make_optional<Damage>(scaledRect, Damage::Mode::Rectangles, 4);
        return;
    }

    if (!m_webPage->corePage()->settings().propagateDamagingInformation()) {
        m_frameDamage = std::nullopt;
        return;
    }

    m_frameDamage = std::make_optional<Damage>(scaledRect, m_webPage->corePage()->settings().unifyDamagedRegions() ? Damage::Mode::BoundingBox : Damage::Mode::Rectangles, 4);
}
#endif

void NonCompositedFrameRenderer::sizeDidChange()
{
#if ENABLE(DAMAGE_TRACKING)
    resetFrameDamage();
    addDirtyRect(m_webPage->bounds());
#endif
    if (canUpdateRendering())
        updateRendering();
    else
        scheduleRenderingUpdate();
}

void NonCompositedFrameRenderer::scheduleRenderingUpdate()
{
    WTFEmitSignpost(this, NonCompositedScheduleRenderingUpdate, "canRenderNextFrame %s", canUpdateRendering() ? "yes" : "no");

    if (m_layerTreeStateIsFrozen || m_isSuspended || m_webPage->size().isEmpty())
        return;

    if (!canUpdateRendering()) {
        m_shouldRenderFollowupFrame = true;
        return;
    }

    scheduleRenderingUpdateRunLoopObserver();
}

bool NonCompositedFrameRenderer::canUpdateRendering() const
{
    return !m_pendingNotifyFrame;
}

void NonCompositedFrameRenderer::updateRendering()
{
    invalidateRenderingUpdateRunLoopObserver();

    if (m_layerTreeStateIsFrozen || m_isSuspended)
        return;

    SetForScope<bool> reentrancyProtector(m_isUpdatingRendering, true);

    WTFBeginSignpost(this, NonCompositedRenderingUpdate);

    Ref webPage = m_webPage;
    webPage->updateRendering();
    webPage->finalizeRenderingUpdate({ });
    webPage->flushPendingEditorStateUpdate();

    IntSize scaledSize = webPage->size();
    scaledSize.scale(webPage->deviceScaleFactor());

    RefPtr drawingArea = webPage->drawingArea();
    if (drawingArea)
        drawingArea->willStartRenderingUpdateDisplay();

    if (m_context)
        m_context->makeContextCurrent();
    m_surface->willRenderFrame(scaledSize);

    if (auto* canvas = m_surface->canvas()) {
        if (m_context)
            PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent();

        canvas->save();
        GraphicsContextSkia graphicsContext(*canvas, m_context ? RenderingMode::Accelerated : RenderingMode::Unaccelerated, RenderingPurpose::DOM);
        graphicsContext.applyDeviceScaleFactor(webPage->deviceScaleFactor());

        if (m_surface->shouldPaintMirrored()) {
            SkMatrix matrix;
            matrix.setScaleTranslate(1, -1, 0, webPage->size().height());
            canvas->concat(matrix);
        }

#if ENABLE(DAMAGE_TRACKING)
        if (m_frameDamage) {
            if (m_frameDamageHistoryForTesting)
                m_frameDamageHistoryForTesting->append(m_frameDamage->regionForTesting());
            m_surface->setFrameDamage(WTF::move(*m_frameDamage));
            resetFrameDamage();
        }
#endif

        auto drawRect = [&](const IntRect& rect) {
            WTFBeginSignpost(canvas, DrawRect, "Skia/%s, dirty region %ix%i+%i+%i", m_context ? "GPU" : "CPU", rect.x(), rect.y(), rect.width(), rect.height());
            webPage->drawRect(graphicsContext, rect);
            WTFEndSignpost(canvas, DrawRect);
        };

#if ENABLE(DAMAGE_TRACKING)
        if (auto& renderTargetDamage = m_surface->renderTargetDamage()) {
            for (const auto& rect : *renderTargetDamage) {
                auto scaledRect = rect;
                scaledRect.scale(1 / webPage->deviceScaleFactor());
                drawRect(scaledRect);
            }
        } else
            drawRect(webPage->bounds());
#else
        drawRect(webPage->bounds());
#endif

        canvas->restore();

        if (m_context) {
            if (auto* surface = canvas->getSurface())
                PlatformDisplay::sharedDisplay().skiaGrContext()->flushAndSubmit(surface, GrSyncCpu::kNo);

            m_context->makeContextCurrent();
        }
    }

    m_surface->didRenderFrame();
    webPage->didUpdateRendering();

    if (drawingArea) {
        drawingArea->didCompleteRenderingUpdateDisplay();
        drawingArea->dispatchPendingCallbacksAfterEnsuringDrawing();
    }

    WTFEndSignpost(this, NonCompositedRenderingUpdate);

    if (m_isWaitingForFrameComplete)
        m_pendingNotifyFrame = true;
    else
        finishRenderingUpdate();
}

void NonCompositedFrameRenderer::finishRenderingUpdate()
{
    m_isWaitingForFrameComplete = true;
    m_surface->sendFrame();

    if (m_forcedRepaintAsyncCallback)
        m_forcedRepaintAsyncCallback();
}

void NonCompositedFrameRenderer::frameComplete()
{
    WTFEmitSignpost(this, FrameComplete);

    m_isWaitingForFrameComplete = false;

    bool pendingNotifyFrame = std::exchange(m_pendingNotifyFrame, false);
    if (pendingNotifyFrame)
        finishRenderingUpdate();

    bool shouldRenderFollowupFrame = std::exchange(m_shouldRenderFollowupFrame, false);
    if (!m_isSuspended && !m_layerTreeStateIsFrozen && shouldRenderFollowupFrame)
        scheduleRenderingUpdateRunLoopObserver();
}

void NonCompositedFrameRenderer::updateRenderingWithForcedRepaint()
{
    if (!canUpdateRendering() || m_isUpdatingRendering)
        return;

    protect(m_webPage)->corePage()->forceRepaintAllFrames();
    addDirtyRect(m_webPage->bounds());
    updateRendering();
}

bool NonCompositedFrameRenderer::ensureDrawing()
{
    if (m_layerTreeStateIsFrozen || m_isSuspended || m_webPage->size().isEmpty())
        return false;

    setNeedsDisplay();
    return true;
}

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM) && (USE(GBM) || OS(ANDROID))
void NonCompositedFrameRenderer::preferredBufferFormatsDidChange()
{
    ASSERT(RunLoop::isMain());
    m_surface->preferredBufferFormatsDidChange();
}
#endif

#if ENABLE(DAMAGE_TRACKING)
void NonCompositedFrameRenderer::resetDamageHistoryForTesting()
{
    m_frameDamageHistoryForTesting = std::make_optional<Vector<WebCore::Region>>();
}

void NonCompositedFrameRenderer::foreachRegionInDamageHistoryForTesting(Function<void(const Region&)>&& callback) const
{
    if (m_frameDamageHistoryForTesting) {
        for (const auto& region : *m_frameDamageHistoryForTesting)
            callback(region);
    }
}
#endif

#if PLATFORM(GTK)
void NonCompositedFrameRenderer::adjustTransientZoom(double scale, FloatPoint, FloatPoint unscrolledOrigin)
{
    Ref webPage = m_webPage;
    webPage->scalePage(scale / webPage->viewScaleFactor(), roundedIntPoint(-unscrolledOrigin));
}

void NonCompositedFrameRenderer::commitTransientZoom(double scale, FloatPoint, FloatPoint unscrolledOrigin)
{
    Ref webPage = m_webPage;
    webPage->scalePage(scale / webPage->viewScaleFactor(), roundedIntPoint(-unscrolledOrigin));
}
#endif

void NonCompositedFrameRenderer::fillGLInformation(RenderProcessInfo&& info, CompletionHandler<void(RenderProcessInfo&&)>&& completionHandler)
{
    if (!m_context) {
        completionHandler(WTF::move(info));
        return;
    }

    {
        GLContext::ScopedGLContextCurrent currentContext(*m_context);
        info.glRenderer = String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        info.glVendor = String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
        info.glVersion = String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_VERSION)));
        info.glShadingVersion = String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
        info.glExtensions = String::fromUTF8(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));

        auto eglDisplay = eglGetCurrentDisplay();
        info.eglVersion = String::fromUTF8(eglQueryString(eglDisplay, EGL_VERSION));
        info.eglVendor = String::fromUTF8(eglQueryString(eglDisplay, EGL_VENDOR));
        info.eglExtensions = makeString(unsafeSpan(eglQueryString(nullptr, EGL_EXTENSIONS)), ' ', unsafeSpan(eglQueryString(eglDisplay, EGL_EXTENSIONS)));
    }

    completionHandler(WTF::move(info));
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
