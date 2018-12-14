/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "WPEView.h"

#include "APIPageConfiguration.h"
#include "APIViewClient.h"
#include "DrawingAreaProxy.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebTouchEvent.h"
#include "NativeWebWheelEvent.h"
#include "WebPageGroup.h"
#include "WebProcessPool.h"
#include <wpe/wpe.h>

using namespace WebKit;

namespace WKWPE {

View::View(struct wpe_view_backend* backend, const API::PageConfiguration& baseConfiguration)
    : m_client(std::make_unique<API::ViewClient>())
    , m_pageClient(std::make_unique<PageClientImpl>(*this))
    , m_size { 800, 600 }
#if !defined(WPE_BACKEND_CHECK_VERSION) || !WPE_BACKEND_CHECK_VERSION(1, 1, 0)
    , m_viewStateFlags { WebCore::ActivityState::WindowIsActive, WebCore::ActivityState::IsFocused, WebCore::ActivityState::IsVisible, WebCore::ActivityState::IsInWindow }
#endif
    , m_compositingManagerProxy(*this)
    , m_backend(backend)
{
    ASSERT(m_backend);

    auto configuration = baseConfiguration.copy();
    auto* preferences = configuration->preferences();
    if (!preferences && configuration->pageGroup()) {
        preferences = &configuration->pageGroup()->preferences();
        configuration->setPreferences(preferences);
    }
    if (preferences) {
        preferences->setAcceleratedCompositingEnabled(true);
        preferences->setForceCompositingMode(true);
        preferences->setAccelerated2dCanvasEnabled(true);
        preferences->setWebGLEnabled(true);
        preferences->setDeveloperExtrasEnabled(true);
    }

    auto* pool = configuration->processPool();
    m_pageProxy = pool->createWebPage(*m_pageClient, WTFMove(configuration));

#if ENABLE(MEMORY_SAMPLER)
    if (getenv("WEBKIT_SAMPLE_MEMORY"))
        pool->startMemorySampler(0);
#endif

    m_compositingManagerProxy.initialize();

    static struct wpe_view_backend_client s_backendClient = {
        // set_size
        [](void* data, uint32_t width, uint32_t height)
        {
            auto& view = *reinterpret_cast<View*>(data);
            view.setSize(WebCore::IntSize(width, height));
        },
        // frame_displayed
        [](void* data)
        {
            auto& view = *reinterpret_cast<View*>(data);
            view.frameDisplayed();
        },
#if defined(WPE_BACKEND_CHECK_VERSION) && WPE_BACKEND_CHECK_VERSION(1, 1, 0)
        // activity_state_changed
        [](void* data, uint32_t state)
        {
            auto& view = *reinterpret_cast<View*>(data);
            OptionSet<WebCore::ActivityState::Flag> flags;
            if (state & wpe_view_activity_state_visible)
                flags.add(WebCore::ActivityState::IsVisible);
            if (state & wpe_view_activity_state_focused) {
                flags.add(WebCore::ActivityState::IsFocused);
                flags.add(WebCore::ActivityState::WindowIsActive);
            }
            if (state & wpe_view_activity_state_in_window)
                flags.add(WebCore::ActivityState::IsInWindow);
            view.setViewState(flags);
        },
#else
        nullptr,
#endif
        // padding
        nullptr,
        nullptr,
        nullptr
    };
    wpe_view_backend_set_backend_client(m_backend, &s_backendClient, this);

    static struct wpe_view_backend_input_client s_inputClient = {
        // handle_keyboard_event
        [](void* data, struct wpe_input_keyboard_event* event)
        {
            auto& view = *reinterpret_cast<View*>(data);
            if (event->pressed
                && event->modifiers & wpe_input_keyboard_modifier_control
                && event->modifiers & wpe_input_keyboard_modifier_shift
                && event->key_code == WPE_KEY_G) {
                auto& preferences = view.page().preferences();
                preferences.setResourceUsageOverlayVisible(!preferences.resourceUsageOverlayVisible());
                return;
            }
            view.page().handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(event));
        },
        // handle_pointer_event
        [](void* data, struct wpe_input_pointer_event* event)
        {
            auto& page = reinterpret_cast<View*>(data)->page();
            page.handleMouseEvent(WebKit::NativeWebMouseEvent(event, page.deviceScaleFactor()));
        },
        // handle_axis_event
        [](void* data, struct wpe_input_axis_event* event)
        {
            auto& page = reinterpret_cast<View*>(data)->page();
            page.handleWheelEvent(WebKit::NativeWebWheelEvent(event, page.deviceScaleFactor()));
        },
        // handle_touch_event
        [](void* data, struct wpe_input_touch_event* event)
        {
            auto& page = reinterpret_cast<View*>(data)->page();
            page.handleTouchEvent(WebKit::NativeWebTouchEvent(event, page.deviceScaleFactor()));
        },
        // padding
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };
    wpe_view_backend_set_input_client(m_backend, &s_inputClient, this);

    wpe_view_backend_initialize(m_backend);

    m_pageProxy->initializeWebPage();
}

View::~View()
{
    m_compositingManagerProxy.finalize();
}

void View::setClient(std::unique_ptr<API::ViewClient>&& client)
{
    if (!client)
        m_client = std::make_unique<API::ViewClient>();
    else
        m_client = WTFMove(client);
}

void View::frameDisplayed()
{
    m_client->frameDisplayed(*this);
}

void View::handleDownloadRequest(DownloadProxy& download)
{
    m_client->handleDownloadRequest(*this, download);
}

void View::setSize(const WebCore::IntSize& size)
{
    m_size = size;
    if (m_pageProxy->drawingArea())
        m_pageProxy->drawingArea()->setSize(size);
}

void View::setViewState(OptionSet<WebCore::ActivityState::Flag> flags)
{
    auto changedFlags = m_viewStateFlags ^ flags;
    m_viewStateFlags = flags;

    if (changedFlags)
        m_pageProxy->activityStateDidChange(changedFlags);
}

void View::close()
{
    m_pageProxy->close();
}

} // namespace WKWPE
