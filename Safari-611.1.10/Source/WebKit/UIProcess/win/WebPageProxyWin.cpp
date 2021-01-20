/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include "WebPageProxy.h"

#include "NativeWebKeyboardEvent.h"
#include "PageClientImpl.h"
#include <WebCore/SearchPopupMenuDB.h>
#include <WebCore/UserAgent.h>

#if USE(DIRECT2D)
#include <d3d11_1.h>
#endif

namespace WebKit {

void WebPageProxy::platformInitialize()
{
}

String WebPageProxy::userAgentForURL(const URL&)
{
    return userAgent();
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    return WebCore::standardUserAgent(applicationNameForUserAgent);
}

void WebPageProxy::saveRecentSearches(const String& name, const Vector<WebCore::RecentSearch>& searchItems)
{
    if (!name)
        return;

    return WebCore::SearchPopupMenuDB::singleton().saveRecentSearches(name, searchItems);
}

void WebPageProxy::loadRecentSearches(const String& name, CompletionHandler<void(Vector<WebCore::RecentSearch>&&)>&& completionHandler)
{
    if (!name)
        return completionHandler({ });

    Vector<WebCore::RecentSearch> searchItems;
    WebCore::SearchPopupMenuDB::singleton().loadRecentSearches(name, searchItems);
    completionHandler(WTFMove(searchItems));
}

void WebPageProxy::updateEditorState(const EditorState& editorState)
{
    m_editorState = editorState;
}

PlatformViewWidget WebPageProxy::viewWidget()
{
    return static_cast<PageClientImpl&>(pageClient()).viewWidget();
}

#if USE(DIRECT2D)
ID3D11Device1* WebPageProxy::device() const
{
    return m_device.get();
}

void WebPageProxy::setDevice(ID3D11Device1* device)
{
    m_device = device;
}
#endif

void WebPageProxy::dispatchPendingCharEvents(const NativeWebKeyboardEvent& keydownEvent)
{
    auto& pendingCharEvents = keydownEvent.pendingCharEvents();
    for (auto it = pendingCharEvents.rbegin(); it != pendingCharEvents.rend(); it++)
        m_keyEventQueue.prepend(NativeWebKeyboardEvent(it->hwnd, it->message, it->wParam, it->lParam, { }));
}

} // namespace WebKit
