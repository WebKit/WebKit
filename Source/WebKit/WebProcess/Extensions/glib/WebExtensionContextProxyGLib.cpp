/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebExtensionContextProxy.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include <WebCore/NotImplemented.h>

namespace WebKit {

void WebExtensionContextProxy::dispatchActionClickedEvent(const std::optional<WebExtensionTabParameters>&)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchCommandsCommandEvent(const String&, const std::optional<WebExtensionTabParameters>&)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchCommandsChangedEvent(const String&, const String&, const String&)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchCookiesChangedEvent()
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchMenusClickedEvent(const WebExtensionMenuItemParameters&, bool, const WebExtensionMenuItemContextParameters&, const std::optional<WebExtensionTabParameters>&)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchPermissionsEvent(WebExtensionEventListenerType, HashSet<String>, HashSet<String>)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchPortMessageEvent(std::optional<WebPageProxyIdentifier>, WebExtensionPortChannelIdentifier, const String&, bool)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchPortDisconnectEvent(WebExtensionPortChannelIdentifier)
{
    notImplemented();
}

void WebExtensionContextProxy::internalDispatchRuntimeMessageEvent(WebExtensionContentWorldType, const String&, const std::optional<WebExtensionMessageTargetParameters>&, const WebExtensionMessageSenderParameters&, bool, CompletionHandler<void(String&& replyJSON)>&& completionHandler)
{
    notImplemented();
    completionHandler({ });
}

void WebExtensionContextProxy::dispatchTabsCreatedEvent(const WebExtensionTabParameters&)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchTabsUpdatedEvent(const WebExtensionTabParameters&, const WebExtensionTabParameters&)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchTabsReplacedEvent(WebExtensionTabIdentifier, WebExtensionTabIdentifier)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchTabsDetachedEvent(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, uint64_t)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchTabsMovedEvent(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, uint64_t, uint64_t)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchTabsAttachedEvent(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, uint64_t)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchTabsActivatedEvent(WebExtensionTabIdentifier, WebExtensionTabIdentifier, WebExtensionWindowIdentifier)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchTabsHighlightedEvent(const Vector<WebExtensionTabIdentifier>&, WebExtensionWindowIdentifier)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchTabsRemovedEvent(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, WebExtensionContext::WindowIsClosing)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchWebNavigationEvent(WebExtensionEventListenerType, WebExtensionTabIdentifier, const WebExtensionFrameParameters&, WallTime)
{
    notImplemented();
}

void WebExtensionContextProxy::resourceLoadDidSendRequest(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::ResourceRequest&, const ResourceLoadInfo&, const std::optional<IPC::FormDataReference>&)
{
    notImplemented();
}

void WebExtensionContextProxy::resourceLoadDidBlockBeforeRequest(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const ResourceLoadInfo&)
{
    notImplemented();
}

void WebExtensionContextProxy::resourceLoadDidPerformHTTPRedirection(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::ResourceResponse&, const ResourceLoadInfo&, const WebCore::ResourceRequest&)
{
    notImplemented();
}

void WebExtensionContextProxy::resourceLoadDidReceiveChallenge(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::AuthenticationChallenge&, const ResourceLoadInfo&)
{
    notImplemented();
}

void WebExtensionContextProxy::resourceLoadDidReceiveResponse(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::ResourceResponse&, const ResourceLoadInfo&)
{
    notImplemented();
}

void WebExtensionContextProxy::resourceLoadDidCompleteWithError(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::ResourceResponse&, const WebCore::ResourceError&, const ResourceLoadInfo&)
{
    notImplemented();
}

void WebExtensionContextProxy::dispatchWindowsEvent(WebExtensionEventListenerType, const std::optional<WebExtensionWindowParameters>&)
{
    notImplemented();
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
