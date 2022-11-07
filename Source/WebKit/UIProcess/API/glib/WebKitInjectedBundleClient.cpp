/*
 * Copyright (C) 2013, 2017 Igalia S.L.
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
#include "WebKitInjectedBundleClient.h"

#include "APIInjectedBundleClient.h"
#include "WebImage.h"
#include "WebKitPrivate.h"
#include "WebKitURIRequestPrivate.h"
#include "WebKitURIResponsePrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebKitWebResourcePrivate.h"
#include "WebKitWebViewPrivate.h"
#include <wtf/glib/GUniquePtr.h>

using namespace WebKit;
using namespace WebCore;

class WebKitInjectedBundleClient final : public API::InjectedBundleClient {
public:
    explicit WebKitInjectedBundleClient(WebKitWebContext* webContext)
        : m_webContext(webContext)
    {
    }

private:
    RefPtr<API::Object> getInjectedBundleInitializationUserData(WebProcessPool&) override
    {
        GRefPtr<GVariant> data = webkitWebContextInitializeWebExtensions(m_webContext);
        GUniquePtr<gchar> dataString(g_variant_print(data.get(), TRUE));
        return API::String::create(String::fromUTF8(dataString.get()));
    }

    WebKitWebContext* m_webContext;
};

void attachInjectedBundleClientToContext(WebKitWebContext* webContext)
{
    webkitWebContextGetProcessPool(webContext).setInjectedBundleClient(makeUnique<WebKitInjectedBundleClient>(webContext));
}
