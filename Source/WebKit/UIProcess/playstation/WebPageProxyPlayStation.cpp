/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#include "MessageSenderInlines.h"
#include "PageClient.h"
#include "WebAccessibilityObject.h"
#include "WebFrameProxy.h"
#include "WebPageMessages.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/SearchPopupMenu.h>
#include <WebCore/UserAgent.h>

namespace WebKit {

void WebPageProxy::platformInitialize()
{
    notImplemented();
}

struct wpe_view_backend* WebPageProxy::viewBackend()
{
    return nullptr;
}

String WebPageProxy::userAgentForURL(const URL&)
{
    return userAgent();
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    return WebCore::standardUserAgent(applicationNameForUserAgent);
}

void WebPageProxy::saveRecentSearches(const String&, const Vector<WebCore::RecentSearch>&)
{
    notImplemented();
}

void WebPageProxy::loadRecentSearches(const String&, CompletionHandler<void(Vector<WebCore::RecentSearch>&&)>&& completionHandler)
{
    notImplemented();
    completionHandler({ });
}

void WebPageProxy::didUpdateEditorState(const EditorState&, const EditorState&)
{
}

void WebPageProxy::accessibilityNotification(const WebAccessibilityObjectData& data, uint32_t notification)
{
    std::optional<WebCore::FrameIdentifier> frameId = data.webFrameID();
    if (!frameId)
        return;

    WebFrameProxy* frame = WebFrameProxy::webFrame(*frameId);
    if (!frame || frame->frameLoadState().state() != FrameLoadState::State::Finished)
        return;

    RefPtr<WebAccessibilityObject> webAXObject = WebAccessibilityObject::create(data, frame);
    pageClient().handleAccessibilityNotification(webAXObject.get(), static_cast<WebCore::AXObjectCache::AXNotification>(notification));
}

void WebPageProxy::accessibilityTextChange(const WebAccessibilityObjectData& data, uint32_t textChange, uint32_t offset, const String& text)
{
    std::optional<WebCore::FrameIdentifier> frameId = data.webFrameID();
    if (!frameId)
        return;

    WebFrameProxy* frame = WebFrameProxy::webFrame(*frameId);
    if (!frame || frame->frameLoadState().state() != FrameLoadState::State::Finished)
        return;

    RefPtr<WebAccessibilityObject> webAXObject = WebAccessibilityObject::create(data, frame);
    pageClient().handleAccessibilityTextChange(webAXObject.get(), static_cast<WebCore::AXTextChange>(textChange), offset, text);
}

void WebPageProxy::accessibilityLoadingEvent(const WebAccessibilityObjectData& data, uint32_t loadingEvent)
{
    std::optional<WebCore::FrameIdentifier> frameId = data.webFrameID();
    if (!frameId)
        return;

    WebFrameProxy* frame = WebFrameProxy::webFrame(*frameId);
    if (!frame)
        return;

    RefPtr<WebAccessibilityObject> webAXObject = WebAccessibilityObject::create(data, frame);
    pageClient().handleAccessibilityLoadingEvent(webAXObject.get(), static_cast<WebCore::AXObjectCache::AXLoadingEvent>(loadingEvent));
}

void WebPageProxy::accessibilityRootObject()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::AccessibilityRootObject());
}

void WebPageProxy::didReceiveAccessibilityRootObject(const WebAccessibilityObjectData& data)
{
    std::optional<WebCore::FrameIdentifier> frameId = data.webFrameID();
    if (!frameId)
        return;

    WebFrameProxy* frame = WebFrameProxy::webFrame(*frameId);
    if (!frame)
        return;

    RefPtr<WebAccessibilityObject> webAXFocusedObject = WebAccessibilityObject::create(data, frame);
    pageClient().handleAccessibilityRootObject(&webAXFocusedObject.releaseNonNull().leakRef());
}

void WebPageProxy::accessibilityFocusedObject()
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::AccessibilityFocusedObject());
}

void WebPageProxy::didReceiveAccessibilityFocusedObject(const WebAccessibilityObjectData& data)
{
    std::optional<WebCore::FrameIdentifier> frameId = data.webFrameID();
    if (!frameId)
        return;

    WebFrameProxy* frame = WebFrameProxy::webFrame(*frameId);
    if (!frame)
        return;

    RefPtr<WebAccessibilityObject> webAXFocusedObject = WebAccessibilityObject::create(data, frame);
    pageClient().handleAccessibilityFocusedObject(&webAXFocusedObject.releaseNonNull().leakRef());
}

void WebPageProxy::accessibilityHitTest(const WebCore::IntPoint& point)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::AccessibilityHitTest(point));
}

void WebPageProxy::didReceiveAccessibilityHitTest(const WebAccessibilityObjectData& data)
{
    std::optional<WebCore::FrameIdentifier> frameId = data.webFrameID();
    if (!frameId)
        return;

    WebFrameProxy* frame = WebFrameProxy::webFrame(*frameId);
    if (!frame)
        return;

    RefPtr<WebAccessibilityObject> webAXFocusedObject = WebAccessibilityObject::create(data, frame);
    pageClient().handleAccessibilityHitTest(&webAXFocusedObject.releaseNonNull().leakRef());
}

} // namespace WebKit
