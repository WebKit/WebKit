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
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

bool WebInspectorServer::platformResourceForPath(const String& path, Vector<char>& data, String& contentType)
{
    // The page list contains an unformated list of pages that can be inspected with a link to open a session.
    if (path == "/pagelist.json") {
        buildPageList(data, contentType);
        return true;
    }

    // Point the default path to a formatted page that queries the page list and display them.
    CString localPath = WebCore::fileSystemRepresentation(String(WebCore::sharedResourcesPath().data()) + ((path == "/") ? "/webinspector/inspectorPageIndex.html" : path));
    if (localPath.isNull())
        return false;

    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(localPath.data()));
    GRefPtr<GFileInfo> fileInfo = adoptGRef(g_file_query_info(file.get(), G_FILE_ATTRIBUTE_STANDARD_SIZE","G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, 0, 0));
    if (!fileInfo)
        return false;

    GRefPtr<GFileInputStream> inputStream = adoptGRef(g_file_read(file.get(), 0, 0));
    if (!inputStream)
        return false;

    data.grow(g_file_info_get_size(fileInfo.get()));
    if (!g_input_stream_read_all(G_INPUT_STREAM(inputStream.get()), data.data(), data.size(), 0, 0, 0))
        return false;

    contentType = GOwnPtr<gchar>(g_file_info_get_attribute_as_string(fileInfo.get(), G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE)).get();
    return true;
}

void WebInspectorServer::buildPageList(Vector<char>& data, String& contentType)
{
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
        builder.append(webPage->pageTitle());
        builder.appendLiteral("\", \"url\": \"");
        builder.append(webPage->activeURL());
        builder.appendLiteral("\", \"inspectorUrl\": \"");
        builder.appendLiteral("/webinspector/inspector.html?page=");
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
