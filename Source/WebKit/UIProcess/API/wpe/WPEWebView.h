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
#include "InputMethodFilter.h"
#include "KeyAutoRepeatHandler.h"
#include "PageClientImpl.h"
#include "WebFullScreenManagerProxy.h"
#include "WebPageProxy.h"
#include <WebCore/ActivityState.h>
#include <memory>
#include <wtf/OptionSet.h>
#include <wtf/RefPtr.h>

typedef struct _WebKitInputMethodContext WebKitInputMethodContext;
typedef struct _WPEView WPEView;
struct wpe_view_backend;

namespace API {
class ViewClient;
}

namespace WebCore {
class Cursor;
struct CompositionUnderline;
}

namespace WebKit {
class WebKitWebResourceLoadManager;
struct EditingRange;
struct UserMessage;
}

namespace WKWPE {

class View : public API::ObjectImpl<API::Object::Type::View> {
public:
    virtual ~View();

    // Client methods
    void setClient(std::unique_ptr<API::ViewClient>&&);
    void frameDisplayed();
    void willStartLoad();
    void didChangePageID();
    void didReceiveUserMessage(WebKit::UserMessage&&, CompletionHandler<void(WebKit::UserMessage&&)>&&);
    WebKit::WebKitWebResourceLoadManager* webResourceLoadManager();

    void setInputMethodContext(WebKitInputMethodContext*);
    WebKitInputMethodContext* inputMethodContext() const;
    void setInputMethodState(std::optional<WebKit::InputMethodState>&&);

    void themeColorDidChange();

#if ENABLE(FULLSCREEN_API)
    bool isFullScreen() const;
    void willEnterFullScreen(CompletionHandler<void(bool)>&&);
    void willExitFullScreen(CompletionHandler<void()>&&);
#endif

    void selectionDidChange();
    void close();

    WebKit::WebPageProxy& page() { return *m_pageProxy; }
    API::ViewClient& client() const { return *m_client; }
    const WebCore::IntSize& size() const { return m_size; }
    OptionSet<WebCore::ActivityState> viewState() const { return m_viewStateFlags; }

    virtual struct wpe_view_backend* backend() const { return nullptr; }
#if ENABLE(WPE_PLATFORM)
    virtual WPEView* wpeView() const { return nullptr; }
#endif
#if ENABLE(POINTER_LOCK)
    virtual void requestPointerLock() { };
    virtual void didLosePointerLock() { };
#endif

    virtual void setCursor(const WebCore::Cursor&) { };
    virtual void synthesizeCompositionKeyPress(const String&, std::optional<Vector<WebCore::CompositionUnderline>>&&, std::optional<WebKit::EditingRange>&&) = 0;
    virtual void callAfterNextPresentationUpdate(CompletionHandler<void()>&&) = 0;

protected:
    View();

    void createWebPage(const API::PageConfiguration&);
    void setSize(const WebCore::IntSize&);

    std::unique_ptr<API::ViewClient> m_client;
    const std::unique_ptr<WebKit::PageClientImpl> m_pageClient;
    RefPtr<WebKit::WebPageProxy> m_pageProxy;
    WebCore::IntSize m_size;
    OptionSet<WebCore::ActivityState> m_viewStateFlags;
#if ENABLE(FULLSCREEN_API)
    WebKit::WebFullScreenManagerProxy::FullscreenState m_fullscreenState { WebKit::WebFullScreenManagerProxy::FullscreenState::NotInFullscreen };
#endif
    WebKit::InputMethodFilter m_inputMethodFilter;
    WebKit::KeyAutoRepeatHandler m_keyAutoRepeatHandler;
};

} // namespace WKWPE

SPECIALIZE_TYPE_TRAITS_BEGIN(WKWPE::View)
static bool isType(const API::Object& object) { return object.type() == API::Object::Type::View; }
SPECIALIZE_TYPE_TRAITS_END()
