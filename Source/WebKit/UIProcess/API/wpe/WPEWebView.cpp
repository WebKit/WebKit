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
#include "WPEWebView.h"

#include "APIPageConfiguration.h"
#include "APIViewClient.h"
#include "DrawingAreaProxy.h"
#include "EditingRange.h"
#include "EditorState.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include <WebCore/CompositionUnderline.h>

using namespace WebKit;

namespace WKWPE {

View::View()
    : m_client(makeUnique<API::ViewClient>())
    , m_pageClient(makeUniqueWithoutRefCountedCheck<PageClientImpl>(*this))
{
}

View::~View()
{
    m_pageProxy->close();
}

void View::createWebPage(const API::PageConfiguration& configuration)
{
    auto& pool = configuration.processPool();
    m_pageProxy = pool.createWebPage(*m_pageClient, configuration.copy());

#if ENABLE(MEMORY_SAMPLER)
    if (getenv("WEBKIT_SAMPLE_MEMORY"))
        pool.startMemorySampler(0);
#endif
}

void View::setClient(std::unique_ptr<API::ViewClient>&& client)
{
    if (!client)
        m_client = makeUnique<API::ViewClient>();
    else
        m_client = WTFMove(client);
}

void View::frameDisplayed()
{
    m_client->frameDisplayed(*this);
}

void View::willStartLoad()
{
    m_client->willStartLoad(*this);
}

void View::didChangePageID()
{
    m_client->didChangePageID(*this);
}

void View::didReceiveUserMessage(UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    m_client->didReceiveUserMessage(*this, WTFMove(message), WTFMove(completionHandler));
}

WebKitWebResourceLoadManager* View::webResourceLoadManager()
{
    return m_client->webResourceLoadManager();
}

void View::setInputMethodContext(WebKitInputMethodContext* context)
{
    m_inputMethodFilter.setContext(context);
}

WebKitInputMethodContext* View::inputMethodContext() const
{
    return m_inputMethodFilter.context();
}

void View::setInputMethodState(std::optional<InputMethodState>&& state)
{
    m_inputMethodFilter.setState(WTFMove(state));
}

void View::selectionDidChange()
{
    const auto& editorState = m_pageProxy->editorState();
    if (editorState.hasPostLayoutAndVisualData()) {
        m_inputMethodFilter.notifyCursorRect(editorState.visualData->caretRectAtStart);
        m_inputMethodFilter.notifySurrounding(editorState.postLayoutData->surroundingContext, editorState.postLayoutData->surroundingContextCursorPosition,
            editorState.postLayoutData->surroundingContextSelectionPosition);
    }
}

void View::themeColorDidChange()
{
    m_client->themeColorDidChange();
}

void View::setSize(const WebCore::IntSize& size)
{
    m_size = size;
    if (m_pageProxy->drawingArea())
        m_pageProxy->drawingArea()->setSize(size);
}

void View::close()
{
    m_pageProxy->close();
}

#if ENABLE(FULLSCREEN_API)
bool View::isFullScreen() const
{
    return m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen || m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::InFullscreen;
}

void View::willEnterFullScreen(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::NotInFullscreen);
    completionHandler(true);
    m_fullscreenState = WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen;
}

void View::willExitFullScreen(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::EnteringFullscreen || m_fullscreenState == WebFullScreenManagerProxy::FullscreenState::InFullscreen);

    completionHandler();
    m_fullscreenState = WebFullScreenManagerProxy::FullscreenState::ExitingFullscreen;
}
#endif // ENABLE(FULLSCREEN_API)

} // namespace WKWPE
