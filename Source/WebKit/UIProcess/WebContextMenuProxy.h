/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "ContextMenuContextData.h"
#include "UserData.h"
#include "WebContextMenuListenerProxy.h"
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(COCOA)
#include "QuickLookPreviewActivity.h"
#endif

OBJC_CLASS NSMenu;

namespace WebKit {

class WebContextMenuItem;
class WebPageProxy;

class WebContextMenuProxy : public RefCounted<WebContextMenuProxy>, public WebContextMenuListenerProxy::Client {
public:
    virtual ~WebContextMenuProxy();

    virtual void show();

    WebPageProxy* page() const { return m_page.get(); }

#if PLATFORM(COCOA)
    virtual NSMenu *platformMenu() const = 0;
    virtual NSArray *platformData() const = 0;
    virtual QuickLookPreviewActivity quickLookPreviewActivity() const { return QuickLookPreviewActivity::None; };
#endif // PLATFORM(COCOA)

protected:
    WebContextMenuProxy(WebPageProxy&, ContextMenuContextData&&, const UserData&);

    // WebContextMenuListenerProxy::Client
    void useContextMenuItems(Vector<Ref<WebContextMenuItem>>&&) override;

    const ContextMenuContextData m_context;
    const UserData m_userData;

private:
    virtual Vector<Ref<WebContextMenuItem>> proposedItems() const;
    virtual void showContextMenuWithItems(Vector<Ref<WebContextMenuItem>>&&) = 0;

    RefPtr<WebContextMenuListenerProxy> m_contextMenuListener;
    WeakPtr<WebPageProxy> m_page;
};

} // namespace WebKit

#endif
