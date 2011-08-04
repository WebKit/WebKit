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
#include "qwebkittypes.h"
#include <QtCore/QObject>

QT_BEGIN_NAMESPACE
class QAction;
QT_END_NAMESPACE

class QtWebPageProxy;
class QWebNavigationControllerPrivate;

class QWEBKIT_EXPORT QWebNavigationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAction* backAction READ backAction CONSTANT)
    Q_PROPERTY(QAction* forwardAction READ forwardAction CONSTANT)
    Q_PROPERTY(QAction* stopAction READ stopAction CONSTANT)
    Q_PROPERTY(QAction* reloadAction READ reloadAction CONSTANT)
public:
    QWebNavigationController(QtWebPageProxy*);
    ~QWebNavigationController();

    QAction* backAction() const;
    QAction* forwardAction() const;
    QAction* stopAction() const;
    QAction* reloadAction() const;

    QAction* navigationAction(QtWebKit::NavigationAction which) const;

public slots:
    void back();
    void forward();
    void stop();
    void reload();

private:
    QWebNavigationControllerPrivate* d;
};

#endif // qwebnavigationcontroller_h
