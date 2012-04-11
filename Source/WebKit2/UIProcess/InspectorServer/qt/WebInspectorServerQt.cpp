/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */


#include "config.h"

#include "WebInspectorServer.h"

#include "WebInspectorProxy.h"
#include "WebPageProxy.h"
#include <WebCore/MIMETypeRegistry.h>
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
    String localPath = (path == "/") ? "/webkit/resources/inspectorPageIndex.html" : path;
    // All other paths are mapped directly to a resource, if possible.
    QFile file(QString::fromLatin1(":%1").arg(localPath));
    if (file.exists()) {
        file.open(QIODevice::ReadOnly);
        data.grow(file.size());
        file.read(data.data(), data.size());

        size_t extStart = localPath.reverseFind('.');
        String ext = localPath.substring(extStart != notFound ? extStart + 1 : 0);
        contentType = WebCore::MIMETypeRegistry::getMIMETypeForExtension(ext);
        return true;
    }
    return false;
}

void WebInspectorServer::buildPageList(Vector<char>& data, String& contentType)
{
    StringBuilder builder;
    builder.append("[ ");
    ClientMap::iterator end = m_clientMap.end();
    for (ClientMap::iterator it = m_clientMap.begin(); it != end; ++it) {
        WebPageProxy* webPage = it->second->page();
        if (it != m_clientMap.begin())
            builder.append(", ");
        builder.append("{ \"id\": " + String::number(it->first));
        builder.append(", \"title\": \"");
        builder.append(webPage->pageTitle());
        builder.append("\", \"url\": \"");
        builder.append(webPage->activeURL());
        builder.append("\", \"inspectorUrl\": \"");
        builder.append("/webkit/inspector/inspector.html?page=" + String::number(it->first));
        builder.append("\" }");
    }
    builder.append(" ]");
    CString cstr = builder.toString().utf8();
    data.append(cstr.data(), cstr.length());
    contentType = "application/json; charset=utf-8";
}

}
