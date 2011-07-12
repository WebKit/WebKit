/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef qdesktopwebview_p_h
#define qdesktopwebview_p_h

#include "qdesktopwebpageproxy.h"
#include "ViewInterface.h"

class QDesktopWebView;

class QDesktopWebViewPrivate : public WebKit::ViewInterface
{
public:
    QDesktopWebViewPrivate(QDesktopWebView*, WKContextRef = 0, WKPageGroupRef = 0);

    static QDesktopWebView* createViewWithPageGroup(WKPageGroupRef);

    QDesktopWebView* q;
    QDesktopWebPageProxy page;

    bool isCrashed;

private:
    /* Implementation of ViewInterface */
    virtual void setViewNeedsDisplay(const QRect&);

    virtual QSize drawingAreaSize();
    virtual void contentSizeChanged(const QSize&);

    virtual bool isActive();
    virtual bool hasFocus();
    virtual bool isVisible();

    virtual void startDrag(Qt::DropActions supportedDropActions, const QImage& dragImage, QMimeData*, QPoint* clientPosition, QPoint* globalPosition, Qt::DropAction* dropAction);

    virtual void didChangeUrl(const QUrl&);
    virtual void didChangeTitle(const QString&);
    virtual void didChangeToolTip(const QString&);
    virtual void didChangeStatusText(const QString&);
    virtual void didChangeCursor(const QCursor&);
    virtual void loadDidBegin();
    virtual void loadDidSucceed();
    virtual void loadDidFail(const QWebError&);
    virtual void didChangeLoadProgress(int);

    virtual void showContextMenu(QSharedPointer<QMenu>);
    virtual void hideContextMenu();

    virtual void processDidCrash();
    virtual void didRelaunchProcess();

    QSharedPointer<QMenu> activeMenu;
};

#endif /* qdesktopwebview_p_h */
