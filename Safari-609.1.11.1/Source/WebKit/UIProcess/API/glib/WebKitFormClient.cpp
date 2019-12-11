/*
 * Copyright (C) 2012 Igalia S.L.
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
#include "WebKitFormClient.h"

#include "APIFormClient.h"
#include "WebFormSubmissionListenerProxy.h"
#include "WebKitFormSubmissionRequestPrivate.h"
#include "WebKitWebViewPrivate.h"
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

class FormClient final : public API::FormClient {
public:
    explicit FormClient(WebKitWebView* webView)
        : m_webView(webView)
    {
    }

private:
    void willSubmitForm(WebPageProxy&, WebFrameProxy&, WebFrameProxy&, const Vector<std::pair<String, String>>& values, API::Object*, WTF::Function<void(void)>&& completionHandler) override
    {
        GRefPtr<WebKitFormSubmissionRequest> request = adoptGRef(webkitFormSubmissionRequestCreate(values, WebFormSubmissionListenerProxy::create(WTFMove(completionHandler))));
        webkitWebViewSubmitFormRequest(m_webView, request.get());
    }

    WebKitWebView* m_webView;
};

void attachFormClientToView(WebKitWebView* webView)
{
    webkitWebViewGetPage(webView).setFormClient(makeUnique<FormClient>(webView));
}
