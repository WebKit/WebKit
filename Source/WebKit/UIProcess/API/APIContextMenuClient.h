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

#if ENABLE(CONTEXT_MENUS)

#include "WKBase.h"
#include "WebContextMenuItem.h"
#include "WebContextMenuListenerProxy.h"
#include "WebHitTestResultData.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

OBJC_CLASS NSMenu;

namespace WebCore {
class IntPoint;
}

namespace WebKit {
class WebContextMenuItemData;
class WebPageProxy;
}

namespace API {

class ContextMenuClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~ContextMenuClient() { }

    virtual void getContextMenuFromProposedMenu(WebKit::WebPageProxy&, Vector<Ref<WebKit::WebContextMenuItem>>&& proposedMenu, WebKit::WebContextMenuListenerProxy& listener, const WebKit::WebHitTestResultData&, API::Object* /* userData */) { listener.useContextMenuItems(WTFMove(proposedMenu)); }
    virtual void customContextMenuItemSelected(WebKit::WebPageProxy&, const WebKit::WebContextMenuItemData&) { }
    virtual void showContextMenu(WebKit::WebPageProxy&, const WebCore::IntPoint&, const Vector<Ref<WebKit::WebContextMenuItem>>&) { }
    virtual bool canShowContextMenu() const { return false; }
    virtual bool hideContextMenu(WebKit::WebPageProxy&) { return false; }

#if PLATFORM(MAC)
    virtual void menuFromProposedMenu(WebKit::WebPageProxy&, NSMenu *menu, const WebKit::WebHitTestResultData&, API::Object*, CompletionHandler<void(RetainPtr<NSMenu>&&)>&& completionHandler) { completionHandler(menu); }
#endif
};

} // namespace API

#endif // ENABLE(CONTEXT_MENUS)
