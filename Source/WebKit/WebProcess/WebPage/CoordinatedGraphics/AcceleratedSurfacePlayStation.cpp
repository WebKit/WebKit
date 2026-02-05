/*
 * Copyright (C) 2023 Igalia S.L.
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

#include "config.h"
#include "AcceleratedSurfacePlayStation.h"

#if USE(COORDINATED_GRAPHICS)
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/GLContext.h>
#include <WebCore/GLFence.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/Region.h>
#include <WebCore/Settings.h>
#include <WebCore/ShareableBitmap.h>
#include <array>
#include <fcntl.h>
#include <unistd.h>
#include <wtf/SafeStrerror.h>
#include <wtf/TZoneMallocInlines.h>

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES2/gl2.h>
#endif

#if USE(WPE_RENDERER)
#include <WebCore/PlatformDisplayLibWPE.h>
#include <wpe/wpe-egl.h>
#include <wtf/UniStdExtras.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(AcceleratedSurface);

static inline bool isColorOpaque(AcceleratedSurface::ColorComponents color)
{
    return color[3] == WebCore::AlphaTraits<float>::opaque;
}

static uint64_t generateID()
{
    static uint64_t identifier = 0;
    return ++identifier;
}

Ref<AcceleratedSurface> AcceleratedSurface::create(WebPage& webPage, Function<void()>&& frameCompleteHandler)
{
    return adoptRef(*new AcceleratedSurface(webPage, WTF::move(frameCompleteHandler)));
}

static bool useExplicitSync()
{
    auto& display = PlatformDisplay::sharedDisplay();
    auto& extensions = display.eglExtensions();
    return extensions.ANDROID_native_fence_sync && (display.eglCheckVersion(1, 5) || extensions.KHR_fence_sync);
}

AcceleratedSurface::AcceleratedSurface(WebPage& webPage, Function<void()>&& frameCompleteHandler)
    : m_webPage(webPage)
    , m_frameCompleteHandler(WTF::move(frameCompleteHandler))
    , m_id(generateID())
    , m_isVisible(webPage.activityState().contains(ActivityState::IsVisible))
    , m_useExplicitSync(useExplicitSync())
{
    auto color = webPage.backgroundColor();
    m_backgroundColor = color ? color->toResolvedColorComponentsInColorSpace(WebCore::ColorSpace::SRGB) : white;

#if USE(WPE_RENDERER)
    if (m_swapChain.type() == SwapChain::Type::WPEBackend)
        m_swapChain.initialize(webPage);
#endif
}

AcceleratedSurface::~AcceleratedSurface() = default;

WTF_MAKE_TZONE_ALLOCATED_IMPL(AcceleratedSurface::RenderTarget);

static uint64_t generateTargetID()
{
    static uint64_t identifier = 0;
    return ++identifier;
}

AcceleratedSurface::RenderTarget::RenderTarget()
    : m_id(generateTargetID())
{
}

AcceleratedSurface::RenderTarget::~RenderTarget() = default;

#if USE(WPE_RENDERER)
std::unique_ptr<AcceleratedSurface::RenderTarget> AcceleratedSurface::RenderTargetWPEBackend::create(const IntSize& initialSize, UnixFileDescriptor&& hostFD, const AcceleratedSurface& surface)
{
    return makeUnique<RenderTargetWPEBackend>(initialSize, WTF::move(hostFD), surface);
}

AcceleratedSurface::RenderTargetWPEBackend::RenderTargetWPEBackend(const IntSize& initialSize, UnixFileDescriptor&& hostFD, const AcceleratedSurface& surface)
    : RenderTarget()
    , m_backend(wpe_renderer_backend_egl_target_create(hostFD.release()))
{
    static struct wpe_renderer_backend_egl_target_client s_client = {
        // frame_complete
        [](void* data) {
            auto& surface = *reinterpret_cast<AcceleratedSurface*>(data);
            surface.frameDone();
        },
        // padding
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };
    wpe_renderer_backend_egl_target_set_client(m_backend, &s_client, const_cast<AcceleratedSurface*>(&surface));
    wpe_renderer_backend_egl_target_initialize(m_backend, downcast<PlatformDisplayLibWPE>(PlatformDisplay::sharedDisplay()).backend(),
        std::max(1, initialSize.width()), std::max(1, initialSize.height()));
}

AcceleratedSurface::RenderTargetWPEBackend::~RenderTargetWPEBackend()
{
#if WPE_CHECK_VERSION(1, 9, 1)
    // libwpe 1.9.1 introduced an additional ::deinitialize function, which
    // may be called some time before destruction. As there is no better place
    // to invoke it at the moment, do it right before destroying the object.
    wpe_renderer_backend_egl_target_deinitialize(m_backend);
#endif

    wpe_renderer_backend_egl_target_destroy(m_backend);
}

uint64_t AcceleratedSurface::RenderTargetWPEBackend::window() const
{
    // EGLNativeWindowType changes depending on the EGL implementation: reinterpret_cast works
    // for pointers (only if they are 64-bit wide and not for other cases), and static_cast for
    // numeric types (and when needed they get extended to 64-bit) but not for pointers. Using
    // a plain C cast expression in this one instance works in all cases.
    static_assert(sizeof(EGLNativeWindowType) <= sizeof(uint64_t), "EGLNativeWindowType must not be longer than 64 bits.");
    return (uint64_t)wpe_renderer_backend_egl_target_get_native_window(m_backend);
}

void AcceleratedSurface::RenderTargetWPEBackend::resize(const IntSize& size)
{
    wpe_renderer_backend_egl_target_resize(m_backend, std::max(1, size.width()), std::max(1, size.height()));
}

void AcceleratedSurface::RenderTargetWPEBackend::willRenderFrame()
{
    wpe_renderer_backend_egl_target_frame_will_render(m_backend);
}

void AcceleratedSurface::RenderTargetWPEBackend::didRenderFrame(Vector<IntRect, 1>&&)
{
    wpe_renderer_backend_egl_target_frame_rendered(m_backend);
}
#endif

AcceleratedSurface::SwapChain::SwapChain()
{
    auto& display = PlatformDisplay::sharedDisplay();
    switch (display.type()) {
#if USE(WPE_RENDERER)
    case PlatformDisplay::Type::WPE:
        m_type = Type::WPEBackend;
        break;
#endif
#if PLATFORM(PLAYSTATION)
    case PlatformDisplay::Type::Surfaceless:
        break;
#endif // PLATFORM(PLAYSTATION)
    }
}

bool AcceleratedSurface::SwapChain::resize(const IntSize& size)
{
    if (m_size == size)
        return false;

    m_size = size;
#if USE(WPE_RENDERER)
    if (m_type == Type::WPEBackend) {
        static_cast<RenderTargetWPEBackend*>(m_lockedTargets[0].get())->resize(m_size);
        return true;
    }
#endif
    reset();
    return true;
}

std::unique_ptr<AcceleratedSurface::RenderTarget> AcceleratedSurface::SwapChain::createTarget() const
{
    switch (m_type) {
#if USE(WPE_RENDERER)
    case Type::WPEBackend:
        ASSERT_NOT_REACHED();
        break;
#endif
    case Type::Invalid:
        break;
    }
    return nullptr;
}

AcceleratedSurface::RenderTarget* AcceleratedSurface::SwapChain::nextTarget()
{
#if USE(WPE_RENDERER)
    if (m_type == Type::WPEBackend)
        return m_lockedTargets[0].get();
#endif

    if (m_freeTargets.isEmpty()) {
        ASSERT(m_lockedTargets.size() < s_maximumBuffers);

        if (m_lockedTargets.isEmpty()) [[unlikely]] {
            // Initial setup.
            for (unsigned i = 0; i < s_initialBuffers; ++i)
                m_freeTargets.append(createTarget());
        } else {
            // Additional buffers creted on demand.
            m_lockedTargets.insert(0, createTarget());
            return m_lockedTargets[0].get();
        }
    }

    auto target = m_freeTargets.takeLast();
    m_lockedTargets.insert(0, WTF::move(target));
    return m_lockedTargets[0].get();
}

void AcceleratedSurface::SwapChain::releaseTarget(uint64_t targetID, UnixFileDescriptor&& releaseFence)
{
#if USE(WPE_RENDERER)
    ASSERT(m_type != Type::WPEBackend);
#endif

    auto index = m_lockedTargets.reverseFindIf([targetID](const auto& item) {
        return item->id() == targetID;
    });
    if (index != notFound) {
        m_lockedTargets[index]->setReleaseFenceFD(WTF::move(releaseFence));
        m_freeTargets.insert(0, WTF::move(m_lockedTargets[index]));
        m_lockedTargets.removeAt(index);
    }
}

void AcceleratedSurface::SwapChain::reset()
{
    m_lockedTargets.clear();
    m_freeTargets.clear();
}

void AcceleratedSurface::SwapChain::releaseUnusedBuffers()
{
#if USE(WPE_RENDERER)
    ASSERT(m_type != Type::WPEBackend);
#endif

    m_freeTargets.clear();
}

#if USE(WPE_RENDERER)
void AcceleratedSurface::SwapChain::initialize(WebPage& webPage)
{
    ASSERT(m_type == Type::WPEBackend);
    m_hostFD = webPage.hostFileDescriptor();
    m_initialSize = webPage.size();
    m_initialSize.scale(webPage.deviceScaleFactor());
}

uint64_t AcceleratedSurface::SwapChain::initializeTarget(const AcceleratedSurface& surface)
{
    ASSERT(m_type == Type::WPEBackend);
    auto target = RenderTargetWPEBackend::create(m_initialSize, WTF::move(m_hostFD), surface);
    auto window = static_cast<RenderTargetWPEBackend*>(target.get())->window();
    m_lockedTargets.append(WTF::move(target));
    return window;
}
#endif

void AcceleratedSurface::visibilityDidChange(bool isVisible)
{
    if (m_isVisible == isVisible)
        return;

    m_isVisible = isVisible;
    if (!m_releaseUnusedBuffersTimer)
        return;

    if (m_isVisible)
        m_releaseUnusedBuffersTimer->stop();
    else {
        static const Seconds releaseUnusedBuffersDelay = 10_s;
        m_releaseUnusedBuffersTimer->startOneShot(releaseUnusedBuffersDelay);
    }
}

void AcceleratedSurface::backgroundColorDidChange()
{
    ASSERT(RunLoop::isMain());
    const auto& color = m_webPage->backgroundColor();

    bool wasOpaque = isColorOpaque(m_backgroundColor);
    m_backgroundColor = color ? color->toResolvedColorComponentsInColorSpace(WebCore::ColorSpace::SRGB) : white;
    bool isOpaque = isColorOpaque(m_backgroundColor);

    if (isOpaque == wasOpaque)
        return;
}

void AcceleratedSurface::releaseUnusedBuffersTimerFired()
{
    m_swapChain.releaseUnusedBuffers();
}

void AcceleratedSurface::didCreateCompositingRunLoop(RunLoop& runLoop)
{
#if USE(WPE_RENDERER)
    if (m_swapChain.type() == SwapChain::Type::WPEBackend)
        return;
#endif

    m_releaseUnusedBuffersTimer = makeUnique<RunLoop::Timer>(runLoop, "AcceleratedSurface::ReleaseUnusedBuffersTimer"_s, this, &AcceleratedSurface::releaseUnusedBuffersTimerFired);
#if USE(GLIB_EVENT_LOOP)
    m_releaseUnusedBuffersTimer->setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#endif
}

void AcceleratedSurface::willDestroyCompositingRunLoop()
{
    m_frameCompleteHandler = nullptr;

#if USE(WPE_RENDERER)
    if (m_swapChain.type() == SwapChain::Type::WPEBackend)
        return;
#endif

    m_releaseUnusedBuffersTimer = nullptr;
}

void AcceleratedSurface::willDestroyGLContext()
{
    m_swapChain.reset();
}

uint64_t AcceleratedSurface::surfaceID() const
{
    return m_id;
}

uint64_t AcceleratedSurface::window() const
{
#if USE(WPE_RENDERER)
    if (m_swapChain.type() == SwapChain::Type::WPEBackend)
        return const_cast<SwapChain*>(&m_swapChain)->initializeTarget(*this);
#endif
    return 0;
}

void AcceleratedSurface::willRenderFrame(const IntSize& size)
{
    bool sizeDidChange = m_swapChain.resize(size);

    m_target = m_swapChain.nextTarget();
    if (m_target)
        m_target->willRenderFrame();

    if (sizeDidChange)
        glViewport(0, 0, size.width(), size.height());
}

void AcceleratedSurface::clear(const OptionSet<WebCore::CompositionReason>& reasons)
{
    ASSERT(!RunLoop::isMain());
    auto backgroundColor = m_backgroundColor.load();
    if (!isColorOpaque(backgroundColor)) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    } else if (reasons.contains(CompositionReason::AsyncScrolling)) {
        auto [r, g, b, a] = backgroundColor;
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

void AcceleratedSurface::didRenderFrame()
{
    if (!m_target)
        return;

    m_target->sync(m_useExplicitSync);

    Vector<IntRect, 1> damageRects;
    m_target->didRenderFrame(WTF::move(damageRects));
}

void AcceleratedSurface::frameDone()
{
    if (m_frameCompleteHandler)
        m_frameCompleteHandler();
    m_target = nullptr;
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
