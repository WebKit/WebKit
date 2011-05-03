/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "qdeclarativewebview_p.h"

#include <QtDeclarative/qdeclarative.h>
#include <QtDeclarative/qdeclarativeextensionplugin.h>

QT_BEGIN_NAMESPACE

class WebKitQmlPlugin : public QDeclarativeExtensionPlugin {
    Q_OBJECT
public:
    virtual void registerTypes(const char* uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtWebKit"));
        qmlRegisterType<QDeclarativeWebSettings>();
        qmlRegisterType<QDeclarativeWebView>(uri, 1, 0, "WebView");
#ifdef Q_REVISION
        qmlRegisterType<QDeclarativeWebView>(uri, 1, 1, "WebView");
        qmlRegisterRevision<QDeclarativeWebView, 0>("QtWebKit", 1, 0);
        qmlRegisterRevision<QDeclarativeWebView, 1>("QtWebKit", 1, 1);
#endif
    }
};

QT_END_NAMESPACE

#include "plugin.moc"

Q_EXPORT_PLUGIN2(qmlwebkitplugin, QT_PREPEND_NAMESPACE(WebKitQmlPlugin));

