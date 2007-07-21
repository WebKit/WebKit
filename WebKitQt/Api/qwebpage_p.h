/*
    Copyright (C) 2007 Trolltech ASA

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#ifndef QWEBPAGE_P_H
#define QWEBPAGE_P_H

#include <qnetworkproxy.h>
#include <qpointer.h>

#include "qwebpage.h"
#include "qwebframe.h"

namespace WebCore
{
    class ChromeClientQt;
    class ContextMenuClientQt;
    class EditorClientQt;
    class Page;
}

class QUndoStack;

class QWebPagePrivate
{
public:
    QWebPagePrivate(QWebPage *);
    ~QWebPagePrivate();
    void createMainFrame();

    WebCore::ChromeClientQt *chromeClient;
    WebCore::ContextMenuClientQt *contextMenuClient;
    WebCore::EditorClientQt *editorClient;
    WebCore::Page *page;

    QPointer<QWebFrame> mainFrame;

    QWebPage *q;
    QUndoStack *undoStack;

    QWebNetworkInterface *networkInterface;

    bool modified;

    bool insideOpenCall;
    QWebPage::NavigationRequestResponse navigationRequested(QWebFrame *frame, const QWebNetworkRequest &request, QWebPage::NavigationType type);
#ifndef QT_NO_NETWORKPROXY
    QNetworkProxy networkProxy;
#endif
};

#endif
