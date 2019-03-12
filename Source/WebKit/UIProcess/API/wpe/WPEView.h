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

#pragma once

#include "APIObject.h"
#include "PageClientImpl.h"
#include "WebPageProxy.h"
#include <WebCore/ActivityState.h>
#include <memory>
#include <wtf/OptionSet.h>
#include <wtf/RefPtr.h>

typedef struct OpaqueJSContext* JSGlobalContextRef;
struct wpe_view_backend;

namespace API {
class ViewClient;
}

namespace WebKit {
class DownloadProxy;
class WebPageGroup;
class WebProcessPool;
}

namespace WKWPE {

class View : public API::ObjectImpl<API::Object::Type::View> {
public:
    static View* create(struct wpe_view_backend* backend, const API::PageConfiguration& configuration)
    {
        return new View(backend, configuration);
    }

    // Client methods
    void setClient(std::unique_ptr<API::ViewClient>&&);
    void frameDisplayed();
    void handleDownloadRequest(WebKit::DownloadProxy&);
    void willStartLoad();

    WebKit::WebPageProxy& page() { return *m_pageProxy; }

    struct wpe_view_backend* backend() { return m_backend; }

    const WebCore::IntSize& size() const { return m_size; }

    OptionSet<WebCore::ActivityState::Flag> viewState() const { return m_viewStateFlags; }

    void close();

#if ENABLE(FULLSCREEN_API)
    bool isFullScreen() { return m_fullScreenModeActive; };
    void setFullScreen(bool fullScreenState) { m_fullScreenModeActive = fullScreenState; };
#endif

private:
    View(struct wpe_view_backend*, const API::PageConfiguration&);

    void setSize(const WebCore::IntSize&);
    void setViewState(OptionSet<WebCore::ActivityState::Flag>);

    std::unique_ptr<API::ViewClient> m_client;

    std::unique_ptr<WebKit::PageClientImpl> m_pageClient;
    RefPtr<WebKit::WebPageProxy> m_pageProxy;
    WebCore::IntSize m_size;
    OptionSet<WebCore::ActivityState::Flag> m_viewStateFlags;

    struct wpe_view_backend* m_backend;

#if ENABLE(FULLSCREEN_API)
    bool m_fullScreenModeActive { false };
#endif
};

} // namespace WKWPE
