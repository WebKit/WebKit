/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

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

#include "qdesktopwebview.h"
#include "qtouchwebpage.h"
#include "qtouchwebview.h"

#include <QtDeclarative/qdeclarative.h>
#include <QtDeclarative/qdeclarativeextensionplugin.h>

QT_BEGIN_NAMESPACE

class WebKit2QmlPlugin : public QDeclarativeExtensionPlugin {
    Q_OBJECT
public:
    virtual void registerTypes(const char* uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtWebKit.experimental"));
        qmlRegisterType<QDesktopWebView>(uri, 5, 0, "DesktopWebView");
        qmlRegisterType<QTouchWebView>(uri, 5, 0, "TouchWebView");
        qmlRegisterUncreatableType<QTouchWebPage>(uri, 5, 0, "TouchWebPage", QObject::tr("Cannot create separate instance of TouchWebPage, use TouchWebView"));
    }
};

QT_END_NAMESPACE

#include "plugin.moc"

Q_EXPORT_PLUGIN2(webkit2qmlplugin, QT_PREPEND_NAMESPACE(WebKit2QmlPlugin));

