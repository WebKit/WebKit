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


#ifndef QtWebIconDatabaseClient_h
#define QtWebIconDatabaseClient_h

#include "WKIconDatabase.h"
#include "qwebkitglobal.h"
#include <QtCore/QObject>
#include <QtCore/QSize>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

QT_BEGIN_NAMESPACE
class QImage;
class QUrl;
QT_END_NAMESPACE

namespace WTF {
class String;
}

namespace WebKit {

class QtWebContext;
class WebIconDatabase;

class QtWebIconDatabaseClient : public QObject {
    Q_OBJECT

public:
    QtWebIconDatabaseClient(QtWebContext*);
    ~QtWebIconDatabaseClient();

    QImage iconImageForPageURL(const WTF::String& pageURL, const QSize& iconSize = QSize(32, 32));
    void retainIconForPageURL(const WTF::String&);
    void releaseIconForPageURL(const WTF::String&);

public Q_SLOTS:
    void requestIconForPageURL(const QUrl&);

public:
    Q_SIGNAL void iconChangedForPageURL(const QUrl& pageURL, const QUrl& iconURL);

private:
    unsigned iconURLHashForPageURL(const WTF::String&);
    static void didChangeIconForPageURL(WKIconDatabaseRef, WKURLRef pageURL, const void* clientInfo);
    uint64_t m_contextId;
    RefPtr<WebKit::WebIconDatabase> m_iconDatabase;
    Mutex m_imageLock;
};

} // namespace WebKit

#endif
