/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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
#include "UIGamepadProvider.h"

#if ENABLE(GAMEPAD)

#include "WebKitWebViewBasePrivate.h"
#include "WebPageProxy.h"
#include <WebCore/GtkUtilities.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/glib/GRefPtr.h>

namespace WebKit {

using namespace WebCore;

static WebPageProxy* getWebPageProxy(GtkWidget* widget)
{
    if (!widget || !GTK_IS_CONTAINER(widget))
        return nullptr;

    if (WEBKIT_IS_WEB_VIEW_BASE(widget))
        return gtk_widget_is_visible(widget) ? webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(widget)) : nullptr;

    GUniquePtr<GList> children(gtk_container_get_children(GTK_CONTAINER(widget)));
    for (GList* iter = children.get(); iter; iter= g_list_next(iter)) {
        if (WebPageProxy* proxy = getWebPageProxy(GTK_WIDGET(iter->data)))
            return proxy;
    }
    return nullptr;
}

WebPageProxy* UIGamepadProvider::platformWebPageProxyForGamepadInput()
{
    GUniquePtr<GList> toplevels(gtk_window_list_toplevels());
    for (GList* iter = toplevels.get(); iter; iter = g_list_next(iter)) {
        if (!WebCore::widgetIsOnscreenToplevelWindow(GTK_WIDGET(iter->data)))
            continue;

        GtkWindow* window = GTK_WINDOW(iter->data);
        if (!gtk_window_has_toplevel_focus(window))
            continue;

        if (WebPageProxy* proxy = getWebPageProxy(GTK_WIDGET(window)))
            return proxy;
    }
    return nullptr;
}

}

#endif // ENABLE(GAMEPAD)
