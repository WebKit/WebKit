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

#ifndef qwebnavigationcontroller_h
#define qwebnavigationcontroller_h


#include "qwebkitglobal.h"
#include <QtCore/QObject>

QT_BEGIN_NAMESPACE
class QAction;
QT_END_NAMESPACE

class QtWebPageProxy;
class QWebNavigationControllerPrivate;

class QWEBKIT_EXPORT QWebNavigationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY navigationStateChanged FINAL)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY navigationStateChanged FINAL)
    Q_PROPERTY(bool canStop READ canStop NOTIFY navigationStateChanged FINAL)
    Q_PROPERTY(bool canReload READ canReload NOTIFY navigationStateChanged FINAL)
public:
    QWebNavigationController(QtWebPageProxy*);
    ~QWebNavigationController();

    bool canGoBack() const;
    bool canGoForward() const;
    bool canStop() const;
    bool canReload() const;

public slots:
    void goBack();
    void goForward();
    void stop();
    void reload();

Q_SIGNALS:
    void navigationStateChanged();

private:
    QWebNavigationControllerPrivate* d;
};

#endif // qwebnavigationcontroller_h
