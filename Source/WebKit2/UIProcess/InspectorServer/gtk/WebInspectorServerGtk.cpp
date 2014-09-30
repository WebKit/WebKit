/*
 * Copyright (C) 2012 Samsung Electronics Ltd. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(INSPECTOR_SERVER)
#include "WebInspectorServer.h"

#include "WebInspectorProxy.h"
#include "WebPageProxy.h"
#include <WebCore/FileSystem.h>
#include <WebCore/MIMETypeRegistry.h>
#include <gio/gio.h>
#include <glib.h>
#include <wtf/gobject/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenate.h>

namespace WebKit {

bool WebInspectorServer::platformResourceForPath(const String& path, Vector<char>& data, String& contentType)
{
    // The page list contains an unformated list of pages that can be inspected with a link to open a session.
    if (path == "/pagelist.json" || path == "/json") {
        buildPageList(data, contentType);
        return true;
    }

    // Point the default path to a formatted page that queries the page list and display them.
    CString resourcePath = makeString("/org/webkitgtk/inspector/UserInterface", (path == "/" ? "/inspectorPageIndex.html" : path)).utf8();
    if (resourcePath.isNull())
        return false;

    GUniqueOutPtr<GError> error;
    GRefPtr<GBytes> resourceBytes = adoptGRef(g_resources_lookup_data(resourcePath.data(), G_RESOURCE_LOOKUP_FLAGS_NONE, &error.outPtr()));
    if (!resourceBytes) {
        StringBuilder builder;
        builder.appendLiteral("<!DOCTYPE html><html><head></head><body>Error: ");
        builder.appendNumber(error->code);
        builder.appendLiteral(", ");
        builder.append(error->message);
        builder.appendLiteral(" occurred during fetching inspector resource files.</body></html>");

        CString errorHTML = builder.toString().utf8();
        data.append(errorHTML.data(), errorHTML.length());
        contentType = "text/html; charset=utf-8";

        WTFLogAlways("Error fetching webinspector resource files: %d, %s", error->code, error->message);
        return false;
    }

    gsize resourceDataSize;
    gconstpointer resourceData = g_bytes_get_data(resourceBytes.get(), &resourceDataSize);
    data.append(static_cast<const char*>(resourceData), resourceDataSize);

    GUniquePtr<gchar> mimeType(g_content_type_guess(resourcePath.data(), static_cast<const guchar*>(resourceData), resourceDataSize, nullptr));
    contentType = mimeType.get();
    return true;
}

void WebInspectorServer::buildPageList(Vector<char>& data, String& contentType)
{
    // chromedevtools (http://code.google.com/p/chromedevtools) 0.3.8 expected JSON format:
    // {
    //  "title": "Foo",
    //  "url": "http://foo",
    //  "devtoolsFrontendUrl": "/Main.html?ws=localhost:9222/devtools/page/1",
    //  "webSocketDebuggerUrl": "ws://localhost:9222/devtools/page/1"
    // },

    StringBuilder builder;
    builder.appendLiteral("[ ");
    ClientMap::iterator end = m_clientMap.end();
    for (ClientMap::iterator it = m_clientMap.begin(); it != end; ++it) {
        WebPageProxy* webPage = it->value->page();
        if (it != m_clientMap.begin())
            builder.appendLiteral(", ");
        builder.appendLiteral("{ \"id\": ");
        builder.appendNumber(it->key);
        builder.appendLiteral(", \"title\": \"");
        builder.append(webPage->pageLoadState().title());
        builder.appendLiteral("\", \"url\": \"");
        builder.append(webPage->pageLoadState().activeURL());
        builder.appendLiteral("\", \"inspectorUrl\": \"");
        builder.appendLiteral("/Main.html?page=");
        builder.appendNumber(it->key);
        builder.appendLiteral("\", \"devtoolsFrontendUrl\": \"");
        builder.appendLiteral("/Main.html?ws=");
        builder.append(bindAddress());
        builder.appendLiteral(":");
        builder.appendNumber(port());
        builder.appendLiteral("/devtools/page/");
        builder.appendNumber(it->key);
        builder.appendLiteral("\", \"webSocketDebuggerUrl\": \"");
        builder.appendLiteral("ws://");
        builder.append(bindAddress());
        builder.appendLiteral(":");
        builder.appendNumber(port());
        builder.appendLiteral("/devtools/page/");
        builder.appendNumber(it->key);
        builder.appendLiteral("\" }");
    }
    builder.appendLiteral(" ]");
    CString cstr = builder.toString().utf8();
    data.append(cstr.data(), cstr.length());
    contentType = "application/json; charset=utf-8";
}

}
#endif
