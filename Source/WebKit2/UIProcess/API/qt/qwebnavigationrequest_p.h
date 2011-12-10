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

#ifndef qwebnavigationrequest_p_h
#define qwebnavigationrequest_p_h

#include "qwebkitglobal.h"

#include <QtCore/QObject>
#include <QtCore/QUrl>

class QWebNavigationRequestPrivate;

class QWEBKIT_EXPORT QWebNavigationRequest : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl url READ url CONSTANT FINAL)
    Q_PROPERTY(int button READ button CONSTANT FINAL)
    Q_PROPERTY(int modifiers READ modifiers CONSTANT FINAL)
    Q_PROPERTY(int action READ action WRITE setAction NOTIFY actionChanged FINAL)

public:
    QWebNavigationRequest(const QUrl& url, Qt::MouseButton button, Qt::KeyboardModifiers modifiers, QObject* parent = 0);
    ~QWebNavigationRequest();

    QUrl url() const;
    int button() const;
    int modifiers() const;
    int action() const;

    void setAction(int action);

Q_SIGNALS:
    void actionChanged();

private:
    QWebNavigationRequestPrivate* d;
};

#endif // qwebnavigationrequest_h
